#pragma once

#include<d3d12.h>
#include<DirectXMath.h>
#include<vector>
#include<map>
#include<string>
#include<unordered_map>
#include<wrl.h>

class Dx12Wrapper;
class PMDRenderer;
class PMDActor
{
	friend PMDRenderer;
private:
	unsigned int _duration = 0;
	PMDRenderer& _renderer;
	Dx12Wrapper& _dx12;
	DirectX::XMMATRIX _localMat;
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;
	
	//���_�֘A
	ComPtr<ID3D12Resource> _vb = nullptr;
	ComPtr<ID3D12Resource> _ib = nullptr;
	D3D12_VERTEX_BUFFER_VIEW _vbView = {};
	D3D12_INDEX_BUFFER_VIEW _ibView = {};

	ComPtr<ID3D12Resource> _transformMat = nullptr;//���W�ϊ��s��(���̓��[���h�̂�)
	ComPtr<ID3D12DescriptorHeap> _transformHeap = nullptr;//���W�ϊ��q�[�v

	//�V�F�[�_���ɓ�������}�e���A���f�[�^
	struct MaterialForHlsl {
		DirectX::XMFLOAT3 diffuse; //�f�B�t���[�Y�F
		float alpha; // �f�B�t���[�Y��
		DirectX::XMFLOAT3 specular; //�X�y�L�����F
		float specularity;//�X�y�L�����̋���(��Z�l)
		DirectX::XMFLOAT3 ambient; //�A���r�G���g�F
	};
	//����ȊO�̃}�e���A���f�[�^
	struct AdditionalMaterial {
		std::string texPath;//�e�N�X�`���t�@�C���p�X
		int toonIdx; //�g�D�[���ԍ�
		bool edgeFlg;//�}�e���A�����̗֊s���t���O
	};
	//�܂Ƃ߂�����
	struct Material {
		unsigned int indicesNum;//�C���f�b�N�X��
		MaterialForHlsl material;
		AdditionalMaterial additional;
	};

	struct Transform {
		//�����Ɏ����Ă�XMMATRIX�����o��16�o�C�g�A���C�����g�ł��邽��
		//Transform��new����ۂɂ�16�o�C�g���E�Ɋm�ۂ���
		void* operator new(size_t size);
		DirectX::XMMATRIX world;
	};

	Transform _transform;
	DirectX::XMMATRIX* _mappedMatrices = nullptr;
	ComPtr<ID3D12Resource> _transformBuff = nullptr;

	//�}�e���A���֘A
	std::vector<Material> _materials;
	ComPtr<ID3D12Resource> _materialBuff = nullptr;
	std::vector<ComPtr<ID3D12Resource>> _textureResources;
	std::vector<ComPtr<ID3D12Resource>> _sphResources;
	std::vector<ComPtr<ID3D12Resource>> _spaResources;
	std::vector<ComPtr<ID3D12Resource>> _toonResources;

	//�{�[���֘A
	std::vector<DirectX::XMMATRIX> _boneMatrices;

	struct BoneNode {
		uint32_t boneIdx;//�{�[���C���f�b�N�X
		uint32_t boneType;//�{�[�����
		uint32_t parentBone;
		uint32_t ikParentBone;//IK�e�{�[��
		DirectX::XMFLOAT3 startPos;//�{�[����_(��]���S)
		std::vector<BoneNode*> children;//�q�m�[�h
	};
	std::unordered_map<std::string, BoneNode> _boneNodeTable;
	std::vector<std::string> _boneNameArray;//�C���f�b�N�X���疼�O���������₷���悤�ɂ��Ă���
	std::vector<BoneNode*> _boneNodeAddressArray;//�C���f�b�N�X����m�[�h���������₷���悤�ɂ��Ă���


	struct PMDIK {
		uint16_t boneIdx;//IK�Ώۂ̃{�[��������
		uint16_t targetIdx;//�^�[�Q�b�g�ɋ߂Â��邽�߂̃{�[���̃C���f�b�N�X
		uint16_t iterations;//���s��
		float limit;//��񓖂���̉�]����
		std::vector<uint16_t> nodeIdxes;//�Ԃ̃m�[�h�ԍ�
	};
	std::vector<PMDIK> _ikData;
	
	//�ǂݍ��񂾃}�e���A�������ƂɃ}�e���A���o�b�t�@���쐬
	HRESULT CreateMaterialData();
	
	ComPtr< ID3D12DescriptorHeap> _materialHeap = nullptr;//�}�e���A���q�[�v(5�Ԃ�)
	//�}�e���A�����e�N�X�`���̃r���[���쐬
	HRESULT CreateMaterialAndTextureView();

	//���W�ϊ��p�r���[�̐���
	HRESULT CreateTransformView();

	//PMD�t�@�C���̃��[�h
	HRESULT LoadPMDFile(const char* path);
	void RecursiveMatrixMultipy(BoneNode* node, const DirectX::XMMATRIX& mat,bool flg=false);
	float _angle;//�e�X�g�pY����]


	///�L�[�t���[���\����
	struct KeyFrame {
		unsigned int frameNo;//�t���[����(�A�j���[�V�����J�n����̌o�ߎ���)
		DirectX::XMVECTOR quaternion;//�N�H�[�^�j�I��
		DirectX::XMFLOAT3 offset;//IK�̏������W����̃I�t�Z�b�g���
		DirectX::XMFLOAT2 p1, p2;//�x�W�F�̒��ԃR���g���[���|�C���g
		KeyFrame(
			unsigned int fno, 
			const DirectX::XMVECTOR& q,
			const DirectX::XMFLOAT3& ofst,
			const DirectX::XMFLOAT2& ip1,
			const DirectX::XMFLOAT2& ip2):
			frameNo(fno),
			quaternion(q),
			offset(ofst),
			p1(ip1),
			p2(ip2){}
	};
	std::unordered_map<std::string, std::vector<KeyFrame>> _motiondata;

	float GetYFromXOnBezier(float x,const DirectX::XMFLOAT2& a,const DirectX::XMFLOAT2& b, uint8_t n = 12);
	
	std::vector<uint32_t> _kneeIdxes;

	DWORD _startTime;//�A�j���[�V�����J�n���_�̃~���b����
	
	void MotionUpdate();

	///CCD-IK�ɂ��{�[������������
	///@param ik �Ώ�IK�I�u�W�F�N�g
	void SolveCCDIK(const PMDIK& ik);

	///�]���藝IK�ɂ��{�[������������
	///@param ik �Ώ�IK�I�u�W�F�N�g
	void SolveCosineIK(const PMDIK& ik);
	
	///LookAt�s��ɂ��{�[������������
	///@param ik �Ώ�IK�I�u�W�F�N�g
	void SolveLookAt(const PMDIK& ik);

	void IKSolve(int frameNo);

	//IK�I���I�t�f�[�^
	struct VMDIKEnable {
		uint32_t frameNo;
		std::unordered_map<std::string, bool> ikEnableTable;
	};
	std::vector<VMDIKEnable> _ikEnableData;

public:
	PMDActor(const char* filepath,PMDRenderer& renderer);
	~PMDActor();
	///�N���[���͒��_����у}�e���A���͋��ʂ̃o�b�t�@������悤�ɂ���
	PMDActor* Clone();
	void LoadVMDFile(const char* filepath, const char* name);
	void Update();
	void Draw();
	void PlayAnimation();

	void LookAt(float x,float y, float z);
};

