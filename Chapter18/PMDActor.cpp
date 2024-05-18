#include "PMDActor.h"
#include<iostream>
#include<cassert>
#include<DirectXMath.h>
#include<string>
#include<algorithm>

#include<sstream>//������X�g���[���p
#include<iomanip>//������}�j�s�����[�^�p(n�����낦�Ƃ�0���߂Ƃ��Ɏg��)

#include<Windows.h>
#include<d3d12.h>
#include<d3dx12.h>

#include"Dx12Wrapper.h"
#include"Helper.h"

using namespace DirectX;
using namespace std;

namespace {

	string GetDirectoryFromPath(string path) {
		int pos1 = path.rfind('/');
		int pos2 = path.rfind('\\');
		if (pos1 == string::npos&&pos2 == string::npos) {
			return "";
		}
		int pos=max(pos1, pos2);
		return path.substr(0, pos + 1);
	}
	///������𕪗�����
	///@param str �����Ώۂ̕�����
	///@param separator ��������(�����ŕ�������ڈ�)
	///@return ������̕�����x�N�^�z��
	std::vector<string> SeparateString(const std::string& str, const char separator = '*') {
		vector<string> ret;//�߂�l�p
		//�Ⴆ�Ε�����"a.bmp*b.sph"�������Ƃ���
		//�����string��find��substr���g�p���ĕ��������
		size_t idx = 0;
		size_t offset = 0;
		do {
			idx = str.find('*',offset);
			if (idx != string::npos) {
				ret.emplace_back(str.substr(offset,idx-offset));
				offset = idx + 1;
			}
			else {
				ret.emplace_back(str.substr(offset));
			}
		} while (idx != string::npos);
		return ret;
	}
	///�g���q��Ԃ�
	///@param path ���̃p�X������
	///@return �g���q������
	string GetExtension(const string& path) {
		int index = path.find_last_of('.');
		return path.substr(index + 1, path.length() - index);
	}
	constexpr float epsilon = 0.0005f;
	///�x�W�F�ɂ�����X����Y��Ԃ�
	///@param cPoints �R���g���[���|�C���g(�^�񒆂̂Q��)
	///@param x y���擾���邽�߂�x
	///@param trycnt �ߎ��̎��s��
	float GetYFromXOnBezier(const std::array<XMFLOAT2,2>& cPoints,float x,uint32_t trycnt=8) {
		//�x�W�F�̎�
		// P = P0*(1-t)^3 + 3P1*(1-t)^2*t + 3P2*(1-t)*t^2 + P3*t^3
		// ���������̎菇��x���W����(�ߎ���p����)t�����߁A
		// ���̓���ꂽt�����ƂɃx�W�F�̎��ɂ��y��Ԃ�
		//�������AP0=(0,0),P1=(1,1)

		//����p1.x==p1.y && p2.x==p2.y�̏ꍇ�A����͋Ȑ��ł͂Ȃ�
		//�v�Z�����������Ȃ��̂ł��̂܂ܕԂ�
		if (cPoints[0].x == cPoints[0].y && cPoints[1].x == cPoints[1].y) {
			return x;
		}

		float t = x;
		float r = 1.0f - t;
		for (int i = 0; i < trycnt; ++i) {
			//P1�`P4���Ƃ����
			float ft = 3 * cPoints[0].x * r*r*t +//P2
						3*cPoints[1].x * r*t*t +//P3
						t*t*t - x;//P4=1�Ȃ̂�t^3-x�ƂȂ�

			if (abs(ft) <= epsilon) {
				break;
			}

			t -= ft / 2.0f;
			r = 1.0f - t;
		}

		return 3 * cPoints[0].y * r*r*t +
			3 * cPoints[1].y * r*t*t +
			t * t*t;
	}
	///Z�������̕������������s���Ԃ��֐�
///@param lookat ���������������x�N�g��
///@param up ��x�N�g��
///@param right �E�x�N�g��
	XMMATRIX LookAtMatrix(const XMVECTOR& lookat, XMFLOAT3& up, XMFLOAT3& right) {
		//��������������(z��)
		XMVECTOR vz = XMVector3Normalize(lookat);

		//(�������������������������Ƃ���)����y���x�N�g��
		XMVECTOR vy = XMVector3Normalize(XMLoadFloat3(&up));

		//(�������������������������Ƃ���)y��
		XMVECTOR vx = XMVector3Normalize(XMVector3Cross(vy, vz));
		vy = XMVector3Normalize(XMVector3Cross(vz, vx));

		///LookAt��up�����������������Ă���right��ō�蒼��
		if (abs(XMVector3Dot(vy, vz).m128_f32[0]) == 1.0f) {
			//����X�������`
			vx = XMVector3Normalize(XMLoadFloat3(&right));
			//�������������������������Ƃ���Y�����v�Z
			vy = XMVector3Normalize(XMVector3Cross(vz, vx));
			//�^��X�����v�Z
			vx = XMVector3Normalize(XMVector3Cross(vy, vz));
		}
		XMMATRIX ret = XMMatrixIdentity();
		ret.r[0] = vx;
		ret.r[1] = vy;
		ret.r[2] = vz;
		return ret;
	}

