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
		// �t�H���_��؂肪/�ł�\�ł��Ή��ł���悤�ɂ���B
		// rfind�͌�����Ȃ�������epos(-1�A0xffffffff)��Ԃ��B
		int pathIndex1 = (int)modelPath.rfind('/');
		int pathIndex2 = (int)modelPath.rfind('\\');
		int pathIndex = max(pathIndex1, pathIndex2);
		const std::string& folderPath = modelPath.substr(0, pathIndex + 1);
		return folderPath + texPath;
	}

	// ���݂�XYZ���W����lookat�̕�����Z'���ɂ���悤�ȉ�]�s������߂�B
	// �܂�A���݂�Z����lookat�̕����ɉ�]������悤�ȉ�]�s������߂�B
	// ���ꂾ������X'���AY'���̕�������܂�Ȃ��̂ŁAup�Aright������n���Ă����A����Ɋ�Â���
	// X'���AY'�������߂�
	XMMATRIX LookAtMatrix(const XMVECTOR& lookat, const XMFLOAT3& up, const XMFLOAT3& right)
	{
		XMVECTOR vz = XMVector3Normalize(lookat);
		XMVECTOR vy = XMVector3Normalize(XMLoadFloat3(&up));

		XMVECTOR vx;
		// lookat��up�����s�Ȃ�Aright��Ō��߂�
		// �����łȂ����up��Ō��߂�
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

		XMMATRIX ret = XMMatrixIdentity(); // ���s�ړ�������0�ɂ��Ă���
		ret.r[0] = vx;
		ret.r[1] = vy;
		ret.r[2] = vz;
		return ret;
	}

	// origin�̕�����lookat�̕����Ɍ�������悤�ȉ�]�s������߂�
	// �c��2�̎��̕��������߂��Ƃ��邽�߂�up�Aright��n���B
	XMMATRIX LookAtMatrix(const XMVECTOR& origin, const XMVECTOR& lookat, const XMFLOAT3& up, const XMFLOAT3& right)
	{
		return
			// origin�����݂�Z���̕����Ɍ�������悤�ȉ�]
			XMMatrixTranspose(LookAtMatrix(origin, up, right))
			// ���݂�Z����lookat�̕����Ɍ�������悤�ȉ�]
			* LookAtMatrix(lookat, up, right);
	}

	float GetYFromXOnBezier(float x, const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b, uint8_t n = 12)
	{
		// �����̂Ƃ���y=x
		if (a.x == a.y && b.x == b.y)
		{
			return x;
		}

		// �M�ҊJ���̔������ݖ@�ŗ^����ꂽx�̂Ƃ���x(t) = x�𖞂���t�����߂�

		float t = x;
		const float k0 = 1 + 3 * a.x - 3 * b.x;
		const float k1 = 3 * b.x - 6 * a.x;
		const float k2 = 3 * a.x;

		// �I������ɗp����덷����l
		constexpr float epsilon = 0.0005f;
		for (int i = 0; i < n; ++i)
		{
			float ft = k0 * t * t * t + k1 * t * t + k2 * t - x;
			// �덷����l���ɂ����܂����̂Ő�ɏI��
			if (ft <= epsilon && ft >= -epsilon)
			{
				break;
			}

			t -= ft * 0.5f;
		}

		// ���܂����ߎ���t����y(t)�����߂�
		float r = 1 - t;
		// TODO:�Ȃ��������1-t�𗘗p���鎮�ŁA��͗��p���Ȃ����ɂ��Ă���̂��H
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
	// PMD�w�b�_�i�[�f�[�^
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
	// PMD�}�e���A���f�[�^�ǂݏo���p
	// PMD�̓t�H���V�F�[�f�B���O�̖͗l
	struct PMDMaterial
	{
		XMFLOAT3 diffuse;
		float alpha;
		float specularity;
		XMFLOAT3 specular;
		XMFLOAT3 ambient;
		unsigned char toonIdx;
		unsigned char edgeFlg;
		// �{�������ł��̍\���̂�2�o�C�g�̃p�f�B���O����������
		unsigned int indicesNum;
		char texFilePath[20];
	}; // pack(1)���Ȃ����70�o�C�g�̂͂���72�o�C�g�ɂȂ�
#pragma pack()

#pragma pack(1)
	struct PMDVertex {
		XMFLOAT3 pos; // ���_ ���W
		XMFLOAT3 normal; // �@�� �x�N�g��
		XMFLOAT2 uv; // uv ���W
		unsigned short boneNo[2]; // �{�[�� �ԍ�
		unsigned char boneWeight; // �{�[�� �e���x
		unsigned char edgeFlg; // �֊s �� �t���O
	}; // pack(1)���Ȃ����38�o�C�g�̂͂���40�o�C�g�ɂȂ�
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

	// ���_�o�b�t�@�ւ̃f�[�^��������
	unsigned char* vertMap = nullptr;
	result = _vertBuff->Map(0, nullptr, (void**)&vertMap);
	if (FAILED(result))
	{
		assert(false);
		return result;
	}
	std::copy(vertices.begin(), vertices.end(), vertMap);
	_vertBuff->Unmap(0, nullptr);

	// ���_�o�b�t�@�[�r���[�̗p��
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

	// �C���f�b�N�X�o�b�t�@�ւ̃f�[�^��������
	unsigned short* idxMap = nullptr;
	result = _idxBuff->Map(0, nullptr, (void**)&idxMap);
	if (FAILED(result))
	{
		assert(false);
		return result;
	}
	std::copy(indices.begin(), indices.end(), idxMap);
	_idxBuff->Unmap(0, nullptr);

	// �C���f�b�N�X�o�b�t�@�[�r���[�̗p��
	_ibView.BufferLocation = _idxBuff->GetGPUVirtualAddress();
	_ibView.Format = DXGI_FORMAT_R16_UINT;
	_ibView.SizeInBytes = (UINT)(indices.size() * sizeof(indices[0]));

	// �}�e���A�����̓ǂݏo��
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

			// �g�D�[���V�F�[�f�B���O�p��CLUT�e�N�X�`�����\�[�X�̃��[�h
			std::string toonFilePath = "toon/";
			char toonFileName[16];
			sprintf_s(toonFileName, 16, "toon%02d.bmp", pmdMaterials[i].toonIdx + 1); // ���̑����Z����255+1��256�ň����邪�A����toon00.bmp�͂Ȃ����߂��̂܂܂ɂ��Ă���
			toonFilePath += toonFileName;
			_toonResources[i] = _dx12.LoadTextureFromFile(toonFilePath);

			if (strlen(pmdMaterials[i].texFilePath) > 0)
			{
				// �ʏ�e�N�X�`���Asph�Aspa�̃��\�[�X�̃��[�h
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
	// PMD�{�[���f�[�^�ǂݏo���p
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

	// �t�@�C���ɂ��f�[�^�ł���ΐe�C���f�b�N�X�����̂��f�[�^��������
	// �Ȃ邪�A�g���o�[�X���l����Ƌt�Ɏq�̔z������f�[�^�\���̕����悢�̂�
	// �f�[�^�����ς���

	// ��Ɨp
	std::vector<std::string> boneNames(boneNum);
	_boneNameArray.resize(boneNum);
	_boneNodeAddressArray.resize(boneNum);

	// "�Ђ�"��PMD�ł�CosineIK�Ő܂�Ȃ��鎲��X���ɌŒ肷��d�l�Ȃ̂�
	// "�Ђ�"�Ƃ�����������܂ލ������W���Ă���
	_kneeIdxes.clear();

	// �{�[���m�[�h�}�b�v�쐬
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

		if (boneNames[idx].find("�Ђ�") != std::string::npos)
		{
			_kneeIdxes.emplace_back(idx);
		}
	}

	// �{�[���m�[�h���m�̐e�q�֌W�̍\�z
	for (const PMDBone& pb : pmdBones)
	{
		if (pb.parentNo >= pmdBones.size())
		{
			// �e�͂��Ȃ��ꍇ��unsigned short�̍ő�l�������Ă���
			continue;
		}

		const std::string& parentName = boneNames[pb.parentNo];
		_boneNodeTable[parentName].children.emplace_back(&_boneNodeTable[pb.boneName]);
	}

	_boneMatrices.resize(boneNum);
	std::fill(_boneMatrices.begin(), _boneMatrices.end(), XMMatrixIdentity());

	// IK�f�o�b�O�R�[�h
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
			oss << "IK�{�[���ԍ�=" << ik.boneIdx << ":" << getNameFromIdx(ik.boneIdx) << std::endl;
			for (uint16_t nodeIdx : ik.nodeIdxes)
			{
				oss << "\t�m�[�h�{�[��=" << nodeIdx << ":" << getNameFromIdx(nodeIdx) << std::endl;
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
		char boneName[15]; // �{�[����
		unsigned int frameNo; // �t���[���ԍ�
		XMFLOAT3 location; //�ʒu
		XMFLOAT4 quaternion; // �N�I�[�^�j�I��
		unsigned char bezier[64]; // [4][4][4] �x�W�F��ԃp�����[�^
	};

	// �{�[�����̌�ɃA���C�������g�Ńp�f�B���O������̂Ń��[�v�ł����������ǂ�
	std::vector<VMDKeyFrame> keyframes(keyframeNum);
	for (VMDKeyFrame& keyframe : keyframes)
	{
		fread(keyframe.boneName, sizeof(keyframe.boneName), 1, fp);
		fread(&keyframe.frameNo, sizeof(keyframe.frameNo) + sizeof(keyframe.location) + sizeof(keyframe.quaternion) + sizeof(keyframe.bezier), 1, fp);
	}

	fclose(fp);

	_duration = 0;

	// VMDKeyFrame����KeyFrame�ɂ߂����B���łɍő�t���[���ԍ��̃L�[�t���[����VMD�̃��[�V�����̃t���[�����Ƃ���
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

	// VMDKeyFrame�̃L�[�t���[���̓t���[���ԍ����ɓ����Ă�Ƃ͌���Ȃ��̂Ń\�[�g
	for (auto& motion : _motiondata) // TODO:std::pair��const &��&���ƃR���p�C�����ʂ�Ȃ��̂�auto���g��
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
		// VMD�ɂ���L�[�t���[�����̃{�[������PMD�ɂ���Ƃ͌���Ȃ��̂ł���Ƃ��̂ݏ���������
		const auto& itBoneNode = _boneNodeTable.find(bonemotion.first);
		if (itBoneNode == _boneNodeTable.end())
		{
			continue;
		}

		const BoneNode&	node = itBoneNode->second;
		const XMFLOAT3& pos = node.startPos;
		//�܂���0�t���[���ڂ̃|�[�Y�̂ݎg��
		// �����ŋ��߂Ă���̂̓��[�J���s��ł���
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
	// �萔�o�b�t�@�쐬

	size_t buffSize = sizeof(XMMATRIX) * (1 + _boneMatrices.size()); // 1�̓��[���h�s��̕�
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

	// �f�B�X�N���v�^�q�[�v��CBV�쐬
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
	// �}�e���A���o�b�t�@���쐬
	// sizeof(MaterialForHlsl)��44�o�C�g��256�ŃA���C�������g���Ă���̂�256�B
	// ���Ȃ���������Ȃ�
	// TODO:�萔�o�b�t�@���}�e���A������������Ă��邩��A��ɂ܂Ƃ߂��Ȃ����H
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

	// �f�B�X�N���v�^�q�[�v��CBV�쐬
	D3D12_DESCRIPTOR_HEAP_DESC materialDescHeapDesc = {};
	materialDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	materialDescHeapDesc.NodeMask = 0;
	materialDescHeapDesc.NumDescriptors = (UINT)(materialNum * 5); // MaterialForHlsl��CBV�ƒʏ�e�N�X�`����sph��spa��CLUT��SRV��5����
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
	// Format�̓e�N�X�`���ɂ��

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
	// ���[�v
	if (frameNo > _duration)
	{
		_startTime = timeGetTime();
		frameNo = 0;
	}

	// �O�t���[���̃|�[�Y���N���A
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

		// ������Ȃ������ꍇ
		if (rit == keyframes.rend())
		{
			continue;
		}

		// ���̃L�[�t���[��
		auto it = rit.base();

		XMMATRIX rotation = XMMatrixRotationQuaternion(rit->quaternion);
		XMVECTOR offset = XMLoadFloat3(&rit->offset);
		if (it != keyframes.end())
		{
			// �M�ҊJ���̔������ݖ@�ŁA�����l��lerp�ɂƂ�A�x�W�G�Ȑ���̋ߎ��l���擾����
			float t = (float)(frameNo - rit->frameNo) / (it->frameNo - rit->frameNo);
			t = GetYFromXOnBezier(t, it->p1, it->p2, 12); // �M�҂̌o����A12����x�Ŏ���
			rotation = XMMatrixRotationQuaternion(XMQuaternionSlerp(rit->quaternion, it->quaternion, t));
			offset = XMVectorLerp(offset, XMLoadFloat3(&it->offset), t);
		}

		const XMFLOAT3& pos = node.startPos;
		// �L�[�t���[���̏��ŉ�]�̂ݎg�p����
		// �����ŋ��߂Ă���̂̓��[�J���s��ł���
		_boneMatrices[node.boneIdx] = XMMatrixTranslation(-pos.x, -pos.y, -pos.z) * rotation * XMMatrixTranslation(pos.x, pos.y, pos.z) * XMMatrixTranslationFromVector(offset);
	}

	// �Z���^�[�͓����Ȃ��O��ŒP�ʍs��
	// ����ɂ����_boneMatrices[]�̓��f���s��ɂȂ�
	RecursiveMatrixMultiply(_boneNodeTable["�Z���^�["], XMMatrixIdentity());

	//TODO: IK���s��������N�������[�V�����t�@�C�����g�������̂ł�������~�߂�
	//IKSolve();

	std::copy(_boneMatrices.begin(), _boneMatrices.end(), &_mappedMatrices[1]);
}

