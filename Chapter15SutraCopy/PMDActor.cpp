#include "PMDActor.h"
#include "Dx12Wrapper.h"
#include <sstream>
#include <array>

using namespace DirectX;
using namespace Microsoft::WRL;

#pragma comment(lib, "winmm.lib")

namespace
{
	std::pair<std::string, std::string> SplitFileName(const std::string& path, const char splitter = '*')
	{
		size_t idx = path.find(splitter);
		std::pair<std::string, std::string> ret;
		ret.first = path.substr(0, idx);
		ret.second = path.substr(idx + 1, path.length() - idx - 1);
		return ret;
	}

	std::string GetTexturePathFromModelAndTexPath(const std::string& modelPath, const char* texPath)
	{
		// フォルダ区切りが/でも\でも対応できるようにする。
		// rfindは見つからなかったらepos(-1、0xffffffff)を返す。
		int pathIndex1 = (int)modelPath.rfind('/');
		int pathIndex2 = (int)modelPath.rfind('\\');
		int pathIndex = max(pathIndex1, pathIndex2);
		const std::string& folderPath = modelPath.substr(0, pathIndex + 1);
		return folderPath + texPath;
	}

	// 現在のXYZ座標軸をlookatの方向をZ'軸にするような回転行列を求める。
	// つまり、現在のZ軸をlookatの方向に回転させるような回転行列を求める。
	// それだけだとX'軸、Y'軸の方向が定まらないので、up、right方向を渡しておき、それに基づいて
	// X'軸、Y'軸を決める
	XMMATRIX LookAtMatrix(const XMVECTOR& lookat, const XMFLOAT3& up, const XMFLOAT3& right)
	{
		XMVECTOR vz = XMVector3Normalize(lookat);
		XMVECTOR vy = XMVector3Normalize(XMLoadFloat3(&up));

		XMVECTOR vx;
		// lookatとupが並行なら、right基準で決める
		// そうでなければup基準で決める
		if (abs(XMVector3Dot(vy, vz).m128_f32[0]) == 1.0f)
		{
			vx = XMVector3Normalize(XMLoadFloat3(&right));
			vy = XMVector3Normalize(XMVector3Cross(vz, vx));
			vx = XMVector3Normalize(XMVector3Cross(vy, vz));
		}
		else
		{
			vx = XMVector3Normalize(XMVector3Cross(vy, vz));
			vy = XMVector3Normalize(XMVector3Cross(vz, vx));
		}

		XMMATRIX ret = XMMatrixIdentity(); // 平行移動部分は0にしておく
		ret.r[0] = vx;
		ret.r[1] = vy;
		ret.r[2] = vz;
		return ret;
	}

	// originの方向をlookatの方向に向かせるような回転行列を求める
	// 残り2つの軸の方向を決める基準とするためにup、rightを渡す。
	XMMATRIX LookAtMatrix(const XMVECTOR& origin, const XMVECTOR& lookat, const XMFLOAT3& up, const XMFLOAT3& right)
	{
		return
			// originを現在のZ軸の方向に向かせるような回転
			XMMatrixTranspose(LookAtMatrix(origin, up, right))
			// 現在のZ軸をlookatの方向に向かせるような回転
			* LookAtMatrix(lookat, up, right);
	}

	float GetYFromXOnBezier(float x, const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b, uint8_t n = 12)
	{
		// 直線のときはy=x
		if (a.x == a.y && b.x == b.y)
		{
			return x;
		}

		// 筆者開発の半分刻み法で与えられたxのときのx(t) = xを満たすtを求める

		float t = x;
		const float k0 = 1 + 3 * a.x - 3 * b.x;
		const float k1 = 3 * b.x - 6 * a.x;
		const float k2 = 3 * a.x;

		// 終了判定に用いる誤差上限値
		constexpr float epsilon = 0.0005f;
		for (int i = 0; i < n; ++i)
		{
			float ft = k0 * t * t * t + k1 * t * t + k2 * t - x;
			// 誤差上限値内におさまったので先に終了
			if (ft <= epsilon && ft >= -epsilon)
			{
				break;
			}

			t -= ft * 0.5f;
		}

		// 求まった近似解tからy(t)を求める
		float r = 1 - t;
		// TODO:なぜこちらは1-tを利用する式で、上は利用しない式にしているのか？
		return t * t * t + 3 * t * r * r * a.y + 3 * t * t * r * b.y;
	}

	enum class BoneType : uint32_t
	{
		Rotation,
		RotAndMove,
		IK,
		Undefined,
		IKChild,
		RotationChild,
		IKDestination,
		Invisible,
	};
} // namespace

PMDActor::PMDActor(Dx12Wrapper& dx12, const std::string& modelPath)
: _dx12(dx12)
{
	HRESULT result = LoadPMDFileAndCreateMeshBuffers(modelPath);
	if (FAILED(result))
	{
		assert(false);
		return;
	}

	result = CreateTransformConstantBuffer();
	if (FAILED(result))
	{
		assert(false);
		return;
	}

	result = CreateMaterialBuffers();
	if (FAILED(result))
	{
		assert(false);
		return;
	}

	return;
}

