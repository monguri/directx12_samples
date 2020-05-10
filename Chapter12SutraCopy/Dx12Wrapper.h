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

	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	ComPtr<ID3D12Device> Device() const;
	ComPtr<ID3D12GraphicsCommandList> CommandList() const;
	static std::string GetExtension(const std::string& path);
	ComPtr<ID3D12Resource> LoadTextureFromFile(const std::string& texPath);
	bool CheckResult(HRESULT result , ID3DBlob* error=nullptr);
	ComPtr<ID3D12Resource> GetGrayGradientTexture();
	ComPtr<ID3D12Resource> GetWhiteTexture();
	ComPtr<ID3D12Resource> GetBlackTexture();

	void PreDrawToPera1();
	void PostDrawToPera1();
	void SetCamera();
	void DrawHorizontalBokeh();
	void Draw();
	void Flip();

private:
	ComPtr<ID3D12Device> _dev = nullptr;
	ComPtr<IDXGIFactory6> _dxgiFactory = nullptr;
	ComPtr<ID3D12CommandAllocator> _cmdAllocator = nullptr;
	ComPtr<ID3D12GraphicsCommandList> _cmdList = nullptr;
	ComPtr<ID3D12CommandQueue> _cmdQueue = nullptr;
	ComPtr<IDXGISwapChain4> _swapchain = nullptr;

	// ファイル拡張子ごとにロード関数を使い分けるためのテーブル
	using LoadLambda_t = std::function<HRESULT(const std::wstring& path, DirectX::TexMetadata* meta, DirectX::ScratchImage& img)>;
	std::map<std::string, LoadLambda_t> _loadLambdaTable;

	// ファイルパスごとにリソースをキャッシュして使いまわすためのテーブル
	std::map<std::string, ComPtr<ID3D12Resource>> _resourceTable;

	ComPtr<ID3D12Resource> _gradTex = nullptr;
	ComPtr<ID3D12Resource> _whiteTex = nullptr;
	ComPtr<ID3D12Resource> _blackTex = nullptr;

	// バッファは描画に用いるので保持し続ける必要がある
	ComPtr<ID3D12Resource> _depthBuffer = nullptr;
	ComPtr<ID3D12Resource> _sceneConstBuff = nullptr;

	struct SceneMatrix* _mappedScene = nullptr;

	ComPtr<ID3D12Fence> _fence = nullptr;
	UINT _fenceVal = 0;

	std::vector<ComPtr<ID3D12Resource>> _backBuffers;

	ComPtr<ID3D12DescriptorHeap> _sceneDescHeap = nullptr;

	ComPtr<ID3D12DescriptorHeap> _rtvHeaps = nullptr;
	ComPtr<ID3D12DescriptorHeap> _dsvHeap = nullptr;

	ComPtr<ID3D12Resource> _peraVB = nullptr;
	D3D12_VERTEX_BUFFER_VIEW _peraVBV;
	ComPtr<ID3D12DescriptorHeap> _peraRTVHeap = nullptr;
	ComPtr<ID3D12DescriptorHeap> _peraRegisterHeap = nullptr;
	ComPtr<ID3D12Resource> _peraResource = nullptr;
	ComPtr<ID3D12Resource> _peraResource2 = nullptr;
	ComPtr<ID3D12RootSignature> _peraRS = nullptr;
	ComPtr<ID3D12PipelineState> _peraPipeline = nullptr;
	ComPtr<ID3D12PipelineState> _peraPipeline2 = nullptr;

	ComPtr<ID3D12DescriptorHeap> _distortionSRVHeap = nullptr;
	ComPtr<ID3D12Resource> _distortionTexBuffer = nullptr;

	ComPtr<ID3D12Resource> _bokehParamResource = nullptr;

	ComPtr<ID3D12Resource> CreateGrayGradientTexture();
	ComPtr<ID3D12Resource> CreateWhiteTexture();
	ComPtr<ID3D12Resource> CreateBlackTexture();

	HRESULT CreateDXGIDevice();
	HRESULT CreateCommand();
	HRESULT CreateFinalRenderTarget(const struct DXGI_SWAP_CHAIN_DESC1& swapchainDesc);
	HRESULT CreatePeraVertex();
	HRESULT CreateEffectBufferAndView();
	HRESULT CreateBokehParamResouce();
	HRESULT CreatePeraResouceAndView();
	HRESULT CreatePeraPipeline();
	HRESULT CreateDepthStencil();
	HRESULT CreateCameraConstantBuffer();
};