void PMDActor::SolveLookAt(const struct PMDIK& ik)
{
	assert(ik.nodeIdxes.size() == 1);
	// ik.targetIdx�͂��̏ꍇ�͖���`�Ȃ̂Ŏg��Ȃ�

	// ���[�g�ł���G�t�F�N�^�ł�����{�[��
	uint16_t rootNodeIdx = ik.nodeIdxes[0];
	// LookAt�̃^�[�Q�b�g�ʒu���w�肷��{�[��
	uint16_t targetNodeIdx = ik.boneIdx;

	const BoneNode* rootNode = _boneNodeAddressArray[rootNodeIdx];
	const BoneNode* targetNode = _boneNodeAddressArray[targetNodeIdx];

	const XMVECTOR& opos1 = XMLoadFloat3(&rootNode->startPos);
	const XMVECTOR& tpos1 = XMLoadFloat3(&targetNode->startPos);

	const XMVECTOR& opos2 = XMVector3Transform(opos1, _boneMatrices[rootNodeIdx]);
	const XMVECTOR& tpos2 = XMVector3Transform(tpos1, _boneMatrices[targetNodeIdx]);

	// �o�C���h�|�[�Y���̃G�t�F�N�^�ƃ^�[�Q�b�g�Ԃ̃x�N�g��
	XMVECTOR originVec = XMVectorSubtract(tpos1, opos1);
	// �A�j���[�V�������̃G�t�F�N�^�ƃ^�[�Q�b�g�Ԃ̃x�N�g��
	XMVECTOR targetVec = XMVectorSubtract(tpos2, opos2);
	originVec = XMVector3Normalize(originVec);
	targetVec = XMVector3Normalize(targetVec);

	// �G�t�F�N�^���^�[�Q�b�g�Ɍ�����ɂ́A�o�C���h�|�[�Y���̃x�N�g����
	// �A�j���[�V�����̃x�N�g���ɕϊ�����悤�ȉ�]��������΂悢
	_boneMatrices[rootNodeIdx] =
		XMMatrixTranslationFromVector(-opos2)
		* LookAtMatrix(originVec, targetVec, XMFLOAT3(0, 1, 0), XMFLOAT3(1, 0, 0))
		* XMMatrixTranslationFromVector(opos2);
}

