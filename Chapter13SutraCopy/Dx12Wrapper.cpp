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

using namespace DirectX;
using namespace Microsoft::WRL;

namespace
{
	std::vector<float> GetGaussianWeights(size_t count, float s)
	{
		// ���E�Ώ̂Ȃ̂ŁA�v�f0�͒��S�s�N�Z���Ƃ��A�E�����̂݊i�[���A�������͊i�[���Ȃ�
		std::vector<float> weights(count);

		float x = 0.0f;
		float total = 0.0f;

		for (float& wgt : weights)
		{
			wgt = expf(-(x * x) / (2 * s * s));
			total += wgt;
			x += 1.0f;
		}

		// ���E�Ώ̂Ȃ̂Ŋ��鐔��2�{����B
		// ���S�̗v�f��2�񕪓���̂ŁA�����e~0=1���Ƃ킩���Ă���̂�
		// �����Ђ�
		total = total * 2.0f - 1.0f;

		for (float& wgt : weights)
		{
			wgt /= total;
		}

		return weights;
	}

	std::wstring GetWideStringFromString(const std::string& str)
	{
		// MultiByteToWideChar�Ŏg���ɂ͐��wchar_t�z���K�v�ȃT�C�Y�̊m�ۂ��K�v�B
		// �Œ蒷�z���Ԃ��Ă�������std::wstring�̕��������₷���̂�
		// ��ɒ������擾����resize���Ă���

		// �����̕�����̒����擾
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

std::string Dx12Wrapper::GetExtension(const std::string& path)
{
	size_t idx = path.rfind('.');
	return path.substr(idx + 1, path.length() - idx - 1);
}

ComPtr<ID3D12Resource> Dx12Wrapper::LoadTextureFromFile(const std::string& texPath)
{
	// �L���b�V���ς݂Ȃ炻���Ԃ�
	// �C�e���[�^�̌^�͕��G�Ȃ̂�auto���g��
	auto it = _resourceTable.find(texPath);
	if (it != _resourceTable.end())
	{
		return _resourceTable[texPath];
	}

	// WIC�e�N�X�`���̃��[�h
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
		// �w�肳�ꂽ�p�X�Ƀt�@�C�������݂��Ȃ��P�[�X�͑z�肵�Ă���̂�assert�͂��Ȃ�
		return nullptr;
	}

	const Image* img = scratchImg.GetImage(0, 0, 0);

	// �e�N�X�`���o�b�t�@�쐬
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

	// �e�N�X�`���o�b�t�@�֍쐬�����e�N�X�`���f�[�^����������
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

	// �e�[�u���ɃL���b�V��
	_resourceTable[texPath] = texbuff;
	return texbuff;
}

struct SceneMatrix
{
	XMMATRIX view;
	XMMATRIX proj;
	XMMATRIX shadow;
	XMFLOAT3 eye;
};

Dx12Wrapper::Dx12Wrapper(HWND hwnd)
{
#ifdef _DEBUG
	EnableDebugLayer();
#endif // _DEBUG

	// �e�N�X�`�����[�_�[�֐��e�[�u���쐬
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

	// �X���b�v�`�F�C���̐����B��֐������Ă΂Ȃ��̂Ŋ֐������Ȃ�
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

	result = CreateDepthStencil();
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

	result = CreateBokehParamResouce();
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

	// �t�F���X�̐���
	result = _dev->CreateFence(_fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(_fence.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(false);
		return;
	}

	result = CreateCameraConstantBuffer();
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
	// DXGIFactory�̐���
	HRESULT result = CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(_dxgiFactory.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	// NVIDIA�A�_�v�^�̑I��
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

	// Direct3D�f�o�C�X�̏�����
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
	// �R�}���h�A���P�[�^�ƃR�}���h���X�g�̐���
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

	// �R�}���h�L���[�̐���
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
	// �o�b�t�@��ID3D12Resource�������ō��Ȃ��Ă��A�X���b�v�`�F�C���쐬����
	// �����ɍ���ăX���b�v�`�F�C���ɕێ�����Ă���
	// RenderTargetView��2���̃o�b�t�@�ɍ��B

	// �f�B�X�N���v�^�q�[�v�̐����ƁA2���̃o�b�t�@�p�̃����_�[�^�[�Q�b�g�r���[�̐���
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

	// SRGB�e�N�X�`���Ή�
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
		{{-1.0f, -1.0f, 0.1f}, {0.0f, 1.0f}}, // ����
		{{-1.0f, 1.0f, 0.1f}, {0.0f, 0.0f}}, // ����
		{{1.0f, -1.0f, 0.1f}, {1.0f, 1.0f}}, // �E��
		{{1.0f, 1.0f, 0.1f}, {1.0f, 0.0f}} // �E��
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

	// ���_�o�b�t�@�[�r���[
	_peraVBV.BufferLocation = _peraVB->GetGPUVirtualAddress();
	_peraVBV.SizeInBytes = (UINT)sizeof(pv);
	_peraVBV.StrideInBytes = sizeof(PeraVertex);

	// ���_�o�b�t�@�ւ̃f�[�^��������
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

HRESULT Dx12Wrapper::CreateDepthSRV()
{
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heapDesc.NumDescriptors = 1;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	HRESULT result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(_depthSRVHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	assert(_depthBuffer != nullptr);
	//D3D12_RESOURCE_DESC desc = _depthBuffer->GetDesc();

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	// ����̓f�v�X�o�b�t�@�����������DXGI_FORMAT_R32_TYPELESS�ł̓G���[�ɂȂ�
	//srvDesc.Format = desc.Format;
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	_dev->CreateShaderResourceView(
		_depthBuffer.Get(),
		&srvDesc,
		_depthSRVHeap->GetCPUDescriptorHandleForHeapStart()
	);

	return result;
}

HRESULT Dx12Wrapper::CreateBokehParamResouce()
{
	// �������܂߂ĕЕ���8�s�N�Z�����B�W���΍���5�s�N�Z���B
	const std::vector<float>& weights = GetGaussianWeights(8, 5.0f);
	// TODO:�{�̂悤�Ɋ֐���������
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

	float* mappedWeight = nullptr;
	result = _bokehParamResource->Map(0, nullptr, (void**)&mappedWeight);
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	std::copy(weights.begin(), weights.end(), mappedWeight);
	_bokehParamResource->Unmap(0, nullptr);

	// CBV�̓y����SRV�Ƃ܂Ƃ߂�̂ł����ł͍��Ȃ�

	return result;
}

HRESULT Dx12Wrapper::CreatePeraResouceAndView()
{
	// FinalRenderTarget�Ɠ����ݒ�̃o�b�t�@���y���p�Ƃ���2�����

	ComPtr<ID3D12Resource> bbuff = _backBuffers[0];
	D3D12_RESOURCE_DESC resDesc = bbuff->GetDesc();

	const D3D12_HEAP_PROPERTIES& heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	float clsClr[4] = {0.5f, 0.5f, 0.5f, 1.0f};
	D3D12_CLEAR_VALUE clearValue = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_R8G8B8A8_UNORM, clsClr);

	HRESULT result = _dev->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&clearValue,
		IID_PPV_ARGS(_peraResource.ReleaseAndGetAddressOf())
	);
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	result = _dev->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&clearValue,
		IID_PPV_ARGS(_peraResource2.ReleaseAndGetAddressOf())
	);
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	// RTV�p�q�[�v�B������_�u���o�b�t�@�̐ݒ���g���B
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = _rtvHeaps->GetDesc();
	heapDesc.NumDescriptors = 2;
	result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(_peraRTVHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	// RTV2���쐬
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	D3D12_CPU_DESCRIPTOR_HANDLE handle = _peraRTVHeap->GetCPUDescriptorHandleForHeapStart();
	_dev->CreateRenderTargetView(_peraResource.Get(), &rtvDesc, handle);

	// TODO:�{�ł͂����Ȃ��Ă��邪RTV���������̂ł�
	//handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	_dev->CreateRenderTargetView(_peraResource2.Get(), &rtvDesc, handle);

	// �K�E�V�A���u���[�̃E�F�C�gCBV�ƃ����_�[�e�N�X�`��SRV�p�q�[�v
	heapDesc.NumDescriptors = 3;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(_peraRegisterHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	handle = _peraRegisterHeap->GetCPUDescriptorHandleForHeapStart();

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = _bokehParamResource->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = (UINT)_bokehParamResource->GetDesc().Width;
	_dev->CreateConstantBufferView(
		&cbvDesc,
		handle
	);

	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = rtvDesc.Format;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	_dev->CreateShaderResourceView(
		_peraResource.Get(),
		&srvDesc,
		handle
	);

	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	_dev->CreateShaderResourceView(
		_peraResource2.Get(),
		&srvDesc,
		handle
	);

	return result;
}

bool Dx12Wrapper::CheckResult(HRESULT result, ID3DBlob* error) {
	if (FAILED(result)) {
		if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
			::OutputDebugStringA("�t�@�C������������܂���");
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
	D3D12_DESCRIPTOR_RANGE range[4] = {};
	// �K�E�V�A���E�F�C�gCBV��b0
	range[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	range[0].BaseShaderRegister = 0;
	range[0].NumDescriptors = 1;
	// �y��1�A2SRV��t0
	range[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	range[1].BaseShaderRegister = 0;
	range[1].NumDescriptors = 1;
	// �f�B�X�g�[�V�����e�N�X�`��SVR��t1
	range[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	range[2].BaseShaderRegister = 1;
	range[2].NumDescriptors = 1;
	// �[�x�l�e�N�X�`��SRV��t2
	range[3].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	range[3].BaseShaderRegister = 2;
	range[3].NumDescriptors = 1;

	D3D12_ROOT_PARAMETER rp[4] = {};
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

	D3D12_STATIC_SAMPLER_DESC sampler = CD3DX12_STATIC_SAMPLER_DESC(0);

	D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
	rsDesc.NumParameters = 4;
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
#if 0 // �����K�E�V�A���u���[
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
	
#if 0 // �|�X�g�v���Z�X�Ȃ�
	result = D3DCompileFromFile(L"PeraPixelShader.hlsl",
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"PeraPS", "ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0, psBlob.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf());
#elif 0 // �P�x�ɂ��O���[�X�P�[��
	result = D3DCompileFromFile(L"PeraPixelShader.hlsl",
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"PeraGrayscalePS", "ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0, psBlob.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf());
#elif 0 // �F���]
	result = D3DCompileFromFile(L"PeraPixelShader.hlsl",
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"PeraInverseColorPS", "ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0, psBlob.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf());
#elif 0 // �F�~����������
	result = D3DCompileFromFile(L"PeraPixelShader.hlsl",
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"PeraDownToneLevelPS", "ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0, psBlob.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf());
#elif 0 // �ߖT9�s�N�Z���ŕ��ς��Ƃ�
	result = D3DCompileFromFile(L"PeraPixelShader.hlsl",
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"Pera9AveragePS", "ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0, psBlob.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf());
#elif 0 // �G���{�X���H
	result = D3DCompileFromFile(L"PeraPixelShader.hlsl",
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"PeraEmbossPS", "ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0, psBlob.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf());
#elif 0 // �V���[�v�l�X
	result = D3DCompileFromFile(L"PeraPixelShader.hlsl",
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"PeraSharpnessPS", "ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0, psBlob.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf());
#elif 0 // �G�b�W���o
	result = D3DCompileFromFile(L"PeraPixelShader.hlsl",
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"PeraEdgeDetectionPS", "ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0, psBlob.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf());
#elif 0 // �K�E�V�A���u���[
	result = D3DCompileFromFile(L"PeraPixelShader.hlsl",
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"PeraGaussianBlurPS", "ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0, psBlob.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf());
#elif 0 // �����K�E�V�A���u���[
	result = D3DCompileFromFile(L"PeraPixelShader.hlsl",
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"PeraVerticalBokehPS", "ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0, psBlob.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf());
#elif 0 // �����K�E�V�A���u���[�{�f�B�X�g�[�V����
	result = D3DCompileFromFile(L"PeraPixelShader.hlsl",
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"PeraVerticalBokehAndDistortionPS", "ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0, psBlob.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf());
#elif 1 // �f�v�X�f�o�b�O�\��
	result = D3DCompileFromFile(L"PeraPixelShader.hlsl",
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"PeraDepthDebugPS", "ps_5_0",
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

	return result;
}

HRESULT Dx12Wrapper::CreateDepthStencil()
{
	// �[�x�o�b�t�@�쐬
	const SIZE& winSize = Application::Instance().GetWindowSize();
	D3D12_RESOURCE_DESC depthResDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		//DXGI_FORMAT_D32_FLOAT,
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

	// D3D12_RESOURCE_DESC��DXGI_FORMAT_R32_TYPELESS�ɂ��Ă��A
	// D3D12_CLEAR_VALUE��DXGI_FORMAT_R32_TYPELESS�ɂ���ƃG���[�ɂȂ�
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
	
	// �f�v�X�X�e���V���r���[�쐬
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	result = _dev->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(_dsvHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	_dev->CreateDepthStencilView(_depthBuffer.Get(), &dsvDesc, _dsvHeap->GetCPUDescriptorHandleForHeapStart());

	return result;
}

HRESULT Dx12Wrapper::CreateCameraConstantBuffer()
{
	// �萔�o�b�t�@�p�f�[�^
	// �萔�o�b�t�@�쐬
	XMFLOAT3 eye(0, 15, -25);
	XMFLOAT3 target(0, 10, 0);
	XMFLOAT3 up(0, 1, 0);
	XMMATRIX viewMat = XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&target), XMLoadFloat3(&up));
	const SIZE& winSize = Application::Instance().GetWindowSize();
	XMMATRIX projMat = XMMatrixPerspectiveFovLH(
		XM_PIDIV4,
		(float)winSize.cx / (float)winSize.cy,
		1.0f,
		100.0f
	);

	HRESULT result = _dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer((sizeof(SceneMatrix) + 0xff) & ~0xff),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_sceneConstBuff.ReleaseAndGetAddressOf())
	);
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	result = _sceneConstBuff->Map(0, nullptr, (void**)&_mappedScene);
	if (FAILED(result))
	{
		assert(false);
		return result;
	}
	_mappedScene->view = viewMat;
	_mappedScene->proj = projMat;
	_mappedScene->eye = eye;

	// �@����Y������A���_��ʂ镽��
	XMFLOAT4 planeVec(0.0f, 1.0f, 0.0f, 0.0f);
	XMFLOAT3 parallelLightVec(1.0f, -1.0f, 1.0f);
	_mappedScene->shadow = XMMatrixShadow(
		XMLoadFloat4(&planeVec),
		-XMLoadFloat3(&parallelLightVec) // w�v�f�͕��s�����Ƃ������Ƃ�0�ł���
	);

	// �f�B�X�N���v�^�q�[�v��CBV�쐬
	D3D12_DESCRIPTOR_HEAP_DESC basicHeapDesc = {};
	basicHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	basicHeapDesc.NodeMask = 0;
	basicHeapDesc.NumDescriptors = 1;
	basicHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	result = _dev->CreateDescriptorHeap(&basicHeapDesc, IID_PPV_ARGS(_sceneDescHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	CD3DX12_CPU_DESCRIPTOR_HANDLE basicHeapHandle(_sceneDescHeap->GetCPUDescriptorHandleForHeapStart());

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = _sceneConstBuff->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = (UINT)_sceneConstBuff->GetDesc().Width;

	_dev->CreateConstantBufferView(
		&cbvDesc,
		basicHeapHandle
	);

	return result;
}

ComPtr<ID3D12Resource> Dx12Wrapper::CreateGrayGradientTexture()
{
	//TODO: CreateXxxTexture()�ŋ��ʂ̏����������̂ŋ��ʉ�������

	// �e�N�X�`���o�b�t�@�쐬
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

	// �オ�����ĉ��������O���[�X�P�[���O���f�[�V�����e�N�X�`���̍쐬
	// 4byte 4x256�̃e�N�X�`��
	std::vector<unsigned int> data(4 * 256);
	// �O���[�X�P�[���l
	unsigned int grayscale = 0xff;
	for (auto it = data.begin(); it != data.end(); it += 4) // �C���N�������g�͍s�P��
	{
		// �O���[�X�P�[���l��RGBA4�`�����l���ɓK�p��������
		unsigned int grayscaleRGBA = (grayscale << 24) | (grayscale << 16) | (grayscale << 8) | grayscale;
		// �s��4�s�N�Z�������ɓh��
		std::fill(it, it + 4, grayscaleRGBA);
		// �O���[�X�P�[���l��������
		--grayscale;
	}

	// �e�N�X�`���o�b�t�@�֍쐬�����e�N�X�`���f�[�^����������
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
	//TODO: CreateXxxTexture()�ŋ��ʂ̏����������̂ŋ��ʉ�������

	// �e�N�X�`���o�b�t�@�쐬
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

	// 4byte 4x4�̃e�N�X�`��
	std::vector<unsigned char> data(4 * 4 * 4);
	// 0xff�Ŗ��߂邽��RGBA��(255, 255, 255, 255)�ɂȂ�
	std::fill(data.begin(), data.end(), 0xff);

	// �e�N�X�`���o�b�t�@�֍쐬�����e�N�X�`���f�[�^����������
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
	//TODO: CreateXxxTexture()�ŋ��ʂ̏����������̂ŋ��ʉ�������

	// �e�N�X�`���o�b�t�@�쐬
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

	// 4byte 4x4�̃e�N�X�`��
	std::vector<unsigned char> data(4 * 4 * 4);
	// 0x00�Ŗ��߂邽��RGBA��(0, 0, 0, 0)�ɂȂ�
	std::fill(data.begin(), data.end(), 0x00);

	// �e�N�X�`���o�b�t�@�֍쐬�����e�N�X�`���f�[�^����������
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

ComPtr<ID3D12Resource> Dx12Wrapper::GetGrayGradientTexture()
{
	return _gradTex;
}

ComPtr<ID3D12Resource> Dx12Wrapper::GetWhiteTexture()
{
	return _whiteTex;
}

ComPtr<ID3D12Resource> Dx12Wrapper::GetBlackTexture()
{
	return _blackTex;
}

void Dx12Wrapper::PreDrawToPera1()
{
	// �y��1��SRV��Ԃ��烌���_�[�^�[�Q�b�g��Ԃɂ���
	_cmdList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(_peraResource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
	);

	// �����_�[�^�[�Q�b�g���y��1�Ɏw�肷��
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHeapPointer = _peraRTVHeap->GetCPUDescriptorHandleForHeapStart();

	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvH(_dsvHeap->GetCPUDescriptorHandleForHeapStart());

	_cmdList->OMSetRenderTargets(1, &rtvHeapPointer, false, &dsvH);

	// �����_�[�^�[�Q�b�g���N���A����
	float clearColor[] = {0.5f, 0.5f, 0.5f, 1.0f};
	_cmdList->ClearRenderTargetView(rtvHeapPointer, clearColor, 0, nullptr);
	_cmdList->ClearDepthStencilView(dsvH, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	const SIZE& winSize = Application::Instance().GetWindowSize();
	CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, (float)winSize.cx, (float)winSize.cy);
	_cmdList->RSSetViewports(1, &viewport);
	CD3DX12_RECT scissorrect = CD3DX12_RECT(0, 0, winSize.cx, winSize.cy);
	_cmdList->RSSetScissorRects(1, &scissorrect);
}

void Dx12Wrapper::PostDrawToPera1()
{
	// �y��1�������_�[�^�[�Q�b�g��Ԃ���SRV��Ԃɂ���
	_cmdList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(_peraResource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
	);
}

void Dx12Wrapper::SetCamera()
{
	ID3D12DescriptorHeap* bdh[] = {_sceneDescHeap.Get()};
	_cmdList->SetDescriptorHeaps(1, bdh);
	_cmdList->SetGraphicsRootDescriptorTable(0, _sceneDescHeap->GetGPUDescriptorHandleForHeapStart());
}

void Dx12Wrapper::DrawHorizontalBokeh()
{
	// Separable Gaussian Blur��1�p�X�ځB

	// �y��2��SRV��Ԃ��烌���_�[�^�[�Q�b�g��Ԃɂ���
	_cmdList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(_peraResource2.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
	);

	// �����_�[�^�[�Q�b�g���y��2�Ɏw�肷��
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHeapPointer = _peraRTVHeap->GetCPUDescriptorHandleForHeapStart();
	rtvHeapPointer.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	_cmdList->OMSetRenderTargets(1, &rtvHeapPointer, false, nullptr);

	// �����_�[�^�[�Q�b�g���N���A����
	float clearColor[] = {0.5f, 0.5f, 0.5f, 1.0f};
	_cmdList->ClearRenderTargetView(rtvHeapPointer, clearColor, 0, nullptr);

	const SIZE& winSize = Application::Instance().GetWindowSize();
	CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, (float)winSize.cx, (float)winSize.cy);
	_cmdList->RSSetViewports(1, &viewport);
	CD3DX12_RECT scissorrect = CD3DX12_RECT(0, 0, winSize.cx, winSize.cy);
	_cmdList->RSSetScissorRects(1, &scissorrect);

	_cmdList->SetGraphicsRootSignature(_peraRS.Get());
	_cmdList->SetDescriptorHeaps(1, _peraRegisterHeap.GetAddressOf());

	D3D12_GPU_DESCRIPTOR_HANDLE handle = _peraRegisterHeap->GetGPUDescriptorHandleForHeapStart();
	// �K�E�V�A���E�F�C�g��CBV��b0�ɐݒ�
	_cmdList->SetGraphicsRootDescriptorTable(0, handle);

	// �y��1��SRV��t0�ɐݒ�
	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	_cmdList->SetGraphicsRootDescriptorTable(1, handle);

	_cmdList->SetPipelineState(_peraPipeline.Get());
	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	_cmdList->IASetVertexBuffers(0, 1, &_peraVBV);
	_cmdList->DrawInstanced(4, 1, 0, 0);

	// �y��2�������_�[�^�[�Q�b�g��Ԃ���SRV��Ԃɂ���
	_cmdList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(_peraResource2.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
	);
}

void Dx12Wrapper::Draw()
{
	// Separable Gaussian Blur��2�p�X�ځB���ڃo�b�N�o�b�t�@�ɕ`�悷��
	UINT bbIdx = _swapchain->GetCurrentBackBufferIndex();

	// Present��Ԃ��烌���_�[�^�[�Q�b�g��Ԃɂ���
	_cmdList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(_backBuffers[bbIdx].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET)
	);

	// �����_�[�^�[�Q�b�g���w�肷��
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvH(_rtvHeaps->GetCPUDescriptorHandleForHeapStart());
	rtvH.ptr += bbIdx * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// �p�C�v���C���ȊO��1�p�X�ڂ̂��̂��ė��p�ł���B�����_�[�^�[�Q�b�g�ݒ��ς��邾���ł���
	_cmdList->OMSetRenderTargets(1, &rtvH, false, nullptr);

	// �����_�[�^�[�Q�b�g���N���A����
	float clearColor[] = {0.2f, 0.5f, 0.5f, 1.0f};
	_cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);

	const SIZE& winSize = Application::Instance().GetWindowSize();
	CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, (float)winSize.cx, (float)winSize.cy);
	_cmdList->RSSetViewports(1, &viewport);
	CD3DX12_RECT scissorrect = CD3DX12_RECT(0, 0, winSize.cx, winSize.cy);
	_cmdList->RSSetScissorRects(1, &scissorrect);

	_cmdList->SetGraphicsRootSignature(_peraRS.Get());
	_cmdList->SetDescriptorHeaps(1, _peraRegisterHeap.GetAddressOf());

	D3D12_GPU_DESCRIPTOR_HANDLE handle = _peraRegisterHeap->GetGPUDescriptorHandleForHeapStart();
	// �K�E�V�A���E�F�C�g��CBV��b0�ɐݒ�
	_cmdList->SetGraphicsRootDescriptorTable(0, handle);

	// �y��2��SRV��t0�ɐݒ�
	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	_cmdList->SetGraphicsRootDescriptorTable(1, handle);

	// �f�B�X�g�[�V�����e�N�X�`����SRV��t1�ɐݒ�
	_cmdList->SetDescriptorHeaps(1, _distortionSRVHeap.GetAddressOf());
	_cmdList->SetGraphicsRootDescriptorTable(2, _distortionSRVHeap->GetGPUDescriptorHandleForHeapStart());

	// �[�x�l�e�N�X�`����SRV��t2�ɐݒ�
	_cmdList->SetDescriptorHeaps(1, _depthSRVHeap.GetAddressOf());
	_cmdList->SetGraphicsRootDescriptorTable(3, _depthSRVHeap->GetGPUDescriptorHandleForHeapStart());

	_cmdList->SetPipelineState(_peraPipeline2.Get());
	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	_cmdList->IASetVertexBuffers(0, 1, &_peraVBV);
	_cmdList->DrawInstanced(4, 1, 0, 0);
}

void Dx12Wrapper::Flip()
{
	UINT bbIdx = _swapchain->GetCurrentBackBufferIndex();

	// �����_�[�^�[�Q�b�g��Ԃ���Present��Ԃɂ���
	_cmdList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(_backBuffers[bbIdx].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT)
	);

	// �R�}���h���X�g�̃N���[�Y
	_cmdList->Close();

	// �R�}���h���X�g�̎��s
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

	// �R�}���h���X�g�̃N���A
	_cmdAllocator->Reset();
	_cmdList->Reset(_cmdAllocator.Get(), nullptr);

	// �X���b�v
	_swapchain->Present(1, 0);
}