	///����̃x�N�g�������̕����Ɍ����邽�߂̍s���Ԃ�
	///@param origin ����̃x�N�g��
	///@param lookat ��������������
	///@param up ��x�N�g��
	///@param right �E�x�N�g��
	///@retval ����̃x�N�g�������̕����Ɍ����邽�߂̍s��
	XMMATRIX LookAtMatrix(const XMVECTOR& origin, const XMVECTOR& lookat, XMFLOAT3& up, XMFLOAT3& right) {
		return XMMatrixTranspose(LookAtMatrix(origin, up, right))*
			LookAtMatrix(lookat, up, right);
	}
}


bool 
PMDActor::LoadFromPMD(const char* filepath) {
	FILE* fp=nullptr;
	auto err = fopen_s(&fp,filepath, "rb");
	assert(fp != nullptr);
	if (fp == nullptr)return false;

#pragma pack(1)
	struct PMDHeader {
		char signature[3];//3
		//�p�f�B���O�P������
		float version;//4
		char name[20];//20
		char comment[256];//256
	};//283�o�C�g
#pragma pack()
	PMDHeader header;
	fread_s(&header, sizeof(header), sizeof(PMDHeader), 1, fp);
	
	fread_s(&_vertNum, sizeof(_vertNum), sizeof(uint32_t), 1, fp);
#pragma pack(1)
	struct PMDVertex {
		float pos[3];//12
		float normal[3];//12
		float uv[2];//8
		uint16_t boneNo[2];//4
		uint8_t boneWeight;//1
		uint8_t edgeFlg;//1
	};//38�o�C�g
#pragma pack()
	struct PMDVertex4 {
		float pos[3];//12
		float normal[3];//12
		float uv[2];//8
		uint16_t boneNo[2];//4
		uint8_t boneWeight;//1
		uint8_t edgeFlg;//1
	};
	auto s = sizeof(PMDVertex4);

	//���_�f�[�^���[�h
	_vertexData.resize(sizeof(PMDVertex)*_vertNum);
	fread_s(_vertexData.data(), _vertexData.size(), _vertexData.size(), 1, fp);

	//�C���f�b�N�X�f�[�^���[�h
	fread(&_indexNum, sizeof(_indexNum), 1, fp);
	_indexData.resize(_indexNum);
	fread(_indexData.data(), sizeof(_indexData[0])*_indexData.size(), 1, fp);

	unsigned int materialCount;
	fread(&materialCount, sizeof(materialCount), 1, fp);
#pragma pack(1)
	struct PMDMaterial {
		XMFLOAT4 diffuse;//�f�B�t���[�Y�F
		float power;//�X�y�L�����搔
		XMFLOAT3 specular;//�X�y�L�����F
		XMFLOAT3 ambient;//����
		uint8_t toon;//�g�D�[���ԍ�
		uint8_t edge;//�G�b�W�t���O
		uint32_t indexNum;//�C���f�b�N�X��
		char texturePath[20];//�e�N�X�`���p�X(����)
	};
#pragma pack()
	vector<PMDMaterial> materials(materialCount);
	_materials.resize(materialCount);
	fread(materials.data(), sizeof(PMDMaterial), materialCount, fp);
	_texturePaths.resize(materialCount);
	for (int i = 0; i < materials.size(); ++i) {
		_materials[i].diffuse = materials[i].diffuse;
		_materials[i].power = materials[i].power;
		_materials[i].specular = materials[i].specular;
		_materials[i].ambient = materials[i].ambient;
		string texpath=materials[i].texturePath;
		if (texpath != "") {
			auto sepStr = SeparateString(texpath);
			for (auto& str : sepStr) {
				auto ext = GetExtension(str);
				auto pathStr = GetDirectoryFromPath(filepath) + str;
				if (ext == "sph") {
					_texturePaths[i].sphPath = pathStr;
				}
				else if (ext == "spa") {
					_texturePaths[i].spaPath = pathStr;
				}
				else {
					_texturePaths[i].texPath = pathStr;
				}
			}
		}
		if (materials[i].toon != 0xff) {
			//�g�D�[���p�X�𓾂�
			ostringstream oss;
			oss << "Model/toon/toon" << setw(2) << setfill('0') << static_cast<int>(materials[i].toon + 1) << ".bmp";
			_texturePaths[i].toonPath = oss.str();
		}
		_materials[i].indicesNum = materials[i].indexNum;
	}

	uint16_t boneNum=0;//�{�[����
	fread(&boneNum, sizeof(boneNum), 1, fp);
#pragma pack(1)
	struct PMDBone {
		char boneName[20];
		uint16_t parentBoneIdx;
		uint16_t tailBone;
		uint8_t type;
		uint16_t ikBone;
		XMFLOAT3 pos;
	};
#pragma pack()
	vector<PMDBone> pmdbones(boneNum);
	fread(pmdbones.data(), sizeof(PMDBone),boneNum, fp);

	fclose(fp);

	_boneMatrices.resize(pmdbones.size());
	_boneTree.resize(pmdbones.size());
	_boneAddressArray.resize(pmdbones.size());
	for (int i = 0; i < pmdbones.size();++i) {
		_boneTable[pmdbones[i].boneName] = 
			BoneInfo(i,pmdbones[i].pos);
		_boneMatrices[i] = XMMatrixIdentity();
		if (pmdbones[i].parentBoneIdx != 0xffff) {
			auto pidx = pmdbones[i].parentBoneIdx;
			_boneTree[pidx].push_back(i);
		}
		_boneAddressArray[i] = &_boneTable[pmdbones[i].boneName];
		string bname = pmdbones[i].boneName;
		if (bname=="��"){//bname.find("��") != std::string::npos) {
			_eyeBoneIdxes.push_back(i);
		}
	}
	return true;
}