void PMDActor::SolveCosineIK(const struct PMDIK& ik)
{
	// IK�\���_�̈ʒu��ێ����郏�[�N�f�[�^
	std::vector<XMVECTOR> positions;
	// �o�C���h�|�[�Y����3�_�̊Ԃ�2�����̕ێ�
	std::array<float, 2> edgeLens;

	// �^�[�Q�b�g
	uint16_t targetNodeIdx = ik.boneIdx;
	const BoneNode* targetNode = _boneNodeAddressArray[targetNodeIdx];
	const XMVECTOR& targetPos = XMVector3Transform(XMLoadFloat3(&targetNode->startPos), _boneMatrices[targetNodeIdx]);

	// ���[�A�G�t�F�N�^
	uint16_t endNodeIdx = ik.targetIdx;
	const BoneNode* endNode = _boneNodeAddressArray[endNodeIdx];

	// positions�Ƀo�C���h�|�[�Y�ʒu������
	// nodeIdxes�͎q������e�̏��Ńf�[�^�������Ă���̂ɒ���
	positions.emplace_back(XMLoadFloat3(&endNode->startPos));

	assert(ik.nodeIdxes.size() == 2);
	for (uint16_t chainBoneIdx : ik.nodeIdxes)
	{
		const BoneNode* boneNode = _boneNodeAddressArray[chainBoneIdx];
		positions.emplace_back(XMLoadFloat3(&boneNode->startPos));
	}
	assert(positions.size() == 3);

	// ���[�g����ɂȂ�悤�ɋt�ɂ���
	std::reverse(positions.begin(), positions.end());

	edgeLens[0] = XMVector3Length(XMVectorSubtract(positions[1], positions[0])).m128_f32[0];
	edgeLens[1] = XMVector3Length(XMVectorSubtract(positions[2], positions[1])).m128_f32[0];

	// positions�ɃA�j���[�V�����ʒu������
	// nodeIdxes�͋t���Ȃ̂Ń��[�g��nodeIdxes[1]�Ȃ̂ɒ���
	positions[0] = XMVector3Transform(positions[0], _boneMatrices[ik.nodeIdxes[1]]);
	// positions[1]��IK�v�Z�Ō��߂�̂ŃA�j���[�V�����ʒu�͌v�Z���Ȃ�
	// �G�t�F�N�^���^�[�Q�b�g�̈ʒu�Ɉړ�������B����āA�A�j���[�V�����ʒu��_boneMatrices[endNodeIdx]����_boneMatrices[targetNodeIdx]���g��
	// TODO:����́A�{���Ƀ^�[�Q�b�g�ʒu�Ɉړ����������Ȃ�
	// positions[2] = targetPos;�ɂ��ׂ��ł́H
	// ������������ƁA���̒���������Ȃ��Ȃ�P�[�X���l�����Ă�̂��Ǝv����
	// �����A�ŏI�s�̌v�Z�����Ă����������A�v�Z���@���l�̒m���Ă�TwoBoneIK�ƈႤ
	positions[2] = XMVector3Transform(positions[2], _boneMatrices[targetNodeIdx]);

	// ���[�g����G�t�F�N�^�ւ̃x�N�g��
	const XMVECTOR& linearVec = XMVectorSubtract(positions[2], positions[0]);
	// CosineIK�v�Z������O�p�`�̊e�ӂ̒���
	float A = XMVector3Length(linearVec).m128_f32[0];
	float B = edgeLens[0];
	float C = edgeLens[1];

	// ���[�g�̊p�̊p�x
	float theta1 = acosf((A * A + B * B - C * C) / (2 * A * B));
	// �^�񒆂̓_�̊p�̊p�x
	float theta2 = acosf((B * B + C * C - A * A) / (2 * B * C));

	// CosineIK�Ő܂�Ȃ��鎲�����߂�
	// PMD��IK�͐^�񒆂��G�̂Ƃ��͋����I��X���ɂ���d�l�ł���
	XMVECTOR axis;
	if (std::find(_kneeIdxes.begin(), _kneeIdxes.end(), ik.nodeIdxes[0]) == _kneeIdxes.end())
	{
		// �^�񒆂��G�ł͂Ȃ��Ƃ��̓��[�g�ƃG�t�F�N�^�����ԃx�N�g����
		// ���[�g�ƃ^�[�Q�b�g�����ԃx�N�g����2���܂ޕ��ʓ��ŉ�]������悤�ɂ���
		const XMVECTOR& vm = XMVector3Normalize(XMVectorSubtract(positions[2], positions[0]));
		const XMVECTOR& vt = XMVector3Normalize(XMVectorSubtract(targetPos, positions[0]));
		axis = XMVector3Cross(vt, vm);
	}
	else
	{
		const XMFLOAT3& right = XMFLOAT3(1, 0, 0);
		axis = XMLoadFloat3(&right);
	}

	// ���[�g
	_boneMatrices[ik.nodeIdxes[1]] *= XMMatrixTranslationFromVector(-positions[0]) * XMMatrixRotationAxis(axis, theta1) * XMMatrixTranslationFromVector(positions[0]);
	// �^��
	// TODO:���̏�Z���Ė{���Ƀ��f���s��̌v�Z�ɂȂ��Ă���̂��H
	_boneMatrices[ik.nodeIdxes[0]] = XMMatrixTranslationFromVector(-positions[1]) * XMMatrixRotationAxis(axis, theta2 - XM_PI) * XMMatrixTranslationFromVector(positions[1]) * _boneMatrices[ik.nodeIdxes[1]];
	// �G�t�F�N�^
	//TODO:���̌v�Z�����s��
	_boneMatrices[endNodeIdx] = _boneMatrices[ik.nodeIdxes[0]];
}