HRESULT PMDActor::LoadPMDFileAndCreateMeshBuffers(const std::string& path)
{
	// PMDヘッダ格納データ
	struct PMDHeader
	{
		float version;
		char model_name[20];
		char comment[256];
	};

	char signature[3];
	PMDHeader pmdheader = {};
	FILE* fp = nullptr;
	errno_t error = fopen_s(&fp, path.c_str(), "rb");
	fread(signature, sizeof(signature), 1, fp);
	fread(&pmdheader, sizeof(pmdheader), 1, fp);

	unsigned int vertNum;
	fread(&vertNum, sizeof(vertNum), 1, fp);

#pragma pack(1)
	// PMDマテリアルデータ読み出し用
	// PMDはフォンシェーディングの模様
	struct PMDMaterial
	{
		XMFLOAT3 diffuse;
		float alpha;
		float specularity;
		XMFLOAT3 specular;
		XMFLOAT3 ambient;
		unsigned char toonIdx;
		unsigned char edgeFlg;
		// 本来ここでこの構造体は2バイトのパディングが発生する
		unsigned int indicesNum;
		char texFilePath[20];
	}; // pack(1)がなければ70バイトのはずが72バイトになる
#pragma pack()

#pragma pack(1)
	struct PMDVertex {
		XMFLOAT3 pos; // 頂点 座標
		XMFLOAT3 normal; // 法線 ベクトル
		XMFLOAT2 uv; // uv 座標
		unsigned short boneNo[2]; // ボーン 番号
		unsigned char boneWeight; // ボーン 影響度
		unsigned char edgeFlg; // 輪郭 線 フラグ
	}; // pack(1)がなければ38バイトのはずが40バイトになる
#pragma pack()

	constexpr unsigned int pmdvertex_size = sizeof(PMDVertex);
	std::vector<unsigned char> vertices(vertNum * pmdvertex_size);
	fread(vertices.data(), vertices.size(), 1, fp);

	fread(&_indexNum, sizeof(_indexNum), 1, fp);

	HRESULT result = _dx12.Device()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(vertices.size()),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_vertBuff.ReleaseAndGetAddressOf())
	);
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	// 頂点バッファへのデータ書き込み
	unsigned char* vertMap = nullptr;
	result = _vertBuff->Map(0, nullptr, (void**)&vertMap);
	if (FAILED(result))
	{
		assert(false);
		return result;
	}
	std::copy(vertices.begin(), vertices.end(), vertMap);
	_vertBuff->Unmap(0, nullptr);

	// 頂点バッファービューの用意
	_vbView.BufferLocation = _vertBuff->GetGPUVirtualAddress();
	_vbView.SizeInBytes = (UINT)vertices.size();
	_vbView.StrideInBytes = pmdvertex_size;

	std::vector<unsigned short> indices(_indexNum);
	fread(indices.data(), indices.size() * sizeof(indices[0]), 1, fp);

	result = _dx12.Device()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(indices.size() * sizeof(indices[0])),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_idxBuff.ReleaseAndGetAddressOf())
	);
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	// インデックスバッファへのデータ書き込み
	unsigned short* idxMap = nullptr;
	result = _idxBuff->Map(0, nullptr, (void**)&idxMap);
	if (FAILED(result))
	{
		assert(false);
		return result;
	}
	std::copy(indices.begin(), indices.end(), idxMap);
	_idxBuff->Unmap(0, nullptr);

	// インデックスバッファービューの用意
	_ibView.BufferLocation = _idxBuff->GetGPUVirtualAddress();
	_ibView.Format = DXGI_FORMAT_R16_UINT;
	_ibView.SizeInBytes = (UINT)(indices.size() * sizeof(indices[0]));

	// マテリアル情報の読み出し
	unsigned int materialNum;
	fread(&materialNum, sizeof(materialNum), 1, fp);

	_materials.resize(materialNum);
	_textureResources.resize(materialNum);
	_sphResources.resize(materialNum);
	_spaResources.resize(materialNum);
	_toonResources.resize(materialNum);

	{
		std::vector<PMDMaterial> pmdMaterials(materialNum);
		fread(pmdMaterials.data(), pmdMaterials.size() * sizeof(PMDMaterial), 1, fp);

		for (int i = 0; i < pmdMaterials.size(); ++i)
		{
			_materials[i].indicesNum = pmdMaterials[i].indicesNum;
			_materials[i].material.diffuse = pmdMaterials[i].diffuse;
			_materials[i].material.alpha = pmdMaterials[i].alpha;
			_materials[i].material.specular = pmdMaterials[i].specular;
			_materials[i].material.specularity = pmdMaterials[i].specularity;
			_materials[i].material.ambient = pmdMaterials[i].ambient;
			_materials[i].additional.toonIdx = pmdMaterials[i].toonIdx;

			_textureResources[i] = nullptr;
			_sphResources[i] = nullptr;
			_spaResources[i] = nullptr;
			_toonResources[i] = nullptr;

			// トゥーンシェーディング用のCLUTテクスチャリソースのロード
			std::string toonFilePath = "toon/";
			char toonFileName[16];
			sprintf_s(toonFileName, 16, "toon%02d.bmp", pmdMaterials[i].toonIdx + 1); // この足し算だと255+1は256で扱われるが、現状toon00.bmpはないためこのままにしておく
			toonFilePath += toonFileName;
			_toonResources[i] = _dx12.LoadTextureFromFile(toonFilePath);

			if (strlen(pmdMaterials[i].texFilePath) > 0)
			{
				// 通常テクスチャ、sph、spaのリソースのロード
				std::string texFileName = pmdMaterials[i].texFilePath;
				std::string sphFileName = "";
				std::string spaFileName = "";

				if (std::count(texFileName.begin(), texFileName.end(), '*') > 0)
				{
					const std::pair<std::string, std::string>& namepair = SplitFileName(texFileName);
					if (Dx12Wrapper::GetExtension(namepair.first) == "sph")
					{
						sphFileName = namepair.first;
						texFileName = namepair.second;
					}
					else if (Dx12Wrapper::GetExtension(namepair.first) == "spa")
					{
						spaFileName = namepair.first;
						texFileName = namepair.second;
					}
					else
					{
						texFileName = namepair.first;
						if (Dx12Wrapper::GetExtension(namepair.second) == "sph")
						{
							sphFileName = namepair.second;
						}
						else if (Dx12Wrapper::GetExtension(namepair.second) == "spa")
						{
							spaFileName = namepair.second;
						}
					}
				}
				else
				{
					if (Dx12Wrapper::GetExtension(texFileName) == "sph")
					{
						sphFileName = texFileName;
						texFileName = "";
					}
					else if (Dx12Wrapper::GetExtension(texFileName) == "spa")
					{
						spaFileName = texFileName;
						texFileName = "";
					}
				}

				if (texFileName.length() > 0)
				{
					const std::string& texFilePath = GetTexturePathFromModelAndTexPath(path, texFileName.c_str());
					_textureResources[i] = _dx12.LoadTextureFromFile(texFilePath);
				}

				if (sphFileName.length() > 0)
				{
					const std::string& sphFilePath = GetTexturePathFromModelAndTexPath(path, sphFileName.c_str());
					_sphResources[i] = _dx12.LoadTextureFromFile(sphFilePath);
				}

				if (spaFileName.length() > 0)
				{
					const std::string& spaFilePath = GetTexturePathFromModelAndTexPath(path, spaFileName.c_str());
					_spaResources[i] = _dx12.LoadTextureFromFile(spaFilePath);
				}
			}

			if (_toonResources[i] == nullptr)
			{
				_toonResources[i] = _dx12.GetGrayGradientTexture();
			}

			if (_textureResources[i] == nullptr)
			{
				_textureResources[i] = _dx12.GetWhiteTexture();
			}

			if (_sphResources[i] == nullptr)
			{
				_sphResources[i] = _dx12.GetWhiteTexture();
			}

			if (_spaResources[i] == nullptr)
			{
				_spaResources[i] = _dx12.GetBlackTexture();
			}
		}
	}

	unsigned short boneNum = 0;
	fread(&boneNum, sizeof(boneNum), 1, fp);