void 
PMDActor::RecursiveBoneTransform(int idx,const DirectX::XMMATRIX& mat) {
	_boneMatrices[idx] *= mat;
	for (auto child:_boneTree[idx]) {
		RecursiveBoneTransform(child, _boneMatrices[idx]);
	}
}

PMDActor::PMDActor(shared_ptr<Dx12Wrapper> dx ,const char* path):_dx(dx),_pos(0,0,0),_rotator(0,0,0)
{
	LoadFromPMD(path);
	if (!CreateVertexBufferAndView()) {
		return;
	}
	if (!CreateIndexBufferAndView()) {
		return;
	}
	if (!CreateMaterialBuffer()) {
		return;
	}
	if (!LoadTexture()) {
		return;
	}
	if (!CreateMaterialBufferView()) {
		return;
	}
	LoadVMDData("motion/yagokoro.vmd");
		

	if (!CreateBoneBuffer()) {
		return;
	}




	if (!CreateTransformBuffer()) {
		return;
	}
	if (!CreateTransformBufferView()) {
		return;
	}
}

void 
PMDActor::LoadVMDData(const char* vmdpath) {
	FILE* fp = nullptr;
	auto err = fopen_s(&fp,vmdpath,"rb");

	//�ŏ���50�o�C�g�͍��̂Ƃ��떳�Ӗ��Ȃ̂Ŕ�΂�
	fseek(fp, 50, SEEK_SET);

	uint32_t keyframeNum=0;
	fread(&keyframeNum, sizeof(keyframeNum), 1, fp);

#pragma pack(1)
	struct VMDKeyFrame {
		char boneName[15];//�{�[����
		uint32_t frameNo;//�t���[���ԍ�
		XMFLOAT3 location;//�I�t�Z�b�g
		XMFLOAT4 quaternion;//�N�I�[�^�j�I��
		uint8_t bezier[64];//�x�W�F�f�[�^
	};
#pragma pack()
	vector<VMDKeyFrame> keyframes(keyframeNum);
	fread(keyframes.data(), sizeof(VMDKeyFrame), keyframes.size(), fp);
	fclose(fp);

	_duration = 0;
	//�e�{�[���̉�]�ɓK�p
	for (auto& keyframe : keyframes) {
		std::string boneName=keyframe.boneName;
		_keyframes[boneName].emplace_back(keyframe.frameNo,keyframe.quaternion,keyframe.location,
			static_cast<float>(keyframe.bezier[3+15])/127.f,//P1.x
			static_cast<float>(keyframe.bezier[7+15]) / 127.f,//P1.y
			static_cast<float>(keyframe.bezier[11+15]) / 127.f,//P2.x
			static_cast<float>(keyframe.bezier[15+15]) / 127.f );////P2.y
		_duration=max(keyframe.frameNo,_duration);
	}


	//�{�[�����̊e�L�[�t���[���̃\�[�g���s��
	//�����ɂ̓t���[���ԍ���p����
	for (auto& boneKeyFrame : _keyframes) {
		auto& keyframeVector = boneKeyFrame.second;
		sort(keyframeVector.begin(), keyframeVector.end(),
			[](const KeyFrame& lval,const KeyFrame& rval) {
			return lval.frameNo < rval.frameNo;
			}
		);
	}

	_lastTickCount = GetTickCount();

}

