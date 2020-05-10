#include "Dx12Wrapper.h"
#include "Application.h"
#include<d3dcompiler.h>

#ifdef _DEBUG
#include <iostream>
#endif

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "DirectXTex.lib")
#pragma comment(lib, "EffekseerRendererDX12.lib")
#pragma comment(lib, "Effekseer.lib")
#pragma comment(lib, "LLGI.lib")

using namespace DirectX;
using namespace Microsoft::WRL;

// シャドウマップ解像度。平行投影
constexpr uint32_t shadow_definition = 1024;

namespace
{
	std::vector<float> GetGaussianWeights(size_t count, float s)
	{
		// 左右対称なので、要素0は中心ピクセルとし、右方向のみ格納し、左方向は格納しない
		std::vector<float> weights(count);

		float x = 0.0f;
		float total = 0.0f;

		for (float& wgt : weights)
		{
			wgt = expf(-(x * x) / (2 * s * s));
			total += wgt;
			x += 1.0f;
		}

		// 左右対称なので割る数を2倍する。
		// 中心の要素も2回分入るので、それはe~0=1だとわかっているので
		// さしひく
		total = total * 2.0f - 1.0f;

		for (float& wgt : weights)
		{
			wgt /= total;
		}

		return weights;
	}

	std::wstring GetWideStringFromString(const std::string& str)
	{
		// MultiByteToWideCharで使うには先にwchar_t配列を必要なサイズの確保が必要。
		// 固定長配列を返してもいいがstd::wstringの方が扱いやすいので
		// 先に長さを取得してresizeしておく

		// 引数の文字列の長さ取得
		int num1 = MultiByteToWideChar(
			CP_ACP,
			MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
			str.c_str(),
			-1,
			nullptr,
			0
		);
		std::wstring wstr;
		wstr.resize(num1);

		int num2 = MultiByteToWideChar(
			CP_ACP,
			MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
			str.c_str(),
			-1,
			&wstr[0],
			num1
		);

		assert(num1 == num2);
		return wstr;
	}

	void EnableDebugLayer()
	{
		ComPtr<ID3D12Debug> debugLayer = nullptr;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debugLayer.ReleaseAndGetAddressOf()))))
		{
			debugLayer->EnableDebugLayer();
			debugLayer->Release();
		}
	}
} // namespace

ComPtr<ID3D12Device> Dx12Wrapper::Device() const
{
	return _dev;
}

ComPtr<ID3D12GraphicsCommandList> Dx12Wrapper::CommandList() const
{
	return _cmdList;
}

ComPtr<ID3D12CommandQueue> Dx12Wrapper::CmdQue() const
{
	return _cmdQueue;
}

std::string Dx12Wrapper::GetExtension(const std::string& path)
{
	size_t idx = path.rfind('.');
	return path.substr(idx + 1, path.length() - idx - 1);
}

ComPtr<ID3D12Resource> Dx12Wrapper::LoadTextureFromFile(const std::string& texPath)
{
	// キャッシュ済みならそれを返す
	// イテレータの型は複雑なのでautoを使う
	auto it = _resourceTable.find(texPath);
	if (it != _resourceTable.end())
	{
		return _resourceTable[texPath];
	}

	// WICテクスチャのロード
	TexMetadata metadata = {};
	ScratchImage scratchImg = {};
	const std::wstring& wtexpath = GetWideStringFromString(texPath);
	const std::string& ext = GetExtension(texPath);

	HRESULT result = _loadLambdaTable[ext](
		wtexpath,
		&metadata,
		scratchImg
	);
	if (FAILED(result))
	{
		// 指定されたパスにファイルが存在しないケースは想定しているのでassertはしない
		return nullptr;
	}

	const Image* img = scratchImg.GetImage(0, 0, 0);

	// テクスチャバッファ作成
	CD3DX12_HEAP_PROPERTIES texHeapProp(
		D3D12_CPU_PAGE_PROPERTY_WRITE_BACK,
		D3D12_MEMORY_POOL_L0
	);

	D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_R8G8B8A8_UNORM,
		(UINT)metadata.width,
		(UINT)metadata.height,
		(UINT16)metadata.arraySize,
		(UINT16)metadata.mipLevels
	);

	ComPtr<ID3D12Resource> texbuff = nullptr;
	result = _dev->CreateCommittedResource(
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(texbuff.ReleaseAndGetAddressOf())
	);
	if (FAILED(result))
	{
		assert(false);
		return nullptr;
	}

	// テクスチャバッファへ作成したテクスチャデータを書き込み
	result = texbuff->WriteToSubresource(
		0,
		nullptr,
		img->pixels,
		(UINT)img->rowPitch,
		(UINT)img->slicePitch
	);
	if (FAILED(result))
	{
		assert(false);
		return nullptr;
	}

	// テーブルにキャッシュ
	_resourceTable[texPath] = texbuff;
	return texbuff;
}

struct SceneMatrix
{
	XMMATRIX view;
	XMMATRIX proj;
	XMMATRIX invviewproj;
	XMMATRIX lightCamera; // ライトビュープロジェクション
	XMMATRIX shadow;
	XMFLOAT4 lightVec; // アラインメントを防ぐためにvec4に
	XMFLOAT3 eye;
	bool isSelfShadow;
};