// ��������Ɏg�p����덷臒l
constexpr float epsilon = 0.0005f;

void PMDActor::SolveCCDIK(const struct PMDIK& ik)
{
	// �^�[�Q�b�g
	uint16_t targetNodeIdx = ik.boneIdx;
	const BoneNode* targetBoneNode = _boneNodeAddressArray[targetNodeIdx];
	const XMVECTOR& targetOriginPos = XMLoadFloat3(&targetBoneNode->startPos);

	const XMMATRIX& parentMat = _boneMatrices[_boneNodeAddressArray[targetNodeIdx]->ikParentBone];
	XMVECTOR det;
	const XMMATRIX& invParentMat = XMMatrixInverse(&det, parentMat);
	// TODO:�����Ȃ胍�[�J���ȍs������f�����W�n�ł�startPos�ɏ�Z���Ăǂ��Ȃ�Ƃ����̂��H
	const XMVECTOR& targetNextPos = XMVector3Transform(targetOriginPos, _boneMatrices[ik.boneIdx] * invParentMat);

	// �G�t�F�N�^�̃o�C���h�|�[�Y�ʒu�B
	XMVECTOR endPos = XMLoadFloat3(&_boneNodeAddressArray[ik.targetIdx]->startPos);

	// IK�Ώۂ̃{�[���̈ʒu��ێ����郏�[�N�f�[�^�i�G�t�F�N�^�ȊO�j
	// �q����e�̏��ɓ����Ă��邱�Ƃɒ���
	std::vector<XMVECTOR> bonePositions;

	// bonePositions�Ƀo�C���h�|�[�Y�ʒu������
	assert(ik.nodeIdxes.size() >= 3);
	for (uint16_t cidx : ik.nodeIdxes)
	{
		const BoneNode* boneNode = _boneNodeAddressArray[cidx];
		bonePositions.emplace_back(XMLoadFloat3(&boneNode->startPos));
	}
	assert(bonePositions.size() >= 3);
	//TODO:�Ȃ��{�ł�CCDIK�̎��s�̏����l���o�C���h�|�[�Y�ł͂��߂�̂��H
	// �A�j���[�V�����ʒu����͂��߂������ω����ŏ��ɂȂ�A���R�ɂȂ�̂ł́H

	// ��]�s��̌v�Z���ʕێ�
	std::vector<XMMATRIX> mats(bonePositions.size());
	// �P�ʍs��ŏ��������Ă���
	std::fill(mats.begin(), mats.end(), XMMatrixIdentity());

	// 1�t���[���ŉ�]������p�x�̏���l
	float ikLimit = ik.limit * XM_PI;

	// ���s�񐔏���l�܂Ń��[�v
	for (int c = 0; c < ik.iterations; ++c)
	{
		// �G�t�F�N�^�ƃ^�[�Q�b�g�ʒu�̍���臒l�ȉ��ɂȂ����玎�s�񐔏���l�ɂȂ�O�ɔ�����
		if (XMVector3Length(XMVectorSubtract(endPos, targetNextPos)).m128_f32[0] <= epsilon)
		{
			break;
		}

		// �q����e�̕����ւ̃��[�v
		for (int bidx = 0; bidx < bonePositions.size(); ++bidx)
		{
			const XMVECTOR& pos = bonePositions[bidx];
			const XMVECTOR& vecToEnd = XMVector3Normalize(XMVectorSubtract(endPos, pos));
			const XMVECTOR& vecToTarget = XMVector3Normalize(XMVectorSubtract(targetNextPos, pos));

			// �قړ����x�N�g���ł���΁A�O�ςł��Ȃ�����]������K�v���R�����̂Ŏ��̃{�[����
			if (XMVector3Length(XMVectorSubtract(vecToEnd, vecToTarget)).m128_f32[0] <= epsilon)
			{
				continue;
			}

			const XMVECTOR& cross = XMVector3Normalize(XMVector3Cross(vecToEnd, vecToTarget));
			// �{�ł�BetweenVectors���g���Ă��邪�v�Z�����������Ȃ��̂�BetweenNormals���g��
			float angle = XMVector3AngleBetweenNormals(vecToEnd, vecToTarget).m128_f32[0];
			// TODO:��C�ɉ�]������ƕs���m�Ȍ��ʂɂȂ����肷��̂��H
			angle = min(angle, ikLimit);

			// ��]���d�˂������Ă���
			const XMMATRIX& mat = XMMatrixTranslationFromVector(-pos) * XMMatrixRotationAxis(cross, angle) * XMMatrixTranslationFromVector(pos);
			mats[bidx] *= mat;
			// �������q�̈ʒu����]�ɂ���čX�V���Ă���
			for (int idx = bidx - 1; idx >= 0; --idx)
			{
				bonePositions[idx] = XMVector3Transform(bonePositions[idx], mat);
			}
			endPos = XMVector3Transform(endPos, mat);

			// �G�t�F�N�^�ƃ^�[�Q�b�g�ʒu�̍���臒l�ȉ��ɂȂ�����e�܂Ń��[�v����O�ɔ�����B���̌�A����ɏ�ɂ���break�őS�̂𔲂��邱�ƂɂȂ�
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
	//TODO:���targetNexPos�̌v�Z�Ƃ����A���̌v�Z�͈Ӗ����킩���
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

	// �}�e���A���Z�N�V�������ƂɃ}�e���A����؂�ւ��ĕ`��
	CD3DX12_GPU_DESCRIPTOR_HANDLE materialH(_materialDescHeap->GetGPUDescriptorHandleForHeapStart());
	unsigned int idxOffset = 0;
	UINT cbvsrvIncSize = _dx12.Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 5; // CBV�ƒʏ�e�N�X�`����sph��spa��CLUT��SRV

	if (isShadow)
	{
		// �V���h�E�}�b�v�`��ł̓}�e���A�����ƂɃh���[�R�[���𕪂���K�v���Ȃ�
		_dx12.CommandList()->DrawIndexedInstanced(_indexNum, 1, 0, 0, 0);
	}
	else
	{
		for (const Material& m : _materials)
		{
			_dx12.CommandList()->SetGraphicsRootDescriptorTable(2, materialH);
			// �{�̂Ɖe���f����2���C���X�^���X�ԍ��𕪂��ĕ`�悷��B
			_dx12.CommandList()->DrawIndexedInstanced(m.indicesNum, 2, idxOffset, 0, 0);
			materialH.ptr += cbvsrvIncSize;
			idxOffset += m.indicesNum;
		}
	}
}