bool
PMDActor::CreateBoneBuffer() {
	if (_boneMatrices.empty()) {
		return false;
	}
	D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto size=AligmentedValue(sizeof(XMMATRIX)*_boneMatrices.size(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(size);
	auto result = _dx->Device()->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_bonesBuff.ReleaseAndGetAddressOf()));

	if (!CheckResult(result)) {
		return false;
	}

	
	result = _bonesBuff->Map(0, nullptr, (void**)&_mappedBoneMatrix);
	copy(_boneMatrices.begin(), _boneMatrices.end(), _mappedBoneMatrix);


	return true;
}

PMDActor::~PMDActor()
{
}

std::vector<Material>& 
PMDActor::Materials() {
	return _materials;
}

std::vector<MultiTexturePath>& 
PMDActor::GetTexturePaths() {
	return _texturePaths;
}

void 
PMDActor::Move(float x, float y, float z) {
	_pos.x += x;
	_pos.y += y;
	_pos.z += z;
}
void 
PMDActor::Rotate(float x, float y, float z) {
	_rotator.x += x;
	_rotator.y += y;
	_rotator.z += z;
}

const XMFLOAT3& 
PMDActor::GetPosition()const {
	return _pos;
}
const XMFLOAT3& 
PMDActor::GetRotate()const {
	return _rotator;
}