Dx12Wrapper::Dx12Wrapper(HWND hwnd)
{
#ifdef _DEBUG
	EnableDebugLayer();
#endif // _DEBUG

	// テクスチャローダー関数テーブル作成
	_loadLambdaTable["sph"] = _loadLambdaTable["spa"] = _loadLambdaTable["bmp"] = _loadLambdaTable["png"] =_loadLambdaTable["jpg"] =

	[](const std::wstring& path, TexMetadata* meta, ScratchImage& img)->HRESULT {
		return LoadFromWICFile(path.c_str(), WIC_FLAGS_NONE, meta, img);
	};

	_loadLambdaTable["tga"] =
	[](const std::wstring& path, TexMetadata* meta, ScratchImage& img)->HRESULT {
		return LoadFromTGAFile(path.c_str(), meta, img);
	};

	_loadLambdaTable["dds"] =
	[](const std::wstring& path, TexMetadata* meta, ScratchImage& img)->HRESULT {
		return LoadFromDDSFile(path.c_str(), WIC_FLAGS_NONE, meta, img);
	};

	HRESULT result = CreateDXGIDevice();
	if (FAILED(result))
	{
		assert(false);
		return;
	}

	result = CreateCommand();
	if (FAILED(result))
	{
		assert(false);
		return;
	}

	const SIZE& winSize = Application::Instance().GetWindowSize();

	// スワップチェインの生成。一関数しか呼ばないので関数化しない
	DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
	swapchainDesc.Width = winSize.cx;
	swapchainDesc.Height = winSize.cy;
	swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapchainDesc.Stereo = false;
	swapchainDesc.SampleDesc.Count = 1;
	swapchainDesc.SampleDesc.Quality = 0;
	swapchainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
	swapchainDesc.BufferCount = 2;
	swapchainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	result = _dxgiFactory->CreateSwapChainForHwnd(
		_cmdQueue.Get(),
		hwnd,
		&swapchainDesc,
		nullptr,
		nullptr,
		(IDXGISwapChain1**)_swapchain.ReleaseAndGetAddressOf()
	);
	if (FAILED(result))
	{
		assert(false);
		return;
	}

	result = CreateFinalRenderTarget(swapchainDesc);
	if (FAILED(result))
	{
		assert(false);
		return;
	}

	result = CreatePeraVertex();
	if (FAILED(result))
	{
		assert(false);
		return;
	}

	result = CreateEffectBufferAndView();
	if (FAILED(result))
	{
		assert(false);
		return;
	}

	result = CreateDepthBuffer();
	if (FAILED(result))
	{
		assert(false);
		return;
	}

	result = CreateDSV();
	if (FAILED(result))
	{
		assert(false);
		return;
	}

	result = CreateDepthSRV();
	if (FAILED(result))
	{
		assert(false);
		return;
	}

	result = CreateTransformConstantBuffer();
	if (FAILED(result))
	{
		assert(false);
		return;
	}

	result = CreateTransformBufferView();
	if (FAILED(result))
	{
		assert(false);
		return;
	}

	result = CreatePostSetting();
	if (FAILED(result))
	{
		assert(false);
		return;
	}

	result = CreateConstantBufferForPera();
	if (FAILED(result))
	{
		assert(false);
		return;
	}

	result = CreateBloomBuffer();
	if (FAILED(result))
	{
		assert(false);
		return;
	}

	result = CreateBlurForDOFBuffer();
	if (FAILED(result))
	{
		assert(false);
		return;
	}

	result = CreateAmbientOcclusionBuffer();
	if (FAILED(result))
	{
		assert(false);
		return;
	}

	result = CreateAmbientOcclusionDescriptorHeap();
	if (FAILED(result))
	{
		assert(false);
		return;
	}

	result = CreatePeraResouceAndView();
	if (FAILED(result))
	{
		assert(false);
		return;
	}

	result = CreatePeraPipeline();
	if (FAILED(result))
	{
		assert(false);
		return;
	}

	result = CreateDescriptorHeapForImgui();
	if (FAILED(result))
	{
		assert(false);
		return;
	}

	// フェンスの生成
	result = _dev->CreateFence(_fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(_fence.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(false);
		return;
	}

	_whiteTex = CreateWhiteTexture();
	_blackTex = CreateBlackTexture();
	_gradTex = CreateGrayGradientTexture();
	assert(_whiteTex != nullptr);
	assert(_blackTex != nullptr);
	assert(_gradTex != nullptr);

	return;
}

HRESULT Dx12Wrapper::CreateDXGIDevice()
{
	// DXGIFactoryの生成
	HRESULT result = CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(_dxgiFactory.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	// NVIDIAアダプタの選択
	std::vector<ComPtr<IDXGIAdapter>> adapters;
	ComPtr<IDXGIAdapter> tmpAdapter = nullptr;
	for (int i = 0; _dxgiFactory->EnumAdapters(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; ++i)
	{
		adapters.push_back(tmpAdapter);
	}

	ComPtr<IDXGIAdapter> nvidiaAdapter = nullptr;
	for (ComPtr<IDXGIAdapter> adapter : adapters)
	{
		DXGI_ADAPTER_DESC desc = {};
		adapter->GetDesc(&desc);
		std::wstring strDesc = desc.Description;
		if (strDesc.find(L"NVIDIA") != std::string::npos)
		{
			nvidiaAdapter = adapter;
			break;
		}
	}

	D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};

	result = S_FALSE;

	// Direct3Dデバイスの初期化
	D3D_FEATURE_LEVEL featureLevel;
	for (D3D_FEATURE_LEVEL level : levels)
	{
		if (SUCCEEDED(D3D12CreateDevice(nvidiaAdapter.Get(), level, IID_PPV_ARGS(_dev.ReleaseAndGetAddressOf()))))
		{
			featureLevel = level;
			result = S_OK;
			break;
		}
	}

	return result;
}

HRESULT Dx12Wrapper::CreateCommand()
{
	// コマンドアロケータとコマンドリストの生成
	HRESULT result = _dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(_cmdAllocator.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	result = _dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmdAllocator.Get(), nullptr, IID_PPV_ARGS(_cmdList.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	// コマンドキューの生成
	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	cmdQueueDesc.NodeMask = 0;
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	result = _dev->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(_cmdQueue.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	return result;
}

HRESULT Dx12Wrapper::CreateFinalRenderTarget(const DXGI_SWAP_CHAIN_DESC1& swapchainDesc)
{
	// バッファはID3D12Resourceを自分で作らなくても、スワップチェイン作成時に
	// 同時に作られてスワップチェインに保持されている
	// RenderTargetViewを2枚のバッファに作る。

	// ディスクリプタヒープの生成と、2枚のバッファ用のレンダーターゲットビューの生成
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 2;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	HRESULT result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(_rtvHeaps.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	CD3DX12_CPU_DESCRIPTOR_HANDLE handle(_rtvHeaps->GetCPUDescriptorHandleForHeapStart());

	// SRGBテクスチャ対応
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	_backBuffers.resize(swapchainDesc.BufferCount);
	for (UINT i = 0; i < swapchainDesc.BufferCount; ++i)
	{
		result = _swapchain->GetBuffer(i, IID_PPV_ARGS(_backBuffers[i].ReleaseAndGetAddressOf()));
		if (FAILED(result))
		{
			assert(false);
			return result;
		}

		rtvDesc.Format = _backBuffers[i]->GetDesc().Format;
		_dev->CreateRenderTargetView(_backBuffers[i].Get(), &rtvDesc, handle);
		handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	return result;
}

HRESULT Dx12Wrapper::CreatePeraVertex()
{
	struct PeraVertex
	{
		XMFLOAT3 pos;
		XMFLOAT2 uv;
	};

	PeraVertex pv[4] = {
		{{-1.0f, -1.0f, 0.1f}, {0.0f, 1.0f}}, // 左下
		{{-1.0f, 1.0f, 0.1f}, {0.0f, 0.0f}}, // 左上
		{{1.0f, -1.0f, 0.1f}, {1.0f, 1.0f}}, // 右下
		{{1.0f, 1.0f, 0.1f}, {1.0f, 0.0f}} // 右上
	};

	HRESULT result = _dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(pv)),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_peraVB.ReleaseAndGetAddressOf())
	);
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	// 頂点バッファービュー
	_peraVBV.BufferLocation = _peraVB->GetGPUVirtualAddress();
	_peraVBV.SizeInBytes = (UINT)sizeof(pv);
	_peraVBV.StrideInBytes = sizeof(PeraVertex);

	// 頂点バッファへのデータ書き込み
	PeraVertex* mappedPera = nullptr;
	result = _peraVB->Map(0, nullptr, (void**)&mappedPera);
	if (FAILED(result))
	{
		assert(false);
		return result;
	}
	std::copy(std::begin(pv), std::end(pv), mappedPera);
	_peraVB->Unmap(0, nullptr);

	return result;
}

HRESULT Dx12Wrapper::CreateEffectBufferAndView()
{
	_distortionTexBuffer = LoadTextureFromFile("normal/normalmap.jpg");
	if (_distortionTexBuffer == nullptr)
	{
		assert(false);
		return E_FAIL;
	}

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heapDesc.NumDescriptors = 1;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	HRESULT result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(_distortionSRVHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	D3D12_RESOURCE_DESC desc = _distortionTexBuffer->GetDesc();

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = desc.Format;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	_dev->CreateShaderResourceView(
		_distortionTexBuffer.Get(),
		&srvDesc,
		_distortionSRVHeap->GetCPUDescriptorHandleForHeapStart()
	);

	return result;
}

HRESULT Dx12Wrapper::CreateConstantBufferForPera()
{
	// 自分を含めて片方向8ピクセル分。標準偏差は5ピクセル。
	const std::vector<float>& weights = GetGaussianWeights(8, 5.0f);
	// TODO:本のように関数化したい
	size_t alignmentedSize = (sizeof(weights[0]) * weights.size() + 0xff) & ~0xff;

	HRESULT result = _dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(alignmentedSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_bokehParamResource.ReleaseAndGetAddressOf())
	);
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	// ディスクリプタヒープとCBV作成
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 1;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(_peraCBVHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = _bokehParamResource->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = (UINT)_bokehParamResource->GetDesc().Width;
	_dev->CreateConstantBufferView(
		&cbvDesc,
		_peraCBVHeap->GetCPUDescriptorHandleForHeapStart()
	);

	float* mappedWeight = nullptr;
	result = _bokehParamResource->Map(0, nullptr, (void**)&mappedWeight);
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	std::copy(weights.begin(), weights.end(), mappedWeight);
	_bokehParamResource->Unmap(0, nullptr);

	return result;
}

HRESULT Dx12Wrapper::CreateBloomBuffer()
{
	// FinalRenderTargetと同じ設定のバッファを2枚作る

	ComPtr<ID3D12Resource> bbuff = _backBuffers[0];
	D3D12_RESOURCE_DESC resDesc = bbuff->GetDesc();

	const D3D12_HEAP_PROPERTIES& heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	float clsClr[4] = {0.0f, 0.0f, 0.0f, 1.0f};
	D3D12_CLEAR_VALUE clearValue = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_R8G8B8A8_UNORM, clsClr);

	for (ComPtr<ID3D12Resource>& res : _bloomBuffers)
	{
		HRESULT result = _dev->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			&clearValue,
			IID_PPV_ARGS(res.ReleaseAndGetAddressOf())
		);
		if (FAILED(result))
		{
			assert(false);
			return result;
		}

		// 2枚目は幅が半分のバッファにする。
		// 1/2の累乗サイズにしていく縮小バッファ8枚分をその中に縦に押し込めるので、高さは変えない
		resDesc.Width >>= 1;
	}

	return S_OK;
}

HRESULT Dx12Wrapper::CreateBlurForDOFBuffer()
{
	// FinalRenderTargetと同じ設定のバッファを2枚作る

	ComPtr<ID3D12Resource> bbuff = _backBuffers[0];
	D3D12_RESOURCE_DESC resDesc = bbuff->GetDesc();

	const D3D12_HEAP_PROPERTIES& heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	float clsClr[4] = {0.0f, 0.0f, 0.0f, 1.0f};
	D3D12_CLEAR_VALUE clearValue = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_R8G8B8A8_UNORM, clsClr);

	// 幅が半分のバッファにする。
	// 1/2の累乗サイズにしていく縮小バッファ8枚分をその中に縦に押し込めるので、高さは変えない
	resDesc.Width >>= 1;

	HRESULT result = _dev->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&clearValue,
		IID_PPV_ARGS(_dofBuffer.ReleaseAndGetAddressOf())
	);
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	return result;
}

HRESULT Dx12Wrapper::CreateAmbientOcclusionBuffer()
{
	ComPtr<ID3D12Resource> bbuff = _backBuffers[0];
	D3D12_RESOURCE_DESC resDesc = bbuff->GetDesc();
	resDesc.Format = DXGI_FORMAT_R32_FLOAT;

	const D3D12_HEAP_PROPERTIES& heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	float clsClr[4] = {1.0f, 1.0f, 1.0f, 1.0f};
	D3D12_CLEAR_VALUE clearValue = CD3DX12_CLEAR_VALUE(resDesc.Format, clsClr);

	HRESULT result = _dev->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&clearValue,
		IID_PPV_ARGS(_aoBuffer.ReleaseAndGetAddressOf())
	);
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	return result;
}

