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
#include<array>

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
	//�V���h�E�}�b�v�p�[�x�o�b�t�@
	ComPtr<ID3D12Resource> _lightDepthBuffer;
	

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
		DirectX::XMMATRIX lightCamera;//���C�g���猩���r���[
		DirectX::XMMATRIX shadow;//�e�s��
		DirectX::XMFLOAT3 eye;//���_
	};
	SceneMatrix* _mappedScene;

	//���_(�J�����̈ʒu)XMVECTOR
	//�����_(����Ώۂ̈ʒu)XMVECTOR
	//��x�N�g��(��)XMVECTOR
	DirectX::XMFLOAT3 _eye;
	DirectX::XMFLOAT3 _target;
	DirectX::XMFLOAT3 _up;
	float _fov = DirectX::XM_PI/6;

	bool CreateCommandList();
	void Barrier(ID3D12Resource* p,
		D3D12_RESOURCE_STATES before, 
		D3D12_RESOURCE_STATES after);

	//std::vector<PMDActor*> _actors;

	//1���ڃ����_�����O�p
	//������y���|���ɒ���t���邽�߂̊G��
	//���������\�[�X�Ƃ��̃r���[
	ComPtr<ID3D12DescriptorHeap> _peraRTVHeap;
	ComPtr<ID3D12DescriptorHeap> _peraSRVHeap;
	std::array<ComPtr<ID3D12Resource>,2> _pera1Resources;
	//�P���ڃy���|���̂��߂̃��\�[�X�ƃr���[��
	//�쐬
	bool CreatePera1ResourceAndView();
	
	//�y���|���p���_�o�b�t�@(N����4�_)
	ComPtr<ID3D12Resource> _peraVB;
	D3D12_VERTEX_BUFFER_VIEW _peraVBV;

	//�y���|���p�p�C�v���C�������[�g�V�O�l�`��
	ComPtr<ID3D12PipelineState> _peraPipeline;
	ComPtr<ID3D12RootSignature> _peraRS;

	


	//�y���|���ɓ�����萔�o�b�t�@
	ComPtr<ID3D12Resource> _peraCB;
	ComPtr<ID3D12DescriptorHeap> _peraCBVHeap;
	bool CreateConstantBufferForPera();

	//�c�ݗp�m�[�}���}�b�v
	ComPtr<ID3D12Resource> _distBuff;
	ComPtr<ID3D12DescriptorHeap> _distSRVHeap;
	//�[�x�l�e�N�X�`���p
	ComPtr<ID3D12DescriptorHeap> _depthSRVHeap;
	bool CreateDistortion();
	bool CreateDepthSRV();

	//�v���~�e�B�u�p���_�o�b�t�@
	std::vector<ComPtr<ID3D12Resource>> _primitivesVB;
	std::vector<D3D12_VERTEX_BUFFER_VIEW> _primitivesVBV;

	//�v���~�e�B�u�p�C���f�b�N�X�o�b�t�@
	std::vector<ComPtr<ID3D12Resource>> _primitivesIB;
	std::vector<D3D12_INDEX_BUFFER_VIEW> _primitivesIBV;
	bool CreatePrimitives();
	
	ComPtr<ID3D12RootSignature> _primitveRS;
	ComPtr<ID3D12PipelineState> _primitivePipeline;
	bool CreatePrimitivePipeline();
	bool CreatePrimitiveRootSignature();
	
	ComPtr<ID3D12PipelineState> _blurPipeline;//��ʑS�̂ڂ����p�p�C�v���C��
	std::array<ComPtr<ID3D12Resource>, 2> _bloomBuffers;//�u���[���p�o�b�t�@
	ComPtr<ID3D12Resource> _dofBuffer;//��ʊE�[�x�p�ڂ����o�b�t�@
	bool CreateBloomBuffer();
	bool CreateBlurForDOFBuffer();

public:
	Dx12Wrapper(HWND hwnd);
	~Dx12Wrapper();

	ID3D12Device* Device() {
		return _dev.Get();
	}
	ID3D12GraphicsCommandList* CmdList() {
		return _cmdList.Get();
	}
	ID3D12CommandQueue* CmdQue() {
		return _cmdQue.Get();
	}


	bool Init();
	bool CreateRenderTargetView();

	ComPtr<ID3D12Resource> WhiteTexture();
	ComPtr<ID3D12Resource> BlackTexture();
	ComPtr<ID3D12Resource> GradTexture();



	bool CreatePeraVertex();
	bool CreatePeraPipeline();

	//���C�g����̕`��(�e�p)�̏���
	bool PreDrawShadow();

	//�y���|���S���ւ̕`�揀��
	bool PreDrawToPera1();

	//�y���|���S���ւ̕`��
	///�v���~�e�B�u�`��(���ʁA�~���A�~���A��)��`��
	void DrawPrimitiveShapes();
	void DrawToPera1(std::shared_ptr<PMDRenderer> renderer);

	void DrawShrinkTextureForBlur();
	//��ʂ̃N���A
	bool Clear();

	//�`��
	void Draw(std::shared_ptr<PMDRenderer> renderer);

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

