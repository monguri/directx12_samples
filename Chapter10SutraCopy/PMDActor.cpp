#include "PMDActor.h"
#include "Dx12Wrapper.h"
#include "PMDRenderer.h"
#include <unordered_map>

using namespace DirectX;
using namespace Microsoft::WRL;

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
} // namespace

PMDActor::PMDActor(Dx12Wrapper& dx12, PMDRenderer& renderer, const std::string& modelPath)
: _dx12(dx12), _renderer(renderer)
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

void* PMDActor::Transform::operator new(size_t size)
{
	return _aligned_malloc(size, 16);
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

	unsigned int indicesNum;
	fread(&indicesNum, sizeof(indicesNum), 1, fp);

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

	std::vector<unsigned short> indices(indicesNum);
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
				_toonResources[i] = _renderer.GetGrayGradientTexture();
			}

			if (_textureResources[i] == nullptr)
			{
				_textureResources[i] = _renderer.GetWhiteTexture();
			}

			if (_sphResources[i] == nullptr)
			{
				_sphResources[i] = _renderer.GetWhiteTexture();
			}

			if (_spaResources[i] == nullptr)
			{
				_spaResources[i] = _renderer.GetBlackTexture();
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

	fclose(fp);

	// ファイルにもつデータであれば親インデックスをもつのがデータが小さく
	// なるが、トラバースを考えると逆に子の配列をもつデータ構造の方がよいので
	// データを作り変える

	// 作業用
	std::vector<std::string> boneNames(boneNum);

	// ボーンノードマップ作成
	for (unsigned short idx = 0; idx < pmdBones.size(); idx++)
	{
		const PMDBone& pb = pmdBones[idx];
		boneNames[idx] = pb.boneName;

		BoneNode& node = _boneNodeTable[pb.boneName];
		node.boneIdx = idx;
		node.startPos = pb.pos;
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

	return result;
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

	struct KeyFrame {
		unsigned int frameNo;
		XMVECTOR quaternion;

		KeyFrame(unsigned int fno, const XMVECTOR& q) : frameNo(fno), quaternion(q) {}
	};

	std::unordered_map<std::string, std::vector<KeyFrame>> _motionData;

	// VMDKeyFrameからKeyFrameにつめかえ
	for (const VMDKeyFrame& keyframe : keyframes)
	{
		_motionData[keyframe.boneName].emplace_back(keyframe.frameNo, XMLoadFloat4(&keyframe.quaternion));
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

	// TODO:曲げテスト
	const BoneNode& armNode = _boneNodeTable["左腕"];
	const XMFLOAT3& armpos = armNode.startPos;
	const XMMATRIX& armMat = XMMatrixTranslation(-armpos.x, -armpos.y, -armpos.z) * XMMatrixRotationZ(XM_PIDIV2) * XMMatrixTranslation(armpos.x, armpos.y, armpos.z);
	_boneMatrices[armNode.boneIdx] = armMat;

	const BoneNode& elbowNode = _boneNodeTable["左ひじ"];
	const XMFLOAT3& elbowpos = elbowNode.startPos;
	const XMMATRIX& elbowMat = XMMatrixTranslation(-elbowpos.x, -elbowpos.y, -elbowpos.z) * XMMatrixRotationZ(-XM_PIDIV2) * XMMatrixTranslation(elbowpos.x, elbowpos.y, elbowpos.z);
	_boneMatrices[elbowNode.boneIdx] = elbowMat;

	RecursiveMatrixMultiply(_boneNodeTable["センター"], XMMatrixIdentity());

	//TODO: Transform構造体がもう不要なようだが。。。
	_mappedMatrices[0] = XMMatrixIdentity();
	std::copy(_boneMatrices.begin(), _boneMatrices.end(), &_mappedMatrices[1]);

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

void PMDActor::Draw()
{
#if 0 // スキニング計算を簡単にするために一旦回転を止める
	_angle += 0.005f;
	_mappedMatrices[0] = XMMatrixRotationY(_angle);
#endif

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

	for (const Material& m : _materials)
	{
		_dx12.CommandList()->SetGraphicsRootDescriptorTable(2, materialH);
		_dx12.CommandList()->DrawIndexedInstanced(m.indicesNum, 1, idxOffset, 0, 0);
		materialH.ptr += cbvsrvIncSize;
		idxOffset += m.indicesNum;
	}
}

