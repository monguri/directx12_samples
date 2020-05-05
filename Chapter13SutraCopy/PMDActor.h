#pragma once
#include <vector>
#include <map>
#include <unordered_map>
#include <wrl.h>
#include <DirectXMath.h>
#include <d3d12.h>
#include <d3dx12.h>

class PMDActor
{
public:
	PMDActor(class Dx12Wrapper& dx12, const std::string& modelPath);
	HRESULT LoadVMDFile(const std::string& path);
	void Move(float x, float y, float z);
	void StartAnimation();
	void Update();
	void Draw(bool isShadow);

private:
	class Dx12Wrapper& _dx12;

	DirectX::XMFLOAT3 _pos = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
	unsigned int _indexNum = 0;

	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	DirectX::XMMATRIX* _mappedMatrices = nullptr;
	ComPtr<ID3D12DescriptorHeap> _transformDescHeap = nullptr;

	// �o�b�t�@�͕`��ɗp����̂ŕێ���������K�v������
	ComPtr<ID3D12Resource> _vertBuff = nullptr;
	ComPtr<ID3D12Resource> _idxBuff = nullptr;
	ComPtr<ID3D12Resource> _materialBuff = nullptr;
	ComPtr<ID3D12Resource> _transformBuff = nullptr;

	std::vector<ComPtr<ID3D12Resource>> _textureResources;
	std::vector<ComPtr<ID3D12Resource>> _sphResources;
	std::vector<ComPtr<ID3D12Resource>> _spaResources;
	std::vector<ComPtr<ID3D12Resource>> _toonResources;

	// �V�F�[�_�ɓn�����߂ɕK�v�Ȃ��̂����I�������f�[�^
	struct MaterialForHlsl
	{
		DirectX::XMFLOAT3 diffuse;
		float alpha;
		DirectX::XMFLOAT3 specular;
		float specularity;
		DirectX::XMFLOAT3 ambient;
	};

	// PMDMaterial�̂����AMaterialForHlsl�ȊO�̃}�e���A�����������Ă���
	// ���߂̃f�[�^
	struct AdditionalMaterial
	{
		std::string texPath;
		int toonIdx;
		bool edgeFlg;
	};

	// MaterialForHlsl��AdditionalMaterial���܂Ƃ߂�����
	struct Material
	{
		unsigned int indicesNum;
		MaterialForHlsl material;
		AdditionalMaterial additional;
	};

	std::vector<Material> _materials;

	ComPtr<ID3D12DescriptorHeap> _materialDescHeap = nullptr;

	D3D12_VERTEX_BUFFER_VIEW _vbView;
	D3D12_INDEX_BUFFER_VIEW _ibView;

	struct BoneNode
	{
		uint32_t boneIdx; // �{�[���C���f�b�N�X
		uint32_t boneType; // �{�[�����
		uint32_t parentBone; // �e�{�[��
		uint32_t ikParentBone; // IK�e�{�[��
		DirectX::XMFLOAT3 startPos; // �{�[����_�i��]���S�j
		std::vector<BoneNode*> children; // �q�{�[���̃m�[�h
	};

	std::map<std::string, BoneNode> _boneNodeTable;
	std::vector<std::string> _boneNameArray;
	std::vector<BoneNode*> _boneNodeAddressArray; // �{�ł̓|�C���^�ɂ��Ă邯�ǎQ�Ƃł��������

	std::vector<DirectX::XMMATRIX> _boneMatrices;

	struct PMDIK
	{
		uint16_t boneIdx;
		uint16_t targetIdx;
		// uint8_ chainLen; // �A���C�������g�̖�肪�o��
		uint16_t iterations;
		float limit;
		std::vector<uint16_t> nodeIdxes; // chainLen�̗v�f��
	};

	std::vector<PMDIK> _ikData;

	struct KeyFrame {
		unsigned int frameNo;
		DirectX::XMVECTOR quaternion;
		DirectX::XMFLOAT3 offset;
		DirectX::XMFLOAT2 p1, p2;

		KeyFrame(unsigned int fno, const DirectX::XMVECTOR& q, const DirectX::XMFLOAT3& ofst, const DirectX::XMFLOAT2& cp1, const DirectX::XMFLOAT2& cp2) : frameNo(fno), quaternion(q), offset(ofst), p1(cp1), p2(cp2) {}
	};

	std::unordered_map<std::string, std::vector<KeyFrame>> _motiondata;

	std::vector<uint32_t> _kneeIdxes;

	DWORD _startTime = 0;
	unsigned int _duration = 0;

	HRESULT LoadPMDFileAndCreateMeshBuffers(const std::string& path);
	void RecursiveMatrixMultiply(const BoneNode& node, const DirectX::XMMATRIX& mat);
	void UpdateMotion();
	void SolveCCDIK(const PMDIK& ik);
	void SolveCosineIK(const PMDIK& ik);
	void SolveLookAt(const PMDIK& ik);
	void IKSolve();
	HRESULT CreateTransformConstantBuffer();
	HRESULT CreateMaterialBuffers();
};