#pragma pack(1)
	// PMDボーンデータ読み出し用
	struct PMDBone
	{
		char boneName[20];
		unsigned short parentNo;
		unsigned short nextNo;
		unsigned char type;
		unsigned short ikBoneNo;
		XMFLOAT3 pos;
	};
#pragma pack()
	std::vector<PMDBone> pmdBones(boneNum);
	fread(pmdBones.data(), sizeof(PMDBone), boneNum, fp);

	uint16_t ikNum = 0;
	fread(&ikNum, sizeof(ikNum), 1, fp);

	_ikData.resize(ikNum);
	for (PMDIK& ik : _ikData)
	{
		fread(&ik.boneIdx, sizeof(ik.boneIdx), 1, fp);
		fread(&ik.targetIdx, sizeof(ik.targetIdx), 1, fp);
		uint8_t chainLen = 0;
		fread(&chainLen, sizeof(chainLen), 1, fp);
		ik.nodeIdxes.resize(chainLen);
		fread(&ik.iterations, sizeof(ik.iterations), 1, fp);
		fread(&ik.limit, sizeof(ik.limit), 1, fp);
		if (chainLen == 0)
		{
			continue;
		}
		fread(ik.nodeIdxes.data(), sizeof(ik.nodeIdxes[0]), chainLen, fp);
	}

	fclose(fp);

	// ファイルにもつデータであれば親インデックスをもつのがデータが小さく
	// なるが、トラバースを考えると逆に子の配列をもつデータ構造の方がよいので
	// データを作り変える

	// 作業用
	std::vector<std::string> boneNames(boneNum);
	_boneNameArray.resize(boneNum);
	_boneNodeAddressArray.resize(boneNum);

	// "ひざ"はPMDではCosineIKで折り曲げる軸をX軸に固定する仕様なので
	// "ひざ"という文字列を含む骨を収集しておく
	_kneeIdxes.clear();

	// ボーンノードマップ作成
	for (unsigned short idx = 0; idx < pmdBones.size(); idx++)
	{
		const PMDBone& pb = pmdBones[idx];
		boneNames[idx] = pb.boneName;

		BoneNode& node = _boneNodeTable[pb.boneName];
		node.boneIdx = idx;
		node.startPos = pb.pos;
		node.boneType = pb.type;
		node.parentBone = pb.parentNo;
		node.ikParentBone = pb.ikBoneNo;

		_boneNameArray[idx] = pb.boneName;
		_boneNodeAddressArray[idx] = &node;

		if (boneNames[idx].find("ひざ") != std::string::npos)
		{
			_kneeIdxes.emplace_back(idx);
		}
	}

	// ボーンノード同士の親子関係の構築
	for (const PMDBone& pb : pmdBones)
	{
		if (pb.parentNo >= pmdBones.size())
		{
			// 親はいない場合はunsigned shortの最大値が入っている
			continue;
		}

		const std::string& parentName = boneNames[pb.parentNo];
		_boneNodeTable[parentName].children.emplace_back(&_boneNodeTable[pb.boneName]);
	}

	_boneMatrices.resize(boneNum);
	std::fill(_boneMatrices.begin(), _boneMatrices.end(), XMMatrixIdentity());

	// IKデバッグコード
	{
		const std::function<std::string(uint16_t)>& getNameFromIdx =
		[&](uint16_t idx)->std::string
		{
			auto it = std::find_if(_boneNodeTable.begin(), _boneNodeTable.end(),
				[idx](const std::pair<std::string, BoneNode>& obj)
				{
					return obj.second.boneIdx == idx;
				}
			);
			if (it == _boneNodeTable.end())
			{
				return "";
			}
			else
			{
				return it->first;
			}
		};

		for (const PMDIK& ik : _ikData)
		{
			std::ostringstream oss;
			oss << "IKボーン番号=" << ik.boneIdx << ":" << getNameFromIdx(ik.boneIdx) << std::endl;
			for (uint16_t nodeIdx : ik.nodeIdxes)
			{
				oss << "\tノードボーン=" << nodeIdx << ":" << getNameFromIdx(nodeIdx) << std::endl;
			}
			OutputDebugString(oss.str().c_str());
		}
	}

	return result;
}

