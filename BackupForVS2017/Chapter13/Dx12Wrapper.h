#pragma once
#include<Windows.h>
#include<d3d12.h>
#include<dxgi1_6.h>
#include<DirectXTex.h>
#include<vector>
#include<DirectXMath.h>
#include<wrl.h>
#include<unordered_map>
#include<memory>

class PMDActor;
class PMDRenderer;
using Microsoft::WRL::ComPtr;
///DirectX12�̊e�v�f�Ƃ��֐���
///���b�v���Ă邾���̃N���X
class Dx12Wrapper
{
private:
	struct MultiTexture{
		ComPtr<ID3D12Resource> texBuff;//�ʏ�e�N�X�`���o�b�t�@
		ComPtr<ID3D12Resource> sphBuff;//SPH�e�N�X�`��
		ComPtr<ID3D12Resource> spaBuff;//SPA�e�N�X�`��
		ComPtr<ID3D12Resource> toonBuff;//�g�D�[���e�N�X�`��
	};

	HWND _hwnd;

	//��{�I�ȓz(DXGI)
	ComPtr < IDXGIFactory4> _dxgiFactory;
	ComPtr < IDXGISwapChain4> _swapchain;

	//��{�I�ȓz(�f�o�C�X)
	ComPtr < ID3D12Device> _dev;


	//�R�}���h�L���[(�R�}���h���s�̒P��)
	ComPtr < ID3D12CommandQueue> _cmdQue;

	//�[�x�o�b�t�@�p�o�b�t�@
	ComPtr<ID3D12Resource> _depthBuffer;
	//�[�x�o�b�t�@�r���[�p�X�N���v�^�q�[�v
	ComPtr<ID3D12DescriptorHeap> _dsvHeap;

	bool CreateDepthBuffer();
	bool CreateDSV();

	//�����_�[�^�[�Q�b�g�r���[�p�f�X�N���v�^�q�[�v
	ComPtr<ID3D12DescriptorHeap> _rtvDescHeap;
	//�X���b�v�`�F�C���������Ă��郊�\�[�X�ւ̃|�C���^
	std::vector<ID3D12Resource*> _backBuffers;

	//�R�}���h���X�g���i�[���邽�߂̃������̈�
	ComPtr <ID3D12CommandAllocator> _cmdAlloc = nullptr;
	//�R�}���h���X�g�{��(�R�}���h�A���P�[�^�ɖ��߂�o�^���邽�߂�
	//�C���^�[�t�F�C�X)
	ComPtr<ID3D12GraphicsCommandList> _cmdList = nullptr;
	
	//�҂��̂��߂̃t�F���X
	ComPtr<ID3D12Fence> _fence;
	UINT64 _fenceValue;

	//���ʓǂ݂��Ȃ��悤�Ƀe�N�X�`���̃e�[�u��������Ă���
	std::unordered_map<std::wstring, ComPtr<ID3D12Resource>> _textureTable;

	bool CreateTextureFromImageData(const DirectX::Image* img, ComPtr<ID3D12Resource>& buff,bool isDiscrete=false);

	ComPtr<ID3D12Resource> _whiteTex;//���e�N�X�`��
	///��Z�p�̐^�����e�N�X�`��(��Z����Ă��e�����o�Ȃ�)�����܂��B
	bool CreateWhiteTexture();

	ComPtr<ID3D12Resource> _blackTex;//���e�N�X�`��
	///���Z�p�̐^�����e�N�X�`��(���Z����Ă��e�����o�Ȃ�)�����܂��B
	bool CreateBlackTexture();

	ComPtr<ID3D12Resource> _gradTex;//�O���[�O���f�[�V�����e�N�X�`��
	//�g�D�[�����Ȃ��ꍇ�̊K�����`����
	bool CreateGradationTexture();

	ComPtr < ID3D12Resource> _sceneCB;//���W�ϊ��萔�o�b�t�@
	ComPtr < ID3D12DescriptorHeap> _sceneHeap;//���W�ϊ�CBV�q�[�v
	///���W�ϊ��p�萔�o�b�t�@����ђ萔�o�b�t�@�r���[���쐬����
	bool CreateTransformConstantBuffer();
	bool CreateTransformBufferView();

