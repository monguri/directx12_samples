#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dx12.h>
#include <map>
#include <functional>
#include <vector>
#include <wrl.h>
#include <DirectXTex.h>

class Dx12Wrapper
{
public:
	Dx12Wrapper(HWND hwnd);
	void BeginDraw(float& angle);
	void SetCamera();
	void EndDraw();

	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	ComPtr<ID3D12Device> Device() const;
	ComPtr<ID3D12GraphicsCommandList> CommandList() const;
	static std::string GetExtension(const std::string& path);
	ComPtr<ID3D12Resource> LoadTextureFromFile(const std::string& texPath);

private:
	ComPtr<ID3D12Device> _dev = nullptr;
	ComPtr<IDXGIFactory6> _dxgiFactory = nullptr;
	ComPtr<ID3D12CommandAllocator> _cmdAllocator = nullptr;
	ComPtr<ID3D12GraphicsCommandList> _cmdList = nullptr;
	ComPtr<ID3D12CommandQueue> _cmdQueue = nullptr;
	ComPtr<IDXGISwapChain4> _swapchain = nullptr;

	// �t�@�C���g���q���ƂɃ��[�h�֐����g�������邽�߂̃e�[�u��
	using LoadLambda_t = std::function<HRESULT(const std::wstring& path, DirectX::TexMetadata* meta, DirectX::ScratchImage& img)>;
	std::map<std::string, LoadLambda_t> _loadLambdaTable;

	// �t�@�C���p�X���ƂɃ��\�[�X���L���b�V�����Ďg���܂킷���߂̃e�[�u��
	std::map<std::string, ComPtr<ID3D12Resource>> _resourceTable;


	DirectX::XMMATRIX _worldMat;

	ComPtr<ID3D12Resource> _depthBuffer = nullptr;
	ComPtr<ID3D12Resource> _constBuff = nullptr;

	struct SceneData* _mapScene = nullptr;

	ComPtr<ID3D12Fence> _fence = nullptr;
	UINT _fenceVal = 0;

	std::vector<ComPtr<ID3D12Resource>> _backBuffers;

	ComPtr<ID3D12DescriptorHeap> _basicDescHeap = nullptr;

	ComPtr<ID3D12DescriptorHeap> _rtvHeaps = nullptr;
	ComPtr<ID3D12DescriptorHeap> _dsvHeap = nullptr;

	CD3DX12_VIEWPORT _viewport;
	CD3DX12_RECT _scissorrect;

	HRESULT CreateDXGIDevice();
	HRESULT CreateCommand();
	HRESULT CreateSwapChain();
	HRESULT CreateFinalRenderTarget(const struct DXGI_SWAP_CHAIN_DESC1& swapchainDesc);
	HRESULT CreateDepthStencil();
	HRESULT CreateCameraConstantBuffer();
};
