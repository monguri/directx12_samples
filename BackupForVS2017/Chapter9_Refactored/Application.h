#pragma once
#include<Windows.h>
#include<tchar.h>
#include<d3d12.h>
#include<dxgi1_6.h>
#include<DirectXMath.h>
#include<vector>
#include<map>
#include<d3dcompiler.h>
#include<DirectXTex.h>
#include<d3dx12.h>
#include<wrl.h>

///�V���O���g���N���X
class Application
{
private:
	//�����ɕK�v�ȕϐ�(�o�b�t�@��q�[�v�Ȃ�)������
	//�E�B���h�E����
	WNDCLASSEX _windowClass;
	HWND _hwnd;
	//DXGI�܂��
	Microsoft::WRL::ComPtr < IDXGIFactory6> _dxgiFactory = nullptr;//DXGI�C���^�[�t�F�C�X
	Microsoft::WRL::ComPtr < IDXGISwapChain4> _swapchain = nullptr;//�X���b�v�`�F�C��

	//DirectX12�܂��
	Microsoft::WRL::ComPtr< ID3D12Device> _dev = nullptr;//�f�o�C�X
	Microsoft::WRL::ComPtr < ID3D12CommandAllocator> _cmdAllocator = nullptr;//�R�}���h�A���P�[�^
	Microsoft::WRL::ComPtr < ID3D12GraphicsCommandList> _cmdList = nullptr;//�R�}���h���X�g
	Microsoft::WRL::ComPtr < ID3D12CommandQueue> _cmdQueue = nullptr;//�R�}���h�L���[

	//�K�v�Œ���̃o�b�t�@�܂��
	Microsoft::WRL::ComPtr<ID3D12Resource> _depthBuffer = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> _vertBuff = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> _idxBuff = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> _constBuff = nullptr;

	//���[�h�p�e�[�u��
	using LoadLambda_t = std::function<HRESULT(const std::wstring& path, DirectX::TexMetadata*, DirectX::ScratchImage&)>;
	std::map < std::string, LoadLambda_t> _loadLambdaTable;

	//�}�e���A������
	unsigned int _materialNum;//�}�e���A����
	Microsoft::WRL::ComPtr<ID3D12Resource> _materialBuff = nullptr;
	D3D12_CONSTANT_BUFFER_VIEW_DESC matCBVDesc = {};

	//�f�t�H���g�̃e�N�X�`��(���A���A�O���C�X�P�[���O���f�[�V����)
	Microsoft::WRL::ComPtr<ID3D12Resource> _whiteTex = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> _blackTex = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> _gradTex = nullptr;

	//���W�ϊ��n�s��
	DirectX::XMMATRIX _worldMat;
	DirectX::XMMATRIX _viewMat;
	DirectX::XMMATRIX _projMat;

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
	std::vector<Material> _materials;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> _textureResources;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> _sphResources;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> _spaResources;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> _toonResources;

	//�V�F�[�_���ɓn�����߂̊�{�I�Ȋ��f�[�^
	struct SceneData {
		DirectX::XMMATRIX world;//���[���h�s��
		DirectX::XMMATRIX view;//�r���[�v���W�F�N�V�����s��
		DirectX::XMMATRIX proj;//
		DirectX::XMFLOAT3 eye;//���_���W
	};
	SceneData* _mapScene;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _basicDescHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _materialDescHeap = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Fence> _fence = nullptr;
	UINT64 _fenceVal = 0;

	//���_���C���f�b�N�X�o�b�t�@�r���[
	D3D12_VERTEX_BUFFER_VIEW _vbView = {};
	D3D12_INDEX_BUFFER_VIEW _ibView = {};

	//�t�@�C�����p�X�ƃ��\�[�X�̃}�b�v�e�[�u��
	std::map<std::string, ID3D12Resource*> _resourceTable;

	//�p�C�v���C�������[�g�V�O�l�`��
	Microsoft::WRL::ComPtr<ID3D12PipelineState> _pipelinestate = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> _rootsignature = nullptr;

	std::vector<ID3D12Resource*> _backBuffers;//�o�b�N�o�b�t�@
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _rtvHeaps = nullptr;//�����_�[�^�[�Q�b�g�p�f�X�N���v�^�q�[�v
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _dsvHeap = nullptr;//�[�x�o�b�t�@�r���[�p�f�X�N���v�^�q�[�v
	CD3DX12_VIEWPORT _viewport;//�r���[�|�[�g
	CD3DX12_RECT _scissorrect;//�V�U�[��`

	//�e�N�X�`���o�b�t�@����
	ID3D12Resource* CreateWhiteTexture();//���e�N�X�`���̐���
	ID3D12Resource*	CreateBlackTexture();//���e�N�X�`���̐���
	ID3D12Resource*	CreateGrayGradationTexture();//�O���[�e�N�X�`���̐���
	ID3D12Resource*	LoadTextureFromFile(std::string& texPath);//�w��e�N�X�`���̃��[�h

	//�ŏI�I�ȃ����_�[�^�[�Q�b�g�̐���
	HRESULT	CreateFinalRenderTarget(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& rtvHeaps, std::vector<ID3D12Resource *>& backBuffers);

	//�X���b�v�`�F�C���̐���
	HRESULT CreateSwapChain(const HWND &hwnd, Microsoft::WRL::ComPtr<IDXGIFactory6> dxgiFactory);

	//�Q�[���p�E�B���h�E�̐���
	void CreateGameWindow(HWND &hwnd, WNDCLASSEX &windowClass);

	//DXGI�܂�菉����
	HRESULT InitializeDXGIDevice();

	//�R�}���h�܂�菉����
	HRESULT InitializeCommand();

	//�p�C�v���C��������
	HRESULT CreateBasicGraphicsPipeline();
	//���[�g�V�O�l�`��������
	HRESULT CreateRootSignature();

	//�e�N�X�`�����[�_�e�[�u���̍쐬
	void CreateTextureLoaderTable();

	//�f�v�X�X�e���V���r���[�̐���
	HRESULT CreateDepthStencilView();

	//PMD�t�@�C���̃��[�h
	HRESULT LoadPMDFile(const char* path);

	//GPU���̃}�e���A���f�[�^�̍쐬
	HRESULT CreateMaterialData();

	//���W�ϊ��p�r���[�̐���
	HRESULT CreateSceneTransformView();

	//�}�e���A�����e�N�X�`���̃r���[���쐬
	void CreateMaterialAndTextureView();

	//���V���O���g���̂��߂ɃR���X�g���N�^��private��
	//����ɃR�s�[�Ƒ�����֎~��
	Application();
	Application(const Application&) = delete;
	void operator=(const Application&) = delete;
public:
	///Application�̃V���O���g���C���X�^���X�𓾂�
	static Application& Instance();

	///������
	bool Init();

	///���[�v�N��
	void Run();

	///�㏈��
	void Terminate();

	~Application();
};

