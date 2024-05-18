#pragma once

#include<vector>
#include<array>
#include<string>
#include<DirectXMath.h>
#include<d3d12.h>
#include<wrl.h>
#include<map>
#include<unordered_map>
#include<memory>
#include<algorithm>

using Microsoft::WRL::ComPtr;
class Dx12Wrapper;
struct Material{
	DirectX::XMFLOAT4 diffuse;//�f�B�t���[�Y�F
	float power;//�X�y�L��������
	DirectX::XMFLOAT3 specular;//�X�y�L�����F
	DirectX::XMFLOAT3 ambient;//���F
	uint32_t indicesNum;//�C���f�b�N�X��
};

struct MultiTexturePath {
	std::string texPath;//�ʏ�e�N�X�`���p�X
	std::string sphPath;//��Z�e�N�X�`���p�X
	std::string spaPath;//���Z�e�N�X�`���p�X

	std::string toonPath;//�g�D�[���e�N�X�`���p�X

};

class PMDActor
{
private:
	//���[�V�������
	struct KeyFrame {
		uint32_t frameNo;//�L�[�t���[��������o�߃t���[����
		DirectX::XMFLOAT4 quaternion;//���̂Ƃ��ǂꂭ�炢��]������̂�
		DirectX::XMFLOAT3 offset;//���̈ʒu����̃I�t�Z�b�g
		std::array<DirectX::XMFLOAT2,2> cpnt;//�R���g���[���|�C���g
		KeyFrame() {}
		KeyFrame(uint32_t fno, DirectX::XMFLOAT4& q,DirectX::XMFLOAT3& ofst,float cx1,float cy1,float cx2,float cy2) :frameNo(fno), 
			quaternion(q),
			offset(ofst){
			cpnt[0].x = cx1;
			cpnt[0].y = cy1;
			cpnt[1].x = cx2;
			cpnt[1].y = cy2;
		}
	};
	std::unordered_map<std::string, std::vector<KeyFrame>> _keyframes;
	uint32_t _duration;//�A�j���[�V�����̑��t���[����

	std::vector<uint32_t> _eyeBoneIdxes;

	//���[�h���̏������I����ăA�j���[�V�������J�n�������_�ł�
	//TickCount��PC�N��������̃~���b
	uint32_t _lastTickCount;

	//���݂̌o�߃t���[�����ɏ]����
	//�{�[���s����X�V����
	void UpdateMotion(uint32_t frame);

	//�{�[���̏��
	struct BoneInfo {
		int index;//�����̃C���f�b�N�X
		DirectX::XMFLOAT3 pos;//�{�[�����S���W
		BoneInfo(int idx, DirectX::XMFLOAT3& inpos) :index(idx), pos(inpos) {}
		BoneInfo() :index(0), pos(DirectX::XMFLOAT3()) {}
	};
	std::vector<BoneInfo*> _boneAddressArray;
	std::map<std::string, BoneInfo> _boneTable;
	std::vector<DirectX::XMMATRIX> _boneMatrices;//�ŏI�I�ɃO���{�ɓn���f�[�^
	std::vector< std::vector<int> > _boneTree;//�{�[���c���[
	ComPtr<ID3D12Resource> _bonesBuff;//�{�[���z��p�o�b�t�@
	DirectX::XMMATRIX* _mappedBoneMatrix;
	bool CreateBoneBuffer();

	DirectX::XMFLOAT3 _rotator;
	DirectX::XMFLOAT3 _pos;

	unsigned int _vertNum;
	unsigned int _indexNum;

	std::vector<uint8_t> _vertexData;// ���_�f�[�^
	std::vector<uint16_t> _indexData;// �C���f�b�N�X�f�[�^
	std::vector<Material> _materials;// �}�e���A���f�[�^
	std::vector<MultiTexturePath> _texturePaths;//�e�N�X�`���̑��΃p�X
	bool LoadFromPMD(const char* filepath);

	ComPtr<ID3D12Resource> _vertexBuff;//���_�o�b�t�@
	ComPtr<ID3D12Resource> _indexBuff;//�C���f�b�N�X�o�b�t�@
	ComPtr<ID3D12Resource> _materialBuff;//�}�e���A���o�b�t�@
	struct CompositeTexture {
		ComPtr<ID3D12Resource> tex;//�ʏ�
		ComPtr<ID3D12Resource> sph;//��Z�X�t�B�A�}�b�v
		ComPtr<ID3D12Resource> spa;//���Z�X�t�B�A�}�b�v
		ComPtr<ID3D12Resource> toon;//�g�D�[��
	};
	std::vector<CompositeTexture> _texBuff;//�e�N�X�`���o�b�t�@(�ʏ�/SPH/SPA/TOON)

	//�r���[
	D3D12_VERTEX_BUFFER_VIEW _vbView;//���_�o�b�t�@
	D3D12_INDEX_BUFFER_VIEW _ibView;//�C���f�b�N�X�o�b�t�@
	ComPtr<ID3D12DescriptorHeap> _materialHeap;//�}�e���A���ЂƂ܂Ƃ�(�e�N�X�`�����܂�)

	//�o�b�t�@���֐�
	bool CreateVertexBufferAndView();
	bool CreateIndexBufferAndView();
	bool CreateMaterialBuffer();
	//�e�N�X�`�����[�h
	bool LoadTexture();
	//�}�e���A���o�b�t�@�r���[
	bool CreateMaterialBufferView();
	std::shared_ptr<Dx12Wrapper> _dx;

	
	ComPtr < ID3D12Resource> _transformCB;//�v���C���[�ړ��萔�o�b�t�@
	ComPtr < ID3D12DescriptorHeap> _transformHeap;//���W�ϊ�CBV�q�[�v
	bool CreateTransformBuffer();
	bool CreateTransformBufferView();

	DirectX::XMMATRIX* _mappedTransform;//


	void RecursiveBoneTransform(int idx,const DirectX::XMMATRIX& mat);

public:
	PMDActor(std::shared_ptr<Dx12Wrapper> dx, const char* path);
	~PMDActor();

	void LoadVMDData(const char* vmdpath);

	const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView()const;
	const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView()const;

	ComPtr<ID3D12Resource> GetMaterialBuffer();
	unsigned int GetMaterialNum()const;
	ComPtr<ID3D12DescriptorHeap> GetMaterialAndTextureView();

	ComPtr<ID3D12Resource> GetTransformBuffer();
	ComPtr<ID3D12DescriptorHeap> GetTransformBufferView();

	std::vector<uint8_t>& GetVertexData();
	unsigned int GetVertexNum()const;

	std::vector<uint16_t>& GetIndexData();
	unsigned int GetIndexNum()const;

	std::vector<Material>& Materials();

	std::vector<MultiTexturePath>& GetTexturePaths();

	void Move(float x, float y, float z);
	void Rotate(float x, float y, float z);

	const DirectX::XMFLOAT3& GetPosition()const;
	const DirectX::XMFLOAT3& GetRotate()const;
	void Update();
	void Draw();
	void StartAmimation();

};