void PMDActor::Move(float x, float y, float z)
{
	_pos.x += x;
	_pos.y += y;
	_pos.z += z;
}

HRESULT PMDActor::LoadVMDFile(const std::string& path)
{
	FILE* fp = nullptr;
	errno_t error = fopen_s(&fp, path.c_str(), "rb");
	fseek(fp, 50, SEEK_SET);

	unsigned int keyframeNum = 0;
	fread(&keyframeNum, sizeof(keyframeNum), 1, fp);

	struct VMDKeyFrame {
		char boneName[15]; // ボーン名
		unsigned int frameNo; // フレーム番号
		XMFLOAT3 location; //位置
		XMFLOAT4 quaternion; // クオータニオン
		unsigned char bezier[64]; // [4][4][4] ベジェ補間パラメータ
	};

	// ボーン名の後にアラインメントでパディングが入るのでループでそれを回避しつつ読む
	std::vector<VMDKeyFrame> keyframes(keyframeNum);
	for (VMDKeyFrame& keyframe : keyframes)
	{
		fread(keyframe.boneName, sizeof(keyframe.boneName), 1, fp);
		fread(&keyframe.frameNo, sizeof(keyframe.frameNo) + sizeof(keyframe.location) + sizeof(keyframe.quaternion) + sizeof(keyframe.bezier), 1, fp);
	}

	fclose(fp);

	_duration = 0;

	// VMDKeyFrameからKeyFrameにつめかえ。ついでに最大フレーム番号のキーフレームをVMDのモーションのフレーム数とする
	for (const VMDKeyFrame& keyframe : keyframes)
	{
		_motiondata[keyframe.boneName].emplace_back(
			keyframe.frameNo,
			XMLoadFloat4(&keyframe.quaternion),
			keyframe.location,
			XMFLOAT2(keyframe.bezier[3] / 127.0f, keyframe.bezier[7] / 127.0f),
			XMFLOAT2(keyframe.bezier[11] / 127.0f, keyframe.bezier[15] / 127.0f)
		);
		_duration = std::max<unsigned int>(_duration, keyframe.frameNo);
	}

	// VMDKeyFrameのキーフレームはフレーム番号順に入ってるとは限らないのでソート
	for (auto& motion : _motiondata) // TODO:std::pairのconst &や&だとコンパイルが通らないのでautoを使う
	{
		std::sort(motion.second.begin(), motion.second.end(),
			[](const KeyFrame& lval, const KeyFrame& rval)
			{
				return lval.frameNo <= rval.frameNo;
			}
		);
	}

	for (const std::pair<std::string, std::vector<KeyFrame>>& bonemotion : _motiondata)
	{
		// VMDにあるキーフレーム情報のボーン名がPMDにあるとは限らないのであるときのみ処理をする
		const auto& itBoneNode = _boneNodeTable.find(bonemotion.first);
		if (itBoneNode == _boneNodeTable.end())
		{
			continue;
		}

		const BoneNode&	node = itBoneNode->second;
		const XMFLOAT3& pos = node.startPos;
		//まずは0フレーム目のポーズのみ使う
		// ここで求めているのはローカル行列である
		_boneMatrices[node.boneIdx] = XMMatrixTranslation(-pos.x, -pos.y, -pos.z) * XMMatrixRotationQuaternion(bonemotion.second[0].quaternion) * XMMatrixTranslation(pos.x, pos.y, pos.z);
	}

	return S_OK;
}

void PMDActor::RecursiveMatrixMultiply(const BoneNode& node, const XMMATRIX& mat)
{
	_boneMatrices[node.boneIdx] *= mat;

	for (const BoneNode* child : node.children)
	{
		assert(child != nullptr);
		RecursiveMatrixMultiply(*child, _boneMatrices[node.boneIdx]);
	}
}

void PMDActor::StartAnimation()
{
	_startTime = timeGetTime();
}