//�o�b�t�@���֐�
bool 
PMDActor::CreateVertexBufferAndView() {
	if (_vertexData.empty())return false;

	//���̏���GPU����g�p���邽�߂ɂ܂�
//���_�o�b�t�@�����
	D3D12_HEAP_PROPERTIES heapProp =
		CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

	D3D12_RESOURCE_DESC resDesc =
		CD3DX12_RESOURCE_DESC::Buffer(_vertexData.size());


	auto result = _dx->Device()->CreateCommittedResource(&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_vertexBuff.ReleaseAndGetAddressOf()));

	if (!CheckResult(result)) {
		return false;
	}

	//���_�f�[�^�𒸓_�o�b�t�@�ɃR�s�[
	uint8_t* mappedVertices;
	result = _vertexBuff->Map(0, nullptr, (void**)&mappedVertices);
	std::copy(_vertexData.begin(), _vertexData.end(), mappedVertices);
	//�I�������A���}�b�v
	_vertexBuff->Unmap(0, nullptr);

	_vbView.BufferLocation = _vertexBuff->GetGPUVirtualAddress();//�A�h���X
	_vbView.SizeInBytes = _vertexData.size();
	_vbView.StrideInBytes = 38;


	_vertexData.clear();
	return true;
}
bool 
PMDActor::CreateIndexBufferAndView() {
	if (_indexData.empty()) {
		return false;
	}

	D3D12_HEAP_PROPERTIES heapProp =
		CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

	D3D12_RESOURCE_DESC resDesc = 
		CD3DX12_RESOURCE_DESC::Buffer(sizeof(_indexData[0])*_indexData.size());
	
	auto result = _dx->Device()->CreateCommittedResource(&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_indexBuff.ReleaseAndGetAddressOf()));

	if (!CheckResult(result)) {
		return false;
	}

	_ibView.BufferLocation = _indexBuff->GetGPUVirtualAddress();
	_ibView.Format = DXGI_FORMAT_R16_UINT;
	_ibView.SizeInBytes = resDesc.Width;

	uint16_t* mappedIndices = nullptr;
	result = _indexBuff->Map(0, nullptr, (void**)&mappedIndices);
	CheckResult(result);
	copy(_indexData.begin(), _indexData.end(), mappedIndices);
	_indexBuff->Unmap(0, nullptr);
	_indexData.clear();
	return true;
}

unsigned int 
PMDActor::GetMaterialNum()const {
	return _materials.size();
}

