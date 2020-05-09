#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dx12.h>
#include <map>
#include <functional>
#include <vector>
#include <array>
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

	void PreDrawShadow();
	void PreDrawToPera1();
	void PostDrawToPera1();
#if 0 // ペラ2に描画するパスは今は使わないのでコメントアウト
	void DrawHorizontalBokeh();
#endif
	void DrawShrinkTextureForBlur();
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
	ComPtr<ID3D12Resource> _lightDepthBuffer = nullptr;
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
	ComPtr<ID3D12DescriptorHeap> _peraSRVHeap = nullptr;
	std::array<ComPtr<ID3D12Resource>, 2> _pera1Resources;
#if 0 // ペラ2に描画するパスは今は使わないのでコメントアウト
	ComPtr<ID3D12Resource> _peraResources2 = nullptr;
#endif
	ComPtr<ID3D12RootSignature> _peraRS = nullptr;
	ComPtr<ID3D12PipelineState> _peraPipeline = nullptr;
#if 0 // ペラ2に描画するパスは今は使わないのでコメントアウト
	ComPtr<ID3D12PipelineState> _peraPipeline2 = nullptr;
#endif

	ComPtr<ID3D12DescriptorHeap> _distortionSRVHeap = nullptr;
	ComPtr<ID3D12Resource> _distortionTexBuffer = nullptr;

	ComPtr<ID3D12DescriptorHeap> _depthSRVHeap = nullptr;

	ComPtr<ID3D12Resource> _bokehParamResource = nullptr;
	ComPtr<ID3D12DescriptorHeap> _peraCBVHeap = nullptr;

	std::array<ComPtr<ID3D12Resource>, 2> _bloomBuffers;
	ComPtr<ID3D12PipelineState> _blurPipeline = nullptr;

	ComPtr<ID3D12Resource> CreateGrayGradientTexture();
	ComPtr<ID3D12Resource> CreateWhiteTexture();
	ComPtr<ID3D12Resource> CreateBlackTexture();

	HRESULT CreateDXGIDevice();
	HRESULT CreateCommand();
	HRESULT CreateSwapChain();
	HRESULT CreateFinalRenderTarget(const struct DXGI_SWAP_CHAIN_DESC1& swapchainDesc);
	HRESULT CreatePeraVertex();
	HRESULT CreateEffectBufferAndView();
	HRESULT CreateDepthBuffer();
	HRESULT CreateDSV();
	HRESULT CreateDepthSRV();
	HRESULT CreateConstantBufferForPera();
	HRESULT CreateBloomBuffer();
	HRESULT CreatePeraResouceAndView();
	HRESULT CreatePeraPipeline();
	HRESULT CreateCameraConstantBuffer();
};