	struct SceneMatrix {
		DirectX::XMMATRIX view;//�r���[
		DirectX::XMMATRIX proj;//�v���W�F�N�V����
		DirectX::XMMATRIX shadow;//�e
		DirectX::XMFLOAT3 eye;//���_
	};
	SceneMatrix* _mappedScene;

	//���_(�J�����̈ʒu)XMVECTOR
	//�����_(����Ώۂ̈ʒu)XMVECTOR
	//��x�N�g��(��)XMVECTOR
	DirectX::XMFLOAT3 _eye;
	DirectX::XMFLOAT3 _target;
	DirectX::XMFLOAT3 _up;
	//���s���C�g�̌���
	DirectX::XMFLOAT3 _parallelLightVec;

	float _fov = DirectX::XM_PI/6;//�f�t�H���g30��

	bool CreateCommandList();
	void Barrier(ID3D12Resource* p,
		D3D12_RESOURCE_STATES before, 
		D3D12_RESOURCE_STATES after);

	//�c�݃e�N�X�`���p
	ComPtr<ID3D12DescriptorHeap> _distortionSRVHeap;
	ComPtr<ID3D12Resource> _distortionTexBuffer;
	bool CreateEffectBufferAndView();


	//1���ڃ����_�����O�p
	//������y���|���ɒ���t���邽�߂̊G��
	//���������\�[�X�Ƃ��̃r���[
	ComPtr<ID3D12DescriptorHeap> _peraRTVHeap;
	ComPtr<ID3D12DescriptorHeap> _peraRegisterHeap;
	ComPtr<ID3D12Resource> _peraResource;
	//�P���ڃy���|���̂��߂̃��\�[�X�ƃr���[��
	//�쐬
	bool CreatePeraResourcesAndView();

	ComPtr<ID3D12Resource> _bokehParamResource;
	//�{�P�Ɋւ���o�b�t�@����蒆�Ƀ{�P�p�����[�^��������
	bool CreateBokehParamResource();

	//�y���|��2����
	ComPtr<ID3D12Resource> _peraResource2;
	ComPtr<ID3D12PipelineState> _peraPipeline2;

	//�y���|���p���_�o�b�t�@(N����4�_)
	ComPtr<ID3D12Resource> _peraVB;
	D3D12_VERTEX_BUFFER_VIEW _peraVBV;

	//�y���|���p�p�C�v���C�������[�g�V�O�l�`��
	ComPtr<ID3D12PipelineState> _peraPipeline;
	ComPtr<ID3D12RootSignature> _peraRS;

	bool CreatePeraVertex();
	bool CreatePeraPipeline();

public:
	Dx12Wrapper(HWND hwnd);
	~Dx12Wrapper();

	ID3D12Device* Device() {
		return _dev.Get();
	}
	ID3D12GraphicsCommandList* CmdList() {
		return _cmdList.Get();
	}

	bool Init();
	bool CreateRenderTargetView();

	ComPtr<ID3D12Resource> WhiteTexture();
	ComPtr<ID3D12Resource> BlackTexture();
	ComPtr<ID3D12Resource> GradTexture();


	//�y���|���S���ւ̕`�揀��
	bool PreDrawToPera1();
	//�y���|���S���ւ̕`��㏈��
	void PostDrawToPera1();

	//�y���|���S���ւ̕`��
	void DrawToPera1(std::shared_ptr<PMDRenderer> renderer);

	//��ʂ̃N���A
	bool Clear();

	//�`��
	void Draw(std::shared_ptr<PMDRenderer> renderer);

	void DrawHorizontalBokeh();

	void SetCameraSetting();

	//�t���b�v
	void Flip();
	void WaitForCommandQueue();

	bool LoadPictureFromFile(std::wstring filepath, ComPtr<ID3D12Resource>& buff);

	void SetFov(float angle);
	void SetEyePosition(float x, float y, float z);
	void MoveEyePosition(float x, float y, float z);

	DirectX::XMVECTOR GetCameraPosition();

};