bool 
PMDActor::CreateMaterialBuffer() {
	if (_materials.empty()) {
		return false;
	}
	D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

	auto& materials = _materials;
	struct MaterialForBuffer {
		DirectX::XMFLOAT4 diffuse;
		float power;
		DirectX::XMFLOAT3 specular;
		DirectX::XMFLOAT3 ambient;
	};
	D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(AligmentedValue(sizeof(MaterialForBuffer),
		D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)*materials.size());

	auto result = _dx->Device()->CreateCommittedResource(&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_materialBuff.ReleaseAndGetAddressOf()));

	assert(SUCCEEDED(result));
	if (FAILED(result)) {
		return false;
	}
	uint8_t* mappedMaterial = nullptr;
	result = _materialBuff->Map(0, nullptr, (void**)&mappedMaterial);
	assert(SUCCEEDED(result));
	if (FAILED(result)) {
		return false;
	}

	for (auto& material : materials) {
		MaterialForBuffer* pMFB = (MaterialForBuffer*)mappedMaterial;

		pMFB->diffuse = material.diffuse;
		pMFB->power = material.power;
		pMFB->specular = material.specular;
		pMFB->ambient = material.ambient;
		mappedMaterial += AligmentedValue(sizeof(MaterialForBuffer), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	}

	_materialBuff->Unmap(0, nullptr);
	//_materials.clear();
	return true;
}
//�e�N�X�`�����[�h
bool 
PMDActor::LoadTexture() {
	bool ret = true;
	_texBuff.resize(_texturePaths.size());
	for (int i = 0; i < _texturePaths.size();++i) {
		if (_texturePaths[i].texPath != "") {
			ret = _dx->LoadPictureFromFile(WStringFromString(_texturePaths[i].texPath), _texBuff[i].tex);
			if (!ret)return ret;
		}
		if (_texturePaths[i].sphPath != "") {
			ret = _dx->LoadPictureFromFile(WStringFromString(_texturePaths[i].sphPath), _texBuff[i].sph);
			if (!ret)return ret;
		}
		if (_texturePaths[i].spaPath != "") {
			ret = _dx->LoadPictureFromFile(WStringFromString(_texturePaths[i].spaPath), _texBuff[i].spa);
			if (!ret)return ret;
		}
		if (_texturePaths[i].toonPath != "") {
			ret = _dx->LoadPictureFromFile(WStringFromString(_texturePaths[i].toonPath), _texBuff[i].toon);
			if (!ret)return ret;
		}
	}
	return true;
}
//�}�e���A���o�b�t�@�r���[
bool 
PMDActor::CreateMaterialBufferView() {
	auto& materials = _materials;
	auto dev = _dx->Device();
	//�}�e���A���o�b�t�@�r���[�̍쐬
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = materials.size() * 5;//�}�e���A���ƃe�N�X�`����SPH��SPA�ƃg�D�[����5��
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	auto result = dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(_materialHeap.ReleaseAndGetAddressOf()));
	assert(SUCCEEDED(result));
	if (FAILED(result)) {
		return false;
	}
	D3D12_CONSTANT_BUFFER_VIEW_DESC viewDesc = {};
	viewDesc.BufferLocation = _materialBuff->GetGPUVirtualAddress();
	viewDesc.SizeInBytes = AligmentedValue(sizeof(materials[0]), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	auto handle = _materialHeap->GetCPUDescriptorHandleForHeapStart();
	auto heapStride = dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	auto buffStride = AligmentedValue(sizeof(materials[0]), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	for (int i = 0; i < materials.size(); ++i) {
		//�}�e���A���p
		dev->CreateConstantBufferView(&viewDesc, handle);
		handle.ptr += heapStride;

		//�e�N�X�`���p
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		///���ꂼ��̃e�N�X�`���r���[�����
		auto& multiTex = _texBuff[i];
		//�ʏ�e�N�X�`��
		if (multiTex.tex.Get() != nullptr) {
			auto desc = multiTex.tex.Get()->GetDesc();
			srvDesc.Format = desc.Format;
			dev->CreateShaderResourceView(multiTex.tex.Get(), &srvDesc, handle);
		}
		else {
			auto desc = _dx->WhiteTexture()->GetDesc();
			srvDesc.Format = desc.Format;
			dev->CreateShaderResourceView(_dx->WhiteTexture().Get(), &srvDesc, handle);
		}
		handle.ptr += heapStride;
		//SPH�e�N�X�`��
		if (multiTex.sph.Get() != nullptr) {
			auto desc = multiTex.sph->GetDesc();
			srvDesc.Format = desc.Format;
			dev->CreateShaderResourceView(multiTex.sph.Get(), &srvDesc, handle);
		}
		else {
			auto desc = _dx->WhiteTexture()->GetDesc();
			srvDesc.Format = desc.Format;
			dev->CreateShaderResourceView(_dx->WhiteTexture().Get(), &srvDesc, handle);
		}
		handle.ptr += heapStride;
		//SPA�e�N�X�`��
		if (multiTex.spa.Get() != nullptr) {
			auto desc = multiTex.spa->GetDesc();
			srvDesc.Format = desc.Format;
			dev->CreateShaderResourceView(multiTex.spa.Get(), &srvDesc, handle);
		}
		else {
			auto desc = _dx->BlackTexture()->GetDesc();
			srvDesc.Format = desc.Format;
			dev->CreateShaderResourceView(_dx->BlackTexture().Get(), &srvDesc, handle);
		}
		handle.ptr += heapStride;

		//�g�D�[���e�N�X�`��
		if (multiTex.toon.Get() != nullptr) {
			auto desc = multiTex.toon->GetDesc();
			srvDesc.Format = desc.Format;
			dev->CreateShaderResourceView(multiTex.toon.Get(), &srvDesc, handle);
		}
		else {
			auto desc = _dx->GradTexture()->GetDesc();
			srvDesc.Format = desc.Format;
			dev->CreateShaderResourceView(_dx->GradTexture().Get(), &srvDesc, handle);
		}
		handle.ptr += heapStride;

		viewDesc.BufferLocation += buffStride;
	}
	return true;
}


ComPtr<ID3D12Resource> 
PMDActor::GetMaterialBuffer() {
	return _materialBuff;
}

ComPtr<ID3D12DescriptorHeap> 
PMDActor::GetMaterialAndTextureView() {
	return _materialHeap;
}

void 
PMDActor::UpdateMotion(uint32_t frame) {
	//������
	fill(_boneMatrices.begin(), _boneMatrices.end(), XMMatrixIdentity());

	//�܂��̓L�[�t���[�����ƂɃ|�[�W���O���Ă݂悤
	for (auto& boneKeyFrame : _keyframes) {
		auto& keyframes = boneKeyFrame.second;
		auto rit = find_if(keyframes.rbegin(), keyframes.rend(),
			[frame](const KeyFrame& f) {
			return f.frameNo <= frame;
		});
		if (rit == keyframes.rend()) {
			continue;
		}
		
		float t = 0.0f;
		auto it = rit.base();
		XMVECTOR q = XMLoadFloat4(&rit->quaternion);
		XMVECTOR ofst = XMLoadFloat3(&rit->offset);
		XMMATRIX rot;
		if (it != keyframes.end()) {
			t = static_cast<float>(frame - rit->frameNo) /
				static_cast<float>(it->frameNo - rit->frameNo);
			
			t = GetYFromXOnBezier(it->cpnt, t);

			auto q2 = XMLoadFloat4(&it->quaternion);
			q=XMQuaternionSlerp(q, q2, t);
			
			auto ofst2= XMLoadFloat3(&it->offset);
			ofst=XMVectorLerp(ofst, ofst2, t);
		}
		rot = XMMatrixRotationQuaternion(q);

		
		//�Ȃ��{�[���ɂ͉������Ȃ�(�Ȃ��{�[���ŕs���ȃ{�[�������h������)
		auto& boneName = boneKeyFrame.first;
		auto boneIt = _boneTable.find(boneName);
		if (boneIt == _boneTable.end())continue;
		auto& bone = boneIt->second;

		auto& pos = bone.pos;
		rot = XMMatrixTranslation(-pos.x, -pos.y, -pos.z)*
			rot*
			XMMatrixTranslation(pos.x, pos.y, pos.z);//90���Ȃ���
		_boneMatrices[bone.index] = rot *XMMatrixTranslationFromVector(ofst);
	}


	

	auto wm=XMMatrixTranslation(_pos.x, _pos.y, _pos.z);

	auto eyeBoneIdx = _eyeBoneIdxes[0];
	auto eyeEndBoneIdx = _boneTree[eyeBoneIdx][0];
	auto startPos = XMVector3TransformCoord( XMLoadFloat3(&_boneAddressArray[eyeBoneIdx]->pos),_boneMatrices[eyeBoneIdx]);
	auto endPos = XMVector3TransformCoord(XMLoadFloat3(&_boneAddressArray[eyeEndBoneIdx]->pos), _boneMatrices[eyeEndBoneIdx]);

	auto wmstartPos = XMVector3Transform(startPos, wm);

	//�f�t�H���g�̖ڐ����v�Z
	auto eyeVec = XMVector3Normalize(XMVectorSubtract(endPos, wmstartPos));
	//�J�����ւ̖ڐ����v�Z
	auto eyeToCamVec = XMVector3Normalize(XMVectorSubtract(_dx->GetCameraPosition(), wmstartPos));

	auto up = XMFLOAT3(0, 1, 0);
	auto right = XMFLOAT3(1, 0, 0);
	XMVECTOR mz;
	mz.m128_f32[0] = mz.m128_f32[1] = mz.m128_f32[3] = 0.0f;
	mz.m128_f32[2] = -1;
	auto mat = XMMatrixTranslationFromVector(-startPos);
	mat *= LookAtMatrix(mz, eyeToCamVec, up, right);
	mat *= XMMatrixTranslationFromVector(startPos);
	_boneMatrices[eyeBoneIdx]= mat* _boneMatrices[eyeBoneIdx];

	RecursiveBoneTransform(_boneTable["�Z���^�["].index, XMMatrixIdentity());

	copy(_boneMatrices.begin(), _boneMatrices.end(), _mappedBoneMatrix);
}

void 
PMDActor::Update() {
	uint32_t frame = (GetTickCount() - _lastTickCount)/30;
	UpdateMotion(frame);
	if (frame > _duration) {
		_lastTickCount = GetTickCount();
	}


	XMMATRIX worldMat = XMMatrixRotationRollPitchYaw(_rotator.x, _rotator.y, _rotator.z)*
		XMMatrixTranslation(_pos.x, _pos.y, _pos.z);

	*_mappedTransform = worldMat;
}

bool 
PMDActor::CreateTransformBufferView() {
	auto dev = _dx->Device();

	//�萔�o�b�t�@�r���[�̍쐬
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 2;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	auto result = dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(_transformHeap.ReleaseAndGetAddressOf()));
	if (!CheckResult(result)) {
		return false;
	}

	auto handle = _transformHeap->GetCPUDescriptorHandleForHeapStart();
	
	//���[���h�ϊ��s��p
	D3D12_CONSTANT_BUFFER_VIEW_DESC viewDesc = {};
	viewDesc.BufferLocation = _transformCB->GetGPUVirtualAddress();
	viewDesc.SizeInBytes = _transformCB->GetDesc().Width;
	dev->CreateConstantBufferView(&viewDesc, handle);

	//�{�[���s��p
	viewDesc.BufferLocation = _bonesBuff->GetGPUVirtualAddress();
	viewDesc.SizeInBytes = _bonesBuff->GetDesc().Width;
	handle.ptr += _dx->Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	dev->CreateConstantBufferView(&viewDesc, handle);

	return true;
}