HRESULT PMDActor::CreateTransformConstantBuffer()
{
	// 定数バッファ作成

	size_t buffSize = sizeof(XMMATRIX) * (1 + _boneMatrices.size()); // 1はワールド行列の分
	buffSize = (buffSize + 0xff) & ~0xff;

	HRESULT result = _dx12.Device()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(buffSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_transformBuff.ReleaseAndGetAddressOf())
	);
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	result = _transformBuff->Map(0, nullptr, (void**)&_mappedMatrices);
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	_mappedMatrices[0] = XMMatrixTranslation(_pos.x, _pos.y, _pos.z);

	// ディスクリプタヒープとCBV作成
	D3D12_DESCRIPTOR_HEAP_DESC transformDescHeapDesc = {};
	transformDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	transformDescHeapDesc.NodeMask = 0;
	transformDescHeapDesc.NumDescriptors = 1;
	transformDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	result = _dx12.Device()->CreateDescriptorHeap(&transformDescHeapDesc, IID_PPV_ARGS(_transformDescHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	CD3DX12_CPU_DESCRIPTOR_HANDLE transfomrHeapHandle(_transformDescHeap->GetCPUDescriptorHandleForHeapStart());

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = _transformBuff->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = (UINT)_transformBuff->GetDesc().Width;

	_dx12.Device()->CreateConstantBufferView(
		&cbvDesc,
		transfomrHeapHandle
	);

	return result;
}

HRESULT PMDActor::CreateMaterialBuffers()
{
	// マテリアルバッファを作成
	// sizeof(MaterialForHlsl)の44バイトを256でアラインメントしているので256。
	// かなりもったいない
	// TODO:定数バッファをマテリアル数だけ作っているから、一個にまとめられないか？
	size_t materialNum = _materials.size();
	size_t materialBuffSize = (sizeof(MaterialForHlsl) + 0xff) & ~0xff;
	HRESULT result = _dx12.Device()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(materialBuffSize * materialNum),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_materialBuff.ReleaseAndGetAddressOf())
	);
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	char* mapMaterial = nullptr;
	result = _materialBuff->Map(0, nullptr, (void**)&mapMaterial);
	for (const Material& m : _materials)
	{
		*((MaterialForHlsl*)mapMaterial) = m.material;
		mapMaterial += materialBuffSize;
	}
	_materialBuff->Unmap(0, nullptr);

	// ディスクリプタヒープとCBV作成
	D3D12_DESCRIPTOR_HEAP_DESC materialDescHeapDesc = {};
	materialDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	materialDescHeapDesc.NodeMask = 0;
	materialDescHeapDesc.NumDescriptors = (UINT)(materialNum * 5); // MaterialForHlslのCBVと通常テクスチャとsphとspaとCLUTのSRVの5つずつ
	materialDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	result = _dx12.Device()->CreateDescriptorHeap(&materialDescHeapDesc, IID_PPV_ARGS(_materialDescHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	D3D12_CONSTANT_BUFFER_VIEW_DESC matCBVDesc = {};
	matCBVDesc.BufferLocation = _materialBuff->GetGPUVirtualAddress();
	matCBVDesc.SizeInBytes = (UINT)materialBuffSize;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	// Formatはテクスチャによる

	D3D12_CPU_DESCRIPTOR_HANDLE matDescHeapH = _materialDescHeap->GetCPUDescriptorHandleForHeapStart();
	UINT incSize = _dx12.Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	for (unsigned int i = 0; i < materialNum; ++i)
	{
		_dx12.Device()->CreateConstantBufferView(
			&matCBVDesc,
			matDescHeapH
		);

		matDescHeapH.ptr += incSize;
		matCBVDesc.BufferLocation += materialBuffSize;

		srvDesc.Format = _textureResources[i]->GetDesc().Format;
		_dx12.Device()->CreateShaderResourceView(
			_textureResources[i].Get(),
			&srvDesc,
			matDescHeapH
		);

		matDescHeapH.ptr += incSize;

		srvDesc.Format = _sphResources[i]->GetDesc().Format;
		_dx12.Device()->CreateShaderResourceView(
			_sphResources[i].Get(),
			&srvDesc,
			matDescHeapH
		);

		matDescHeapH.ptr += incSize;

		srvDesc.Format = _spaResources[i]->GetDesc().Format;
		_dx12.Device()->CreateShaderResourceView(
			_spaResources[i].Get(),
			&srvDesc,
			matDescHeapH
		);

		matDescHeapH.ptr += incSize;

		srvDesc.Format = _toonResources[i]->GetDesc().Format;
		_dx12.Device()->CreateShaderResourceView(
			_toonResources[i].Get(),
			&srvDesc,
			matDescHeapH
		);

		matDescHeapH.ptr += incSize;
	}

	return result;
}

void PMDActor::Update()
{
	_mappedMatrices[0] = XMMatrixTranslation(_pos.x, _pos.y, _pos.z);
	UpdateMotion();
}

void PMDActor::UpdateMotion()
{
	DWORD elapsedTime = timeGetTime() - _startTime;
	unsigned int frameNo = (unsigned int)(30 * (elapsedTime / 1000.0f));
	// ループ
	if (frameNo > _duration)
	{
		_startTime = timeGetTime();
		frameNo = 0;
	}

	// 前フレームのポーズをクリア
	std::fill(_boneMatrices.begin(), _boneMatrices.end(), XMMatrixIdentity());

	for (const std::pair<std::string, std::vector<KeyFrame>>& bonemotion : _motiondata)
	{
		const BoneNode& node = _boneNodeTable[bonemotion.first];

		const std::vector<KeyFrame>& keyframes = bonemotion.second;

		auto rit = std::find_if(keyframes.rbegin(), keyframes.rend(),
			[frameNo](const KeyFrame& keyframe) {
				return keyframe.frameNo <= frameNo;
			}
		);

		// 見つからなかった場合
		if (rit == keyframes.rend())
		{
			continue;
		}

		// 次のキーフレーム
		auto it = rit.base();

		XMMATRIX rotation = XMMatrixRotationQuaternion(rit->quaternion);
		XMVECTOR offset = XMLoadFloat3(&rit->offset);
		if (it != keyframes.end())
		{
			// 筆者開発の半分刻み法で、初期値をlerpにとり、ベジエ曲線上の近似値を取得する
			float t = (float)(frameNo - rit->frameNo) / (it->frameNo - rit->frameNo);
			t = GetYFromXOnBezier(t, it->p1, it->p2, 12); // 筆者の経験上、12回程度で収束
			rotation = XMMatrixRotationQuaternion(XMQuaternionSlerp(rit->quaternion, it->quaternion, t));
			offset = XMVectorLerp(offset, XMLoadFloat3(&it->offset), t);
		}

		const XMFLOAT3& pos = node.startPos;
		// キーフレームの情報で回転のみ使用する
		// ここで求めているのはローカル行列である
		_boneMatrices[node.boneIdx] = XMMatrixTranslation(-pos.x, -pos.y, -pos.z) * rotation * XMMatrixTranslation(pos.x, pos.y, pos.z) * XMMatrixTranslationFromVector(offset);
	}

	// センターは動かない前提で単位行列
	// これによって_boneMatrices[]はモデル行列になる
	RecursiveMatrixMultiply(_boneNodeTable["センター"], XMMatrixIdentity());

	//TODO: IKが不正動作を起こすモーションファイルも使いたいのでいったん止める
	//IKSolve();

	std::copy(_boneMatrices.begin(), _boneMatrices.end(), &_mappedMatrices[1]);
}

void PMDActor::SolveLookAt(const struct PMDIK& ik)
{
	assert(ik.nodeIdxes.size() == 1);
	// ik.targetIdxはこの場合は未定義なので使わない

	// ルートでありつつエフェクタでもあるボーン
	uint16_t rootNodeIdx = ik.nodeIdxes[0];
	// LookAtのターゲット位置を指定するボーン
	uint16_t targetNodeIdx = ik.boneIdx;

	const BoneNode* rootNode = _boneNodeAddressArray[rootNodeIdx];
	const BoneNode* targetNode = _boneNodeAddressArray[targetNodeIdx];

	const XMVECTOR& opos1 = XMLoadFloat3(&rootNode->startPos);
	const XMVECTOR& tpos1 = XMLoadFloat3(&targetNode->startPos);

	const XMVECTOR& opos2 = XMVector3Transform(opos1, _boneMatrices[rootNodeIdx]);
	const XMVECTOR& tpos2 = XMVector3Transform(tpos1, _boneMatrices[targetNodeIdx]);

	// バインドポーズ時のエフェクタとターゲット間のベクトル
	XMVECTOR originVec = XMVectorSubtract(tpos1, opos1);
	// アニメーション中のエフェクタとターゲット間のベクトル
	XMVECTOR targetVec = XMVectorSubtract(tpos2, opos2);
	originVec = XMVector3Normalize(originVec);
	targetVec = XMVector3Normalize(targetVec);

	// エフェクタをターゲットに向けるには、バインドポーズ時のベクトルを
	// アニメーションのベクトルに変換するような回転をさせればよい
	_boneMatrices[rootNodeIdx] =
		XMMatrixTranslationFromVector(-opos2)
		* LookAtMatrix(originVec, targetVec, XMFLOAT3(0, 1, 0), XMFLOAT3(1, 0, 0))
		* XMMatrixTranslationFromVector(opos2);
}

void PMDActor::SolveCosineIK(const struct PMDIK& ik)
{
	// IK構成点の位置を保持するワークデータ
	std::vector<XMVECTOR> positions;
	// バインドポーズ時の3点の間の2距離の保持
	std::array<float, 2> edgeLens;

	// ターゲット
	uint16_t targetNodeIdx = ik.boneIdx;
	const BoneNode* targetNode = _boneNodeAddressArray[targetNodeIdx];
	const XMVECTOR& targetPos = XMVector3Transform(XMLoadFloat3(&targetNode->startPos), _boneMatrices[targetNodeIdx]);

	// 末端、エフェクタ
	uint16_t endNodeIdx = ik.targetIdx;
	const BoneNode* endNode = _boneNodeAddressArray[endNodeIdx];

	// positionsにバインドポーズ位置を入れる
	// nodeIdxesは子供から親の順でデータが入っているのに注意
	positions.emplace_back(XMLoadFloat3(&endNode->startPos));

	assert(ik.nodeIdxes.size() == 2);
	for (uint16_t chainBoneIdx : ik.nodeIdxes)
	{
		const BoneNode* boneNode = _boneNodeAddressArray[chainBoneIdx];
		positions.emplace_back(XMLoadFloat3(&boneNode->startPos));
	}
	assert(positions.size() == 3);

	// ルートからになるように逆にする
	std::reverse(positions.begin(), positions.end());

	edgeLens[0] = XMVector3Length(XMVectorSubtract(positions[1], positions[0])).m128_f32[0];
	edgeLens[1] = XMVector3Length(XMVectorSubtract(positions[2], positions[1])).m128_f32[0];

	// positionsにアニメーション位置を入れる
	// nodeIdxesは逆順なのでルートがnodeIdxes[1]なのに注意
	positions[0] = XMVector3Transform(positions[0], _boneMatrices[ik.nodeIdxes[1]]);
	// positions[1]はIK計算で決めるのでアニメーション位置は計算しない
	// エフェクタをターゲットの位置に移動させる。よって、アニメーション位置は_boneMatrices[endNodeIdx]だが_boneMatrices[targetNodeIdx]を使う
	// TODO:これは、本当にターゲット位置に移動させたいなら
	// positions[2] = targetPos;にすべきでは？
	// 多分そうすると、骨の長さが足りなくなるケースを考慮してるのだと思うが
	// 多分、最終行の計算を見てもそうだが、計算方法が僕の知ってるTwoBoneIKと違う
	positions[2] = XMVector3Transform(positions[2], _boneMatrices[targetNodeIdx]);

	// ルートからエフェクタへのベクトル
	const XMVECTOR& linearVec = XMVectorSubtract(positions[2], positions[0]);
	// CosineIK計算をする三角形の各辺の長さ
	float A = XMVector3Length(linearVec).m128_f32[0];
	float B = edgeLens[0];
	float C = edgeLens[1];

	// ルートの角の角度
	float theta1 = acosf((A * A + B * B - C * C) / (2 * A * B));
	// 真ん中の点の角の角度
	float theta2 = acosf((B * B + C * C - A * A) / (2 * B * C));

	// CosineIKで折り曲げる軸を決める
	// PMDのIKは真ん中が膝のときは強制的にX軸にする仕様である
	XMVECTOR axis;
	if (std::find(_kneeIdxes.begin(), _kneeIdxes.end(), ik.nodeIdxes[0]) == _kneeIdxes.end())
	{
		// 真ん中が膝ではないときはルートとエフェクタを結ぶベクトルと
		// ルートとターゲットを結ぶベクトルの2つを含む平面内で回転させるようにする
		const XMVECTOR& vm = XMVector3Normalize(XMVectorSubtract(positions[2], positions[0]));
		const XMVECTOR& vt = XMVector3Normalize(XMVectorSubtract(targetPos, positions[0]));
		axis = XMVector3Cross(vt, vm);
	}
	else
	{
		const XMFLOAT3& right = XMFLOAT3(1, 0, 0);
		axis = XMLoadFloat3(&right);
	}

	// ルート
	_boneMatrices[ik.nodeIdxes[1]] *= XMMatrixTranslationFromVector(-positions[0]) * XMMatrixRotationAxis(axis, theta1) * XMMatrixTranslationFromVector(positions[0]);
	// 真ん中
	// TODO:この乗算って本当にモデル行列の計算になっているのか？
	_boneMatrices[ik.nodeIdxes[0]] = XMMatrixTranslationFromVector(-positions[1]) * XMMatrixRotationAxis(axis, theta2 - XM_PI) * XMMatrixTranslationFromVector(positions[1]) * _boneMatrices[ik.nodeIdxes[1]];
	// エフェクタ
	//TODO:この計算式が不明
	_boneMatrices[endNodeIdx] = _boneMatrices[ik.nodeIdxes[0]];
}

// 収束判定に使用する誤差閾値
constexpr float epsilon = 0.0005f;

void PMDActor::SolveCCDIK(const struct PMDIK& ik)
{
	// ターゲット
	uint16_t targetNodeIdx = ik.boneIdx;
	const BoneNode* targetBoneNode = _boneNodeAddressArray[targetNodeIdx];
	const XMVECTOR& targetOriginPos = XMLoadFloat3(&targetBoneNode->startPos);

	const XMMATRIX& parentMat = _boneMatrices[_boneNodeAddressArray[targetNodeIdx]->ikParentBone];
	XMVECTOR det;
	const XMMATRIX& invParentMat = XMMatrixInverse(&det, parentMat);
	// TODO:いきなりローカルな行列をモデル座標系でのstartPosに乗算してどうなるというのか？
	const XMVECTOR& targetNextPos = XMVector3Transform(targetOriginPos, _boneMatrices[ik.boneIdx] * invParentMat);

	// エフェクタのバインドポーズ位置。
	XMVECTOR endPos = XMLoadFloat3(&_boneNodeAddressArray[ik.targetIdx]->startPos);

	// IK対象のボーンの位置を保持するワークデータ（エフェクタ以外）
	// 子から親の順に入っていることに注意
	std::vector<XMVECTOR> bonePositions;

	// bonePositionsにバインドポーズ位置を入れる
	assert(ik.nodeIdxes.size() >= 3);
	for (uint16_t cidx : ik.nodeIdxes)
	{
		const BoneNode* boneNode = _boneNodeAddressArray[cidx];
		bonePositions.emplace_back(XMLoadFloat3(&boneNode->startPos));
	}
	assert(bonePositions.size() >= 3);
	//TODO:なぜ本ではCCDIKの試行の初期値をバインドポーズではじめるのか？
	// アニメーション位置からはじめた方が変化が最小になり、自然になるのでは？

	// 回転行列の計算結果保持
	std::vector<XMMATRIX> mats(bonePositions.size());
	// 単位行列で初期化しておく
	std::fill(mats.begin(), mats.end(), XMMatrixIdentity());

	// 1フレームで回転させる角度の上限値
	float ikLimit = ik.limit * XM_PI;

	// 試行回数上限値までループ
	for (int c = 0; c < ik.iterations; ++c)
	{
		// エフェクタとターゲット位置の差が閾値以下になったら試行回数上限値になる前に抜ける
		if (XMVector3Length(XMVectorSubtract(endPos, targetNextPos)).m128_f32[0] <= epsilon)
		{
			break;
		}

		// 子から親の方向へのループ
		for (int bidx = 0; bidx < bonePositions.size(); ++bidx)
		{
			const XMVECTOR& pos = bonePositions[bidx];
			const XMVECTOR& vecToEnd = XMVector3Normalize(XMVectorSubtract(endPos, pos));
			const XMVECTOR& vecToTarget = XMVector3Normalize(XMVectorSubtract(targetNextPos, pos));

			// ほぼ同じベクトルであれば、外積できないし回転させる必要も乏しいので次のボーンへ
			if (XMVector3Length(XMVectorSubtract(vecToEnd, vecToTarget)).m128_f32[0] <= epsilon)
			{
				continue;
			}

			const XMVECTOR& cross = XMVector3Normalize(XMVector3Cross(vecToEnd, vecToTarget));
			// 本ではBetweenVectorsを使っているが計算がもったいないのでBetweenNormalsを使う
			float angle = XMVector3AngleBetweenNormals(vecToEnd, vecToTarget).m128_f32[0];
			// TODO:一気に回転させると不正確な結果になったりするのか？
			angle = min(angle, ikLimit);

			// 回転を重ねがけしていく
			const XMMATRIX& mat = XMMatrixTranslationFromVector(-pos) * XMMatrixRotationAxis(cross, angle) * XMMatrixTranslationFromVector(pos);
			mats[bidx] *= mat;
			// 自分より子の位置を回転によって更新しておく
			for (int idx = bidx - 1; idx >= 0; --idx)
			{
				bonePositions[idx] = XMVector3Transform(bonePositions[idx], mat);
			}
			endPos = XMVector3Transform(endPos, mat);

			// エフェクタとターゲット位置の差が閾値以下になったら親までループする前に抜ける。この後、さらに上にあるbreakで全体を抜けることになる
			if (XMVector3Length(XMVectorSubtract(endPos, targetNextPos)).m128_f32[0] <= epsilon)
			{
				break;
			}
		}
	}

	int idx = 0;
	for (uint16_t cidx : ik.nodeIdxes)
	{
		_boneMatrices[cidx] = mats[idx];
		++idx;
	}
	//TODO:上のtargetNexPosの計算といい、この計算は意味がわからん
	const BoneNode& rootNode = *_boneNodeAddressArray[ik.nodeIdxes.back()];
	RecursiveMatrixMultiply(rootNode, parentMat);
}
void PMDActor::IKSolve()
{
	for (const PMDIK& ik : _ikData)
	{
		size_t childrenNodesCount = ik.nodeIdxes.size();
		switch (childrenNodesCount)
		{
			case 0:
				assert(false);
				break;
			case 1:
				SolveLookAt(ik);
				break;
			case 2:
				SolveCosineIK(ik);
				break;
			default:
				SolveCCDIK(ik);
				break;
		}
	}
}

void PMDActor::Draw(bool isShadow)
{
	_dx12.CommandList()->IASetVertexBuffers(0, 1, &_vbView);
	_dx12.CommandList()->IASetIndexBuffer(&_ibView);

	ID3D12DescriptorHeap* tdh[] = {_transformDescHeap.Get()};
	_dx12.CommandList()->SetDescriptorHeaps(1, tdh);
	_dx12.CommandList()->SetGraphicsRootDescriptorTable(1, _transformDescHeap->GetGPUDescriptorHandleForHeapStart());

	ID3D12DescriptorHeap* mdh[] = {_materialDescHeap.Get()};
	_dx12.CommandList()->SetDescriptorHeaps(1, mdh);

	// マテリアルセクションごとにマテリアルを切り替えて描画
	CD3DX12_GPU_DESCRIPTOR_HANDLE materialH(_materialDescHeap->GetGPUDescriptorHandleForHeapStart());
	unsigned int idxOffset = 0;
	UINT cbvsrvIncSize = _dx12.Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 5; // CBVと通常テクスチャとsphとspaとCLUTのSRV

	if (isShadow)
	{
		// シャドウマップ描画ではマテリアルごとにドローコールを分ける必要がない
		_dx12.CommandList()->DrawIndexedInstanced(_indexNum, 1, 0, 0, 0);
	}
	else
	{
		for (const Material& m : _materials)
		{
			_dx12.CommandList()->SetGraphicsRootDescriptorTable(2, materialH);
			// 本体と影モデルの2つをインスタンス番号を分けて描画する。
			_dx12.CommandList()->DrawIndexedInstanced(m.indicesNum, 2, idxOffset, 0, 0);
			materialH.ptr += cbvsrvIncSize;
			idxOffset += m.indicesNum;
		}
	}
}