HRESULT Dx12Wrapper::CreateAmbientOcclusionDescriptorHeap()
{
	// RTV用ヒープ作成
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	desc.NodeMask = 0;
	desc.NumDescriptors = 1;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	HRESULT result = _dev->CreateDescriptorHeap(&desc, IID_PPV_ARGS(_aoRTVDH.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	// RTV作成
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = _aoBuffer->GetDesc().Format;
	_dev->CreateRenderTargetView(_aoBuffer.Get(), &rtvDesc, _aoRTVDH->GetCPUDescriptorHandleForHeapStart());

	// SRV用ヒープ作成
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	desc.NodeMask = 0;
	desc.NumDescriptors = 1;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	result = _dev->CreateDescriptorHeap(&desc, IID_PPV_ARGS(_aoSRVDH.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	// SRV作成
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = rtvDesc.Format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	_dev->CreateShaderResourceView(_aoBuffer.Get(), &srvDesc, _aoSRVDH->GetCPUDescriptorHandleForHeapStart());

	return result;
}

HRESULT Dx12Wrapper::CreateDescriptorHeapForImgui()
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	desc.NodeMask = 0;
	desc.NumDescriptors = 1;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	HRESULT result = _dev->CreateDescriptorHeap(&desc, IID_PPV_ARGS(_heapForImgui.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	return result;
}

ComPtr<ID3D12DescriptorHeap> Dx12Wrapper::GetHeapForImgui() const
{
	return _heapForImgui;
}

HRESULT Dx12Wrapper::CreatePeraResouceAndView()
{
	// FinalRenderTargetと同じ設定のバッファをペラ用として2枚作る

	ComPtr<ID3D12Resource> bbuff = _backBuffers[0];
	D3D12_RESOURCE_DESC resDesc = bbuff->GetDesc();

	const D3D12_HEAP_PROPERTIES& heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	D3D12_CLEAR_VALUE clearValue = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_R8G8B8A8_UNORM, _bgColor);

	for (ComPtr<ID3D12Resource>& res : _pera1Resources)
	{
		HRESULT result = _dev->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			&clearValue,
			IID_PPV_ARGS(res.ReleaseAndGetAddressOf())
		);
		if (FAILED(result))
		{
			assert(false);
			return result;
		}
	}

#if 0 // ペラ2に描画するパスは今は使わないのでコメントアウト
	result = _dev->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&clearValue,
		IID_PPV_ARGS(_peraResources2.ReleaseAndGetAddressOf())
	);
	if (FAILED(result))
	{
		assert(false);
		return result;
	}
#endif

	// RTV用ヒープ。これもダブルバッファの設定を使う。
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = _rtvHeaps->GetDesc();
	heapDesc.NumDescriptors = 5; // ペラ1プラス法線プラス高輝度プラス縮小高輝度プラス被写界深度用縮小バッファ
	HRESULT result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(_peraRTVHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	// RTV2枚作成
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	D3D12_CPU_DESCRIPTOR_HANDLE handle = _peraRTVHeap->GetCPUDescriptorHandleForHeapStart();
	for (const ComPtr<ID3D12Resource>& res : _pera1Resources)
	{
		_dev->CreateRenderTargetView(res.Get(), &rtvDesc, handle);
		handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

#if 0 // ペラ2に描画するパスは今は使わないのでコメントアウト
	_dev->CreateRenderTargetView(_peraResources2.Get(), &rtvDesc, handle);
	// TODO:本ではこうなっているがRTVが正しいのでは
	//handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
#endif

	// ブルーム用高輝度RTV
	_dev->CreateRenderTargetView(_bloomBuffers[0].Get(), &rtvDesc, handle);
	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	// ブルーム用縮小高輝度RTV
	_dev->CreateRenderTargetView(_bloomBuffers[1].Get(), &rtvDesc, handle);
	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	// 被写界深度用縮小バッファRTV
	_dev->CreateRenderTargetView(_dofBuffer.Get(), &rtvDesc, handle);

	// レンダーテクスチャSRV2枚と高輝度SRV2枚と被写界深度SRV用ヒープ
	heapDesc.NumDescriptors = 5;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(_peraSRVHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	handle = _peraSRVHeap->GetCPUDescriptorHandleForHeapStart();

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = rtvDesc.Format;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	for (const ComPtr<ID3D12Resource>& res : _pera1Resources)
	{
		_dev->CreateShaderResourceView(
			res.Get(),
			&srvDesc,
			handle
		);
		handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}
	// ブルーム用高輝度SRV。
	_dev->CreateShaderResourceView(
		_bloomBuffers[0].Get(),
		&srvDesc,
		handle
	);
	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	// ブルーム用縮小高輝度SRV。
	_dev->CreateShaderResourceView(
		_bloomBuffers[1].Get(),
		&srvDesc,
		handle
	);
	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	// 被写界深度用縮小バッファSRV。
	_dev->CreateShaderResourceView(
		_dofBuffer.Get(),
		&srvDesc,
		handle
	);

#if 0 // ペラ2に描画するパスは今は使わないのでコメントアウト
	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	_dev->CreateShaderResourceView(
		_peraResources2.Get(),
		&srvDesc,
		handle
	);
#endif

	return result;
}

bool Dx12Wrapper::CheckResult(HRESULT result, ID3DBlob* error) {
	if (FAILED(result)) {
		if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
			::OutputDebugStringA("ファイルが見当たりません");
		}
		else {
			std::string errstr;
			errstr.resize(error->GetBufferSize());
			std::copy_n((char*)error->GetBufferPointer(), error->GetBufferSize(), errstr.begin());
			errstr += "\n";
			OutputDebugStringA(errstr.c_str());
		}
		return false;
	}
	else {
		return true;
	}
}

HRESULT Dx12Wrapper::CreatePeraPipeline()
{
	D3D12_DESCRIPTOR_RANGE range[6] = {};
	// ガウシアンウェイトCBVのb0
	range[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	range[0].BaseShaderRegister = 0;
	range[0].NumDescriptors = 1;
	range[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	// ペラ1、2SRVのt0、法線SRVのt1、高輝度SRVのt2、縮小高輝度SRVのt3、被写界深度用SRVのt4
	range[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	range[1].BaseShaderRegister = 0;
	range[1].NumDescriptors = 5;
	range[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	// ディストーションテクスチャSVRのt5
	range[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	range[2].BaseShaderRegister = 5;
	range[2].NumDescriptors = 1;
	range[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	// 深度値テクスチャSRVとシャドウマップSRVのt6とt7
	range[3].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	range[3].BaseShaderRegister = 6;
	range[3].NumDescriptors = 2;
	range[3].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	// SSAOテクスチャSRVのt8
	range[4].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	range[4].BaseShaderRegister = 8;
	range[4].NumDescriptors = 1;
	range[4].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	// ポストプロセスセッティングCBVのb1
	range[5].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	range[5].BaseShaderRegister = 1;
	range[5].NumDescriptors = 1;
	range[5].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rp[6] = {};
	rp[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rp[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rp[0].DescriptorTable.pDescriptorRanges = &range[0];
	rp[0].DescriptorTable.NumDescriptorRanges = 1;

	rp[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rp[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rp[1].DescriptorTable.pDescriptorRanges = &range[1];
	rp[1].DescriptorTable.NumDescriptorRanges = 1;

	rp[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rp[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rp[2].DescriptorTable.pDescriptorRanges = &range[2];
	rp[2].DescriptorTable.NumDescriptorRanges = 1;

	rp[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rp[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rp[3].DescriptorTable.pDescriptorRanges = &range[3];
	rp[3].DescriptorTable.NumDescriptorRanges = 1;

	rp[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rp[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rp[4].DescriptorTable.pDescriptorRanges = &range[4];
	rp[4].DescriptorTable.NumDescriptorRanges = 1;

	rp[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rp[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rp[5].DescriptorTable.pDescriptorRanges = &range[5];
	rp[5].DescriptorTable.NumDescriptorRanges = 1;

	D3D12_STATIC_SAMPLER_DESC sampler = CD3DX12_STATIC_SAMPLER_DESC(0);

	D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
	rsDesc.NumParameters = 6;
	rsDesc.pParameters = rp;
	rsDesc.NumStaticSamplers = 1;
	rsDesc.pStaticSamplers = &sampler;
	rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	
	ComPtr<ID3DBlob> rsBlob = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT result = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, rsBlob.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf());
	if (!CheckResult(result, errorBlob.Get())){
		assert(false);
		return result;
	}
	result = _dev->CreateRootSignature(0, rsBlob->GetBufferPointer(), rsBlob->GetBufferSize(), IID_PPV_ARGS(_peraRS.ReleaseAndGetAddressOf()));
	if (!CheckResult(result, errorBlob.Get())){
		assert(false);
		return result;
	}

	ComPtr<ID3DBlob> vsBlob = nullptr;
	ComPtr<ID3DBlob> psBlob = nullptr;
	result = D3DCompileFromFile(L"PeraVertexShader.hlsl",
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"PeraVS", "vs_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0, vsBlob.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf());
	if (!CheckResult(result, errorBlob.Get())){
		assert(false);
		return result;
	}
#if 0 // 水平ガウシアンブラー
	result = D3DCompileFromFile(L"PeraPixelShader.hlsl",
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"PeraHorizontalBokehPS", "ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0, psBlob.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf());
	if (!CheckResult(result, errorBlob.Get())) {
		assert(false);
		return result;
	}
#else
	result = D3DCompileFromFile(L"PeraPixelShader.hlsl",
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"PeraPS", "ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0, psBlob.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf());
	if (!CheckResult(result, errorBlob.Get())) {
		assert(false);
		return result;
	}
#endif

	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpsDesc = {};
	gpsDesc.VS = CD3DX12_SHADER_BYTECODE(vsBlob.Get());
	gpsDesc.PS = CD3DX12_SHADER_BYTECODE(psBlob.Get());
	gpsDesc.DepthStencilState.DepthEnable = false;
	gpsDesc.DepthStencilState.StencilEnable = false;


	D3D12_INPUT_ELEMENT_DESC layout[2] = {
		{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0 },
		{ "TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0 },
	};

	gpsDesc.InputLayout.pInputElementDescs = layout;
	gpsDesc.InputLayout.NumElements = _countof(layout);
	gpsDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	gpsDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	gpsDesc.NumRenderTargets = 1;
	gpsDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	gpsDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	gpsDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	gpsDesc.SampleDesc.Count = 1;
	gpsDesc.SampleDesc.Quality = 0;
	gpsDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	gpsDesc.pRootSignature = _peraRS.Get();

	result = _dev->CreateGraphicsPipelineState(&gpsDesc, IID_PPV_ARGS(_peraPipeline.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		assert(false);
		return result;
	}
	
#if 0 // ペラ2を使うパスは今は使わないないのでコメントアウトしておく
#if 1 // ポストプロセスなし
	result = D3DCompileFromFile(L"PeraPixelShader.hlsl",
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"PeraPS", "ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0, psBlob.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf());
#elif 0 // 輝度によるグレースケール
	result = D3DCompileFromFile(L"PeraPixelShader.hlsl",
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"PeraGrayscalePS", "ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0, psBlob.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf());
#elif 0 // 色反転
	result = D3DCompileFromFile(L"PeraPixelShader.hlsl",
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"PeraInverseColorPS", "ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0, psBlob.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf());
#elif 0 // 色諧調を下げる
	result = D3DCompileFromFile(L"PeraPixelShader.hlsl",
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"PeraDownToneLevelPS", "ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0, psBlob.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf());
#elif 0 // 近傍9ピクセルで平均をとる
	result = D3DCompileFromFile(L"PeraPixelShader.hlsl",
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"Pera9AveragePS", "ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0, psBlob.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf());
#elif 0 // エンボス加工
	result = D3DCompileFromFile(L"PeraPixelShader.hlsl",
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"PeraEmbossPS", "ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0, psBlob.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf());
#elif 0 // シャープネス
	result = D3DCompileFromFile(L"PeraPixelShader.hlsl",
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"PeraSharpnessPS", "ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0, psBlob.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf());
#elif 0 // エッジ抽出
	result = D3DCompileFromFile(L"PeraPixelShader.hlsl",
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"PeraEdgeDetectionPS", "ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0, psBlob.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf());
#elif 0 // ガウシアンブラー
	result = D3DCompileFromFile(L"PeraPixelShader.hlsl",
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"PeraGaussianBlurPS", "ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0, psBlob.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf());
#elif 0 // 垂直ガウシアンブラー
	result = D3DCompileFromFile(L"PeraPixelShader.hlsl",
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"PeraVerticalBokehPS", "ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0, psBlob.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf());
#elif 0 // 垂直ガウシアンブラー＋ディストーション
	result = D3DCompileFromFile(L"PeraPixelShader.hlsl",
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"PeraVerticalBokehAndDistortionPS", "ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0, psBlob.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf());
#elif 0 // デプスデバッグ表示
	result = D3DCompileFromFile(L"PeraPixelShader.hlsl",
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"PeraDepthDebugPS", "ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0, psBlob.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf());
#elif 0 // シャドウマップデバッグ表示
	result = D3DCompileFromFile(L"PeraPixelShader.hlsl",
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"PeraDepthFromLightDebugPS", "ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0, psBlob.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf());
#endif
	if (!CheckResult(result, errorBlob.Get())) {
		assert(false);
		return result;
	}

	gpsDesc.PS = CD3DX12_SHADER_BYTECODE(psBlob.Get());
	result = _dev->CreateGraphicsPipelineState(&gpsDesc, IID_PPV_ARGS(_peraPipeline2.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		assert(false);
		return result;
	}
#endif

	result = D3DCompileFromFile(L"PeraPixelShader.hlsl",
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"PeraBlurPS", "ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0, psBlob.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf());
	if (!CheckResult(result, errorBlob.Get())) {
		assert(false);
		return result;
	}

	gpsDesc.PS = CD3DX12_SHADER_BYTECODE(psBlob.Get());
	gpsDesc.NumRenderTargets = 2;
	gpsDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	gpsDesc.RTVFormats[1] = DXGI_FORMAT_R8G8B8A8_UNORM;
	result = _dev->CreateGraphicsPipelineState(&gpsDesc, IID_PPV_ARGS(_blurPipeline.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		assert(false);
		return result;
	}

	result = D3DCompileFromFile(L"SSAOPixelShader.hlsl",
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"SsaoPS", "ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0, psBlob.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf());
	if (!CheckResult(result, errorBlob.Get())) {
		assert(false);
		return result;
	}

	gpsDesc.NumRenderTargets = 1;
	gpsDesc.RTVFormats[0] = DXGI_FORMAT_R32_FLOAT;
	gpsDesc.RTVFormats[1] = DXGI_FORMAT_UNKNOWN;
	gpsDesc.BlendState.RenderTarget[0].BlendEnable = false;
	gpsDesc.PS = CD3DX12_SHADER_BYTECODE(psBlob.Get());
	result = _dev->CreateGraphicsPipelineState(&gpsDesc, IID_PPV_ARGS(_aoPipeline.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		assert(false);
		return result;
	}

	return result;
}

HRESULT Dx12Wrapper::CreateDepthBuffer()
{
	// 深度バッファ作成
	const SIZE& winSize = Application::Instance().GetWindowSize();
	D3D12_RESOURCE_DESC depthResDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_R32_TYPELESS,
		winSize.cx,
		winSize.cy,
		1,
		1,
		1,
		0,
		D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
	);

	CD3DX12_HEAP_PROPERTIES depthHeapProp(D3D12_HEAP_TYPE_DEFAULT);

	// D3D12_RESOURCE_DESCをDXGI_FORMAT_R32_TYPELESSにしても、
	// D3D12_CLEAR_VALUEをDXGI_FORMAT_R32_TYPELESSにするとエラーになる
	CD3DX12_CLEAR_VALUE depthClearValue(DXGI_FORMAT_D32_FLOAT, 1.0f, 0);
	
	HRESULT result = _dev->CreateCommittedResource(
		&depthHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&depthResDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue,
		IID_PPV_ARGS(_depthBuffer.ReleaseAndGetAddressOf())
	);
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	// シャドウマップ用バッファ作成
	depthResDesc.Width = shadow_definition;
	depthResDesc.Height = shadow_definition;

	result = _dev->CreateCommittedResource(
		&depthHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&depthResDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue,
		IID_PPV_ARGS(_lightDepthBuffer.ReleaseAndGetAddressOf())
	);
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	return result;
}

HRESULT Dx12Wrapper::CreateDSV()
{
	// デプスステンシルビュー2枚作成
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 2;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	HRESULT result = _dev->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(_dsvHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE handle = _dsvHeap->GetCPUDescriptorHandleForHeapStart();

	// 通常DSV
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	_dev->CreateDepthStencilView(_depthBuffer.Get(), &dsvDesc, handle);

	// シャドウマップ用DSV
	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	_dev->CreateDepthStencilView(_lightDepthBuffer.Get(), &dsvDesc, handle);

	return result;
}

HRESULT Dx12Wrapper::CreateDepthSRV()
{
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heapDesc.NumDescriptors = 2;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	HRESULT result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(_depthSRVHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	assert(_depthBuffer != nullptr);
	//D3D12_RESOURCE_DESC desc = _depthBuffer->GetDesc();

	D3D12_CPU_DESCRIPTOR_HANDLE handle = _depthSRVHeap->GetCPUDescriptorHandleForHeapStart();

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	// これはデプスバッファを作った時のDXGI_FORMAT_R32_TYPELESSではエラーになる
	//srvDesc.Format = desc.Format;
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	_dev->CreateShaderResourceView(
		_depthBuffer.Get(),
		&srvDesc,
		handle
	);

	// シャドウマップのSRVも同じでスクリプタヒープにまとめる
	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	_dev->CreateShaderResourceView(
		_lightDepthBuffer.Get(),
		&srvDesc,
		handle
	);

	return result;
}

HRESULT Dx12Wrapper::CreateTransformConstantBuffer()
{
	HRESULT result = _dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer((sizeof(SceneMatrix) + 0xff) & ~0xff),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_sceneCB.ReleaseAndGetAddressOf())
	);
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	result = _sceneCB->Map(0, nullptr, (void**)&_mappedScene);
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	return result;
}

HRESULT Dx12Wrapper::CreateTransformBufferView()
{
	// ディスクリプタヒープとCBV作成
	D3D12_DESCRIPTOR_HEAP_DESC basicHeapDesc = {};
	basicHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	basicHeapDesc.NodeMask = 0;
	basicHeapDesc.NumDescriptors = 1;
	basicHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	HRESULT result = _dev->CreateDescriptorHeap(&basicHeapDesc, IID_PPV_ARGS(_sceneDescHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	CD3DX12_CPU_DESCRIPTOR_HANDLE basicHeapHandle(_sceneDescHeap->GetCPUDescriptorHandleForHeapStart());

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = _sceneCB->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = (UINT)_sceneCB->GetDesc().Width;

	_dev->CreateConstantBufferView(
		&cbvDesc,
		basicHeapHandle
	);

	return result;
}

HRESULT Dx12Wrapper::CreatePostSetting()
{
	HRESULT result = _dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer((sizeof(PostSetting) + 0xff) & ~0xff),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_postSettingResource.ReleaseAndGetAddressOf())
	);
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	result = _postSettingResource->Map(0, nullptr, (void**)&_mappedPostSetting);
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	// ディスクリプタヒープとCBV作成
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.NodeMask = 0;
	desc.NumDescriptors = 1;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	result = _dev->CreateDescriptorHeap(&desc, IID_PPV_ARGS(_postSettingDH.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = _postSettingResource->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = (UINT)_postSettingResource->GetDesc().Width;

	_dev->CreateConstantBufferView(
		&cbvDesc,
		_postSettingDH->GetCPUDescriptorHandleForHeapStart()
	);

	return result;
}

ComPtr<ID3D12Resource> Dx12Wrapper::CreateGrayGradientTexture()
{
	//TODO: CreateXxxTexture()で共通の処理が多いので共通化したい

	// テクスチャバッファ作成
	D3D12_HEAP_PROPERTIES texHeapProp = CD3DX12_HEAP_PROPERTIES(
		D3D12_CPU_PAGE_PROPERTY_WRITE_BACK,
		D3D12_MEMORY_POOL_L0
	);

	D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_R8G8B8A8_UNORM,
		4,
		256
	);

	ComPtr<ID3D12Resource> texbuff = nullptr;
	HRESULT result = _dev->CreateCommittedResource(
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(texbuff.ReleaseAndGetAddressOf())
	);
	if (FAILED(result))
	{
		assert(false);
		return nullptr;
	}

	// 上が白くて下が黒いグレースケールグラデーションテクスチャの作成
	// 4byte 4x256のテクスチャ
	std::vector<unsigned int> data(4 * 256);
	// グレースケール値
	unsigned int grayscale = 0xff;
	for (auto it = data.begin(); it != data.end(); it += 4) // インクリメントは行単位
	{
		// グレースケール値をRGBA4チャンネルに適用したもの
		unsigned int grayscaleRGBA = (grayscale << 24) | (grayscale << 16) | (grayscale << 8) | grayscale;
		// 行の4ピクセル同時に塗る
		std::fill(it, it + 4, grayscaleRGBA);
		// グレースケール値を下げる
		--grayscale;
	}

	// テクスチャバッファへ作成したテクスチャデータを書き込み
	result = texbuff->WriteToSubresource(
		0,
		nullptr,
		data.data(),
		(UINT)(4 * sizeof(unsigned int)),
		(UINT)(sizeof(unsigned int) * data.size())
	);
	if (FAILED(result))
	{
		assert(false);
		return nullptr;
	}

	return texbuff;
}

ComPtr<ID3D12Resource> Dx12Wrapper::CreateWhiteTexture()
{
	//TODO: CreateXxxTexture()で共通の処理が多いので共通化したい

	// テクスチャバッファ作成
	D3D12_HEAP_PROPERTIES texHeapProp = CD3DX12_HEAP_PROPERTIES(
		D3D12_CPU_PAGE_PROPERTY_WRITE_BACK,
		D3D12_MEMORY_POOL_L0
	);

	D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_R8G8B8A8_UNORM,
		4,
		4
	);

	ComPtr<ID3D12Resource> texbuff = nullptr;
	HRESULT result = _dev->CreateCommittedResource(
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(texbuff.ReleaseAndGetAddressOf())
	);
	if (FAILED(result))
	{
		assert(false);
		return nullptr;
	}

	// 4byte 4x4のテクスチャ
	std::vector<unsigned char> data(4 * 4 * 4);
	// 0xffで埋めるためRGBAは(255, 255, 255, 255)になる
	std::fill(data.begin(), data.end(), 0xff);

	// テクスチャバッファへ作成したテクスチャデータを書き込み
	result = texbuff->WriteToSubresource(
		0,
		nullptr,
		data.data(),
		4 * 4,
		(UINT)data.size()
	);
	if (FAILED(result))
	{
		assert(false);
		return nullptr;
	}

	return texbuff;
}

ComPtr<ID3D12Resource> Dx12Wrapper::CreateBlackTexture()
{
	//TODO: CreateXxxTexture()で共通の処理が多いので共通化したい

	// テクスチャバッファ作成
	CD3DX12_HEAP_PROPERTIES texHeapProp(
		D3D12_CPU_PAGE_PROPERTY_WRITE_BACK,
		D3D12_MEMORY_POOL_L0
	);

	D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_R8G8B8A8_UNORM,
		4,
		4
	);

	ComPtr<ID3D12Resource> texbuff = nullptr;
	HRESULT result = _dev->CreateCommittedResource(
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(texbuff.ReleaseAndGetAddressOf())
	);
	if (FAILED(result))
	{
		assert(false);
		return nullptr;
	}

	// 4byte 4x4のテクスチャ
	std::vector<unsigned char> data(4 * 4 * 4);
	// 0x00で埋めるためRGBAは(0, 0, 0, 0)になる
	std::fill(data.begin(), data.end(), 0x00);

	// テクスチャバッファへ作成したテクスチャデータを書き込み
	result = texbuff->WriteToSubresource(
		0,
		nullptr,
		data.data(),
		4 * 4,
		(UINT)data.size()
	);
	if (FAILED(result))
	{
		assert(false);
		return nullptr;
	}

	return texbuff;
}

ComPtr<ID3D12Resource> Dx12Wrapper::GetGrayGradientTexture() const
{
	return _gradTex;
}

ComPtr<ID3D12Resource> Dx12Wrapper::GetWhiteTexture() const
{
	return _whiteTex;
}

ComPtr<ID3D12Resource> Dx12Wrapper::GetBlackTexture() const
{
	return _blackTex;
}

void Dx12Wrapper::SetDebugDisplay(bool flg)
{
	_mappedPostSetting->isDebugDisp = flg;
}

void Dx12Wrapper::SetSSAO(bool flg)
{
	_mappedPostSetting->isSSAO = flg;
}

void Dx12Wrapper::SetSelfShadow(bool flg)
{
	_isSelfShadow = flg;
}

void Dx12Wrapper::SetFov(float fov)
{
	_fov = fov;
}

void Dx12Wrapper::SetLightVector(float vec[3])
{
	_lightVec.x = vec[0];
	_lightVec.y = vec[1];
	_lightVec.z = vec[2];
}

void Dx12Wrapper::SetBackColor(float col[4])
{
	std::copy_n(col, 4, std::begin(_bgColor));
}

void Dx12Wrapper::SetBloomColor(float col[3])
{
	_mappedPostSetting->bloomColor.x = col[0];
	_mappedPostSetting->bloomColor.y = col[1];
	_mappedPostSetting->bloomColor.z = col[2];
}

void Dx12Wrapper::SetCameraSetting()
{
	// 定数バッファ用データ
	// 定数バッファ作成
	XMFLOAT3 eye(0, 15, -25);
	XMFLOAT3 target(0, 10, 0);
	XMFLOAT3 up(0, 1, 0);
	const XMMATRIX& viewMat = XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&target), XMLoadFloat3(&up));
	const SIZE& winSize = Application::Instance().GetWindowSize();
	const XMMATRIX& projMat = XMMatrixPerspectiveFovLH(
		_fov,
		(float)winSize.cx / (float)winSize.cy,
		1.0f,
		100.0f
	);

	_mappedScene->eye = eye;
	_mappedScene->view = viewMat;
	_mappedScene->proj = projMat;
	XMVECTOR det;
	_mappedScene->invviewproj = XMMatrixInverse(&det, viewMat * projMat);

	_mappedScene->lightVec.x = _lightVec.x;
	_mappedScene->lightVec.y = _lightVec.y;
	_mappedScene->lightVec.z = _lightVec.z;
	_mappedScene->lightVec.w = 0.0f;
	// w要素はアラインメントのためにXMFLOAT4にしただけなので使わない
	_mappedScene->isSelfShadow = _isSelfShadow;

	// 法線はY上方向、原点を通る平面
	XMFLOAT4 planeVec(0.0f, 1.0f, 0.0f, 0.0f);
	// ライトの方向ベクトルの逆方向
	const XMVECTOR& lightVec = -XMLoadFloat3(&_lightVec); // w要素は平行光源ということで0でいい
	_mappedScene->shadow = XMMatrixShadow(
		XMLoadFloat4(&planeVec),
		lightVec
	);

	const XMVECTOR& eyePos = XMLoadFloat3(&eye);
	const XMVECTOR& targetPos = XMLoadFloat3(&target);
	const XMVECTOR& upVec = XMLoadFloat3(&up);
	// ビュー行列のためにライトの位置を決めねばならないので、適当に、ターゲット位置からライト方向に、本カメラとターゲットの間の距離だけ伸ばした位置にしておく
	const XMVECTOR& lightPos = targetPos + XMVector3Normalize(lightVec) * XMVector3Length(XMVectorSubtract(targetPos, eyePos)).m128_f32[0];
	_mappedScene->lightCamera = XMMatrixLookAtLH(lightPos, targetPos, upVec) * XMMatrixOrthographicLH(40.0f, 40.0f, 1.0f, 100.0f);
}

// デプスバッファの場合はDSVとSRVの切り替えバリアが不要なのでPostDrawShadowは実装しない
void Dx12Wrapper::PreDrawShadow()
{
	// DSVをシャドウマップのものに指定する
	D3D12_CPU_DESCRIPTOR_HANDLE handle = _dsvHeap->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	// レンダーターゲットは指定しない
	_cmdList->OMSetRenderTargets(0, nullptr, false, &handle);

	_cmdList->ClearDepthStencilView(handle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	// カメラ情報CBV
	_cmdList->SetDescriptorHeaps(1, _sceneDescHeap.GetAddressOf());
	_cmdList->SetGraphicsRootDescriptorTable(0, _sceneDescHeap->GetGPUDescriptorHandleForHeapStart());

	const SIZE& winSize = Application::Instance().GetWindowSize();
	CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, (float)shadow_definition, (float)shadow_definition);
	_cmdList->RSSetViewports(1, &viewport);
	CD3DX12_RECT scissorrect = CD3DX12_RECT(0, 0, shadow_definition, shadow_definition);
	_cmdList->RSSetScissorRects(1, &scissorrect);
}

void Dx12Wrapper::PreDrawToPera1()
{
	// ペラ1をSRV状態からレンダーターゲット状態にする
	for (const ComPtr<ID3D12Resource>& res : _pera1Resources)
	{
		_cmdList->ResourceBarrier(
			1,
			&CD3DX12_RESOURCE_BARRIER::Transition(res.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
		);
	}
	// 高輝度バッファをSRV状態からレンダーターゲット状態にする
	_cmdList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(_bloomBuffers[0].Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
	);

	// レンダーターゲットをペラ1に指定する
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvs[3] = {};
	D3D12_CPU_DESCRIPTOR_HANDLE baseH = _peraRTVHeap->GetCPUDescriptorHandleForHeapStart();
	INT offset = 0;
	UINT incSize = _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	for (CD3DX12_CPU_DESCRIPTOR_HANDLE& rtv : rtvs)
	{
		rtv.InitOffsetted(baseH, offset);
		offset += incSize;
	}

	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvH(_dsvHeap->GetCPUDescriptorHandleForHeapStart());

	_cmdList->OMSetRenderTargets(_countof(rtvs), rtvs, false, &dsvH);

	// レンダーターゲットとデプスをクリアする
	for (int i = 0; i < _countof(rtvs); ++i)
	{
		if (i == 2)
		{
			// ブルーム用高輝度ではRGBは0でクリアする
			float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
			_cmdList->ClearRenderTargetView(rtvs[i], clearColor, 0, nullptr);
		}
		else
		{
			_cmdList->ClearRenderTargetView(rtvs[i], _bgColor, 0, nullptr);
		}
	}
	_cmdList->ClearDepthStencilView(dsvH, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	// カメラ情報CBV
	_cmdList->SetDescriptorHeaps(1, _sceneDescHeap.GetAddressOf());
	_cmdList->SetGraphicsRootDescriptorTable(0, _sceneDescHeap->GetGPUDescriptorHandleForHeapStart());

	// シャドウマップSRV
	_cmdList->SetDescriptorHeaps(1, _depthSRVHeap.GetAddressOf());
	D3D12_GPU_DESCRIPTOR_HANDLE handle = _depthSRVHeap->GetGPUDescriptorHandleForHeapStart();
	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	_cmdList->SetGraphicsRootDescriptorTable(3, handle); // 3はルートパラメータの番号

	const SIZE& winSize = Application::Instance().GetWindowSize();
	CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, (float)winSize.cx, (float)winSize.cy);
	_cmdList->RSSetViewports(1, &viewport);
	CD3DX12_RECT scissorrect = CD3DX12_RECT(0, 0, winSize.cx, winSize.cy);
	_cmdList->RSSetScissorRects(1, &scissorrect);
}

void Dx12Wrapper::PostDrawToPera1()
{
	// ペラ1をレンダーターゲット状態からSRV状態にする
	for (const ComPtr<ID3D12Resource>& res : _pera1Resources)
	{
		_cmdList->ResourceBarrier(
			1,
			&CD3DX12_RESOURCE_BARRIER::Transition(res.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
		);
	}
	// 高輝度バッファをレンダーターゲット状態からSRV状態にする
	_cmdList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(_bloomBuffers[0].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
	);
}

#if 0
void Dx12Wrapper::DrawHorizontalBokeh()
{
	// Separable Gaussian Blurの1パス目。

	// ペラ2をSRV状態からレンダーターゲット状態にする
	_cmdList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(_peraResources2.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
	);

	// レンダーターゲットをペラ2に指定する
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHeapPointer = _peraRTVHeap->GetCPUDescriptorHandleForHeapStart();
	rtvHeapPointer.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	_cmdList->OMSetRenderTargets(1, &rtvHeapPointer, false, nullptr);

	const SIZE& winSize = Application::Instance().GetWindowSize();
	CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, (float)winSize.cx, (float)winSize.cy);
	_cmdList->RSSetViewports(1, &viewport);
	CD3DX12_RECT scissorrect = CD3DX12_RECT(0, 0, winSize.cx, winSize.cy);
	_cmdList->RSSetScissorRects(1, &scissorrect);

	_cmdList->SetGraphicsRootSignature(_peraRS.Get());
	_cmdList->SetDescriptorHeaps(1, _peraCBVHeap.GetAddressOf());
	// ガウシアンウェイトのCBVをb0に設定
	_cmdList->SetGraphicsRootDescriptorTable(0, _peraCBVHeap->GetGPUDescriptorHandleForHeapStart());

	D3D12_GPU_DESCRIPTOR_HANDLE handle = _peraSRVHeap->GetGPUDescriptorHandleForHeapStart();
	// ペラ1のSRVをt0に設定
	_cmdList->SetDescriptorHeaps(1, _peraSRVHeap.GetAddressOf());
	_cmdList->SetGraphicsRootDescriptorTable(1, handle);

	_cmdList->SetPipelineState(_peraPipeline.Get());
	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	_cmdList->IASetVertexBuffers(0, 1, &_peraVBV);
	_cmdList->DrawInstanced(4, 1, 0, 0);

	// ペラ2をレンダーターゲット状態からSRV状態にする
	_cmdList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(_peraResources2.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
	);
}
#endif

void Dx12Wrapper::DrawShrinkTextureForBlur()
{
	_cmdList->SetPipelineState(_blurPipeline.Get());
	_cmdList->SetGraphicsRootSignature(_peraRS.Get());
	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	_cmdList->IASetVertexBuffers(0, 1, &_peraVBV);

	_cmdList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(_bloomBuffers[1].Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
	);

	_cmdList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(_dofBuffer.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
	);

	// 4枚目のRTVが縮小高輝度バッファ
	D3D12_CPU_DESCRIPTOR_HANDLE rtvBaseHandle = _peraRTVHeap->GetCPUDescriptorHandleForHeapStart();
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandles[2] = {};
	UINT rtvIncSize = _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	rtvHandles[0].InitOffsetted(rtvBaseHandle, rtvIncSize * 3); // 縮小高輝度バッファ
	rtvHandles[1].InitOffsetted(rtvBaseHandle, rtvIncSize * 4); // 被写界深度用縮小バッファ
	_cmdList->OMSetRenderTargets(2, rtvHandles, false, nullptr);

	float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
	_cmdList->ClearRenderTargetView(rtvHandles[0], clearColor, 0, nullptr);
	_cmdList->ClearRenderTargetView(rtvHandles[1], clearColor, 0, nullptr);

	// ガウシアンウェイトのb0
	_cmdList->SetDescriptorHeaps(1, _peraCBVHeap.GetAddressOf());
	_cmdList->SetGraphicsRootDescriptorTable(0, _peraCBVHeap->GetGPUDescriptorHandleForHeapStart());

	_cmdList->SetDescriptorHeaps(1, _peraSRVHeap.GetAddressOf());
	D3D12_GPU_DESCRIPTOR_HANDLE handle = _peraSRVHeap->GetGPUDescriptorHandleForHeapStart();
	_cmdList->SetGraphicsRootDescriptorTable(1, _peraSRVHeap->GetGPUDescriptorHandleForHeapStart());

	// 縮小高輝度バッファに1/2ずつ縮小していった8枚のガウシアンブラーを書き込んでいく
	D3D12_RESOURCE_DESC desc = _bloomBuffers[0]->GetDesc();
	D3D12_VIEWPORT vp = {};
	vp.MaxDepth = 1.0f;
	vp.MinDepth = 0.0f;
	vp.Width = desc.Width / 2.0f;
	vp.Height = desc.Height / 2.0f;
	D3D12_RECT sr = {};
	sr.top = 0;
	sr.left = 0;
	sr.right = (LONG)vp.Width;
	sr.bottom = (LONG)vp.Height;
	for (int i = 0; i < 8; ++i)
	{
		_cmdList->RSSetViewports(1, &vp);
		_cmdList->RSSetScissorRects(1, &sr);
		_cmdList->DrawInstanced(4, 1, 0, 0);

		// 位置を下にずらす
		sr.top += (LONG)vp.Height;
		vp.TopLeftX = 0;
		vp.TopLeftY = (float)sr.top;

		// 次は半分に縮小
		vp.Width /= 2;
		vp.Height /= 2;
		sr.bottom = sr.top + (LONG)vp.Height;
	}

	_cmdList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(_bloomBuffers[1].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
	);

	_cmdList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(_dofBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
	);
}

void Dx12Wrapper::DrawAmbientOcclusion()
{
	_cmdList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(_aoBuffer.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
	);

	D3D12_CPU_DESCRIPTOR_HANDLE rtvBaseHandle = _aoRTVDH->GetCPUDescriptorHandleForHeapStart();
	_cmdList->OMSetRenderTargets(1, &rtvBaseHandle, false, nullptr);
	_cmdList->SetGraphicsRootSignature(_peraRS.Get());

	const SIZE& winSize = Application::Instance().GetWindowSize();

	CD3DX12_VIEWPORT vp = CD3DX12_VIEWPORT(0.0f, 0.0f, (float)winSize.cx, (float)winSize.cy);
	_cmdList->RSSetViewports(1, &vp);

	CD3DX12_RECT rc = CD3DX12_RECT(0, 0, winSize.cx, winSize.cy);
	_cmdList->RSSetScissorRects(1, &rc);

	// 法線テクスチャSRV
	_cmdList->SetDescriptorHeaps(1, _peraSRVHeap.GetAddressOf());
	D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = _peraSRVHeap->GetGPUDescriptorHandleForHeapStart();
	_cmdList->SetGraphicsRootDescriptorTable(1, _peraSRVHeap->GetGPUDescriptorHandleForHeapStart());

	// 深度値テクスチャSRV
	_cmdList->SetDescriptorHeaps(1, _depthSRVHeap.GetAddressOf());
	_cmdList->SetGraphicsRootDescriptorTable(3, _depthSRVHeap->GetGPUDescriptorHandleForHeapStart());

	// カメラ情報CBV
	_cmdList->SetDescriptorHeaps(1, _sceneDescHeap.GetAddressOf());
	_cmdList->SetGraphicsRootDescriptorTable(0, _sceneDescHeap->GetGPUDescriptorHandleForHeapStart());

	_cmdList->SetPipelineState(_aoPipeline.Get());
	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	_cmdList->IASetVertexBuffers(0, 1, &_peraVBV);
	_cmdList->DrawInstanced(4, 1, 0, 0);

	_cmdList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(_aoBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
	);
}

void Dx12Wrapper::Draw()
{
	// Separable Gaussian Blurの2パス目。直接バックバッファに描画する
	UINT bbIdx = _swapchain->GetCurrentBackBufferIndex();

	// Present状態からレンダーターゲット状態にする
	_cmdList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(_backBuffers[bbIdx].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET)
	);

	// レンダーターゲットを指定する
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvH(_rtvHeaps->GetCPUDescriptorHandleForHeapStart());
	rtvH.ptr += bbIdx * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// パイプライン以外は1パス目のものを再利用できる。レンダーターゲット設定を変えるだけでいい
	_cmdList->OMSetRenderTargets(1, &rtvH, false, nullptr);

	const SIZE& winSize = Application::Instance().GetWindowSize();
	CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, (float)winSize.cx, (float)winSize.cy);
	_cmdList->RSSetViewports(1, &viewport);
	CD3DX12_RECT scissorrect = CD3DX12_RECT(0, 0, winSize.cx, winSize.cy);
	_cmdList->RSSetScissorRects(1, &scissorrect);

	_cmdList->SetGraphicsRootSignature(_peraRS.Get());
	// ガウシアンウェイトのb0
	_cmdList->SetDescriptorHeaps(1, _peraCBVHeap.GetAddressOf());
	_cmdList->SetGraphicsRootDescriptorTable(0, _peraCBVHeap->GetGPUDescriptorHandleForHeapStart());

	_cmdList->SetDescriptorHeaps(1, _peraSRVHeap.GetAddressOf());
	D3D12_GPU_DESCRIPTOR_HANDLE handle = _peraSRVHeap->GetGPUDescriptorHandleForHeapStart();
	// ペラ1、法線、高輝度、縮小高輝度、縮小カラーのSRVをt0t1t2t3t4に設定
#if 0 // ペラ2のSRVをt0に使う場合
	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
#endif
	_cmdList->SetGraphicsRootDescriptorTable(1, handle);

	// ディストーションテクスチャのSRVをt5に設定
	_cmdList->SetDescriptorHeaps(1, _distortionSRVHeap.GetAddressOf());
	_cmdList->SetGraphicsRootDescriptorTable(2, _distortionSRVHeap->GetGPUDescriptorHandleForHeapStart());

	// 深度値テクスチャのSRVとシャドウマップのSRVをt6t7に設定
	_cmdList->SetDescriptorHeaps(1, _depthSRVHeap.GetAddressOf());
	_cmdList->SetGraphicsRootDescriptorTable(3, _depthSRVHeap->GetGPUDescriptorHandleForHeapStart());

	// SSAOテクスチャSRVをt8に設定
	_cmdList->SetDescriptorHeaps(1, _aoSRVDH.GetAddressOf());
	_cmdList->SetGraphicsRootDescriptorTable(4, _aoSRVDH->GetGPUDescriptorHandleForHeapStart());


	// ポストプロセスセッティングCBVをb1に設定
	_cmdList->SetDescriptorHeaps(1, _postSettingDH.GetAddressOf());
	_cmdList->SetGraphicsRootDescriptorTable(5, _postSettingDH->GetGPUDescriptorHandleForHeapStart());

#if 1
	_cmdList->SetPipelineState(_peraPipeline.Get());
#else
	// ペラ2を使うパスは今は使わないのでコメントアウト
	_cmdList->SetPipelineState(_peraPipeline2.Get());
#endif
	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	_cmdList->IASetVertexBuffers(0, 1, &_peraVBV);
	_cmdList->DrawInstanced(4, 1, 0, 0);
}

void Dx12Wrapper::Flip()
{
	UINT bbIdx = _swapchain->GetCurrentBackBufferIndex();

	// レンダーターゲット状態からPresent状態にする
	_cmdList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(_backBuffers[bbIdx].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT)
	);

	// コマンドリストのクローズ
	_cmdList->Close();

	// コマンドリストの実行
	ID3D12CommandList* cmdlists[] = { _cmdList.Get() };
	_cmdQueue->ExecuteCommandLists(1, cmdlists);
	_cmdQueue->Signal(_fence.Get(), ++_fenceVal);

	if (_fence->GetCompletedValue() != _fenceVal)
	{
		HANDLE event = CreateEvent(nullptr, false, false, nullptr);
		_fence->SetEventOnCompletion(_fenceVal, event);
		WaitForSingleObject(event, INFINITE);
		CloseHandle(event);
	}

	// コマンドリストのクリア
	_cmdAllocator->Reset();
	_cmdList->Reset(_cmdAllocator.Get(), nullptr);

	// スワップ
	_swapchain->Present(1, 0);
}