void 
PMDActor::Draw(bool isShadow) {
	auto cmdlist = _dx->CmdList();

	cmdlist->IASetVertexBuffers(0, 1, &_vbView);
	cmdlist->IASetIndexBuffer(&_ibView);
	cmdlist->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	//�A�N�^�[���W�ϊ�
	ID3D12DescriptorHeap* heaps[] = {_transformHeap.Get()};
	
	cmdlist->SetDescriptorHeaps(1, heaps);
	auto actorHeapAddress = _transformHeap->GetGPUDescriptorHandleForHeapStart();
	cmdlist->SetGraphicsRootDescriptorTable(2, actorHeapAddress);

	auto matHeapAddress = _materialHeap->GetGPUDescriptorHandleForHeapStart();
	heaps[0] = _materialHeap.Get();
	cmdlist->SetDescriptorHeaps(1, heaps);
	auto incSize = _dx->Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	uint32_t indexOffset = 0;//���[�v�O�ɕϐ���p�ӂ��Ƃ�
	if (isShadow) {
		cmdlist->DrawIndexedInstanced(_indexNum, 1, 0, 0, 0);
	}
	else {
		for (int i = 0; i < _materials.size(); ++i) {
			cmdlist->SetGraphicsRootDescriptorTable(0, matHeapAddress);
			auto& material = _materials[i];
			cmdlist->DrawIndexedInstanced(material.indicesNum,
				1,//�{�̂Ɖe
				indexOffset, 0, 0);
			indexOffset += material.indicesNum;
			matHeapAddress.ptr += incSize * 5;//�}�e���A���ƃe�N�X�`����SPH�Ԃ�
		}
	}
}

bool
PMDActor::CreateTransformBuffer() {
	D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

	
	auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(
		AligmentedValue(sizeof(XMMATRIX), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)
	);
	auto result = _dx->Device()->CreateCommittedResource(&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_transformCB.ReleaseAndGetAddressOf()));

	if (!CheckResult(result)) {
		return false;
	}

	auto worldMat = XMMatrixIdentity();
	result = _transformCB->Map(0, nullptr, (void**)&_mappedTransform);
	*_mappedTransform = worldMat;

	return true;

}

void 
PMDActor::StartAmimation() {
	_lastTickCount = GetTickCount();
}