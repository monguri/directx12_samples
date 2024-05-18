#include "Dx12Wrapper.h"
#include"Application.h"
#include<DirectXMath.h>
#include<d3dcompiler.h>
#include<cassert>
#include<memory>
#include"PMDActor.h"
#include"PMDRenderer.h"
#include<d3dx12.h>
#include"Helper.h"
#include"PrimitiveActor.h"


#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib,"DirectXTex.lib")
#pragma comment(lib,"dxguid.lib")


using namespace DirectX;
using namespace std;

namespace {
	enum class DepthType {//�[�x�^�C�v
		defaultDepth=0,//���ʕ`��Ɏg�p����[�x
		lightDepth=1,//�V���h�E�}�b�v�Ɏg�p���郉�C�g����̐[�x
	};
}

constexpr uint32_t shadow_difinition = 1024;

bool
Dx12Wrapper::CreateConstantBufferForPera() {
	D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

	auto gWeights = GetGaussianValues(5.0f, 8);

	D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(
		AligmentedValue(gWeights.size()*sizeof(gWeights[0]), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)
	);

	auto result = _dev->CreateCommittedResource(&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_peraCB.ReleaseAndGetAddressOf()));

	if (!CheckResult(result)) {
		assert(0);
		return false;
	}

	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
	descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descriptorHeapDesc.NodeMask = 0;
	descriptorHeapDesc.NumDescriptors = 1;
	descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	result = _dev->CreateDescriptorHeap(&descriptorHeapDesc,
		IID_PPV_ARGS(&_peraCBVHeap));

	if (!CheckResult(result)) {
		assert(0);
		return false;
	}

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = _peraCB->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = resDesc.Width;
	_dev->CreateConstantBufferView(&cbvDesc, _peraCBVHeap->GetCPUDescriptorHandleForHeapStart());

	float* mappedWeights = nullptr;
	result = _peraCB->Map(0, nullptr, (void**)&mappedWeights);
	copy(gWeights.begin(), gWeights.end(), mappedWeights);
	_peraCB->Unmap(0, nullptr);
	if (!CheckResult(result)) {
		assert(0);
		return false;
	}

	return true;

}

Dx12Wrapper::Dx12Wrapper(HWND hwnd): _hwnd(hwnd),
	_eye(0,15,-25),
	_target(0,10,0),
	_up(0,1,0)
{
	

}


Dx12Wrapper::~Dx12Wrapper()
{
}

bool
Dx12Wrapper::CreateDepthSRVForTest() {
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 2;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	auto result = _dev->CreateDescriptorHeap(&heapDesc,IID_PPV_ARGS(&_depthSRVHeap));
	if (!CheckResult(result)) {
		return false;
	}
	D3D12_SHADER_RESOURCE_VIEW_DESC resDesc = {};
	resDesc.Format = DXGI_FORMAT_R32_FLOAT;
	resDesc.Texture2D.MipLevels = 1;
	resDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	resDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

	auto handle=_depthSRVHeap->GetCPUDescriptorHandleForHeapStart();

	_dev->CreateShaderResourceView(_depthBuffer.Get(),
		&resDesc,
		handle);

	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	_dev->CreateShaderResourceView(_lightDepthBuffer.Get(),
		&resDesc,
		handle);

	return true;
}

bool 
Dx12Wrapper::Init() {
	ID3D12Debug* debug;
	D3D12GetDebugInterface(IID_PPV_ARGS(&debug));
	debug->EnableDebugLayer();
	debug->Release();

	HRESULT result=S_OK;
	D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};
	for (auto level : levels) {
		if (SUCCEEDED(
			D3D12CreateDevice(
				nullptr,
				level,
				IID_PPV_ARGS(&_dev)
			))) {
			break;
		}
	}
	if (_dev == nullptr) {
		return false;
	}

	if (FAILED(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG,IID_PPV_ARGS(_dxgiFactory.ReleaseAndGetAddressOf())))) {
		return false;
	}

	//�R�}���h�L���[�̍쐬
	D3D12_COMMAND_QUEUE_DESC cmdQDesc = {};
	cmdQDesc.NodeMask = 0;//�A�_�v�^�[�}�X�N�Ȃ�
	cmdQDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	cmdQDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	cmdQDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	if (FAILED(_dev->CreateCommandQueue(&cmdQDesc, IID_PPV_ARGS(_cmdQue.ReleaseAndGetAddressOf())))) {
		return false;
	}

	Application& app = Application::Instance();
	auto wsize = app.GetWindowSize();
	DXGI_SWAP_CHAIN_DESC1 swDesc = {};
	swDesc.BufferCount = 2;//�t���b�v�Ɏg�p���鎆�͂Q��
	swDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swDesc.Flags = 0;
	swDesc.Width = wsize.width;
	swDesc.Height = wsize.height;
	swDesc.SampleDesc.Count = 1;
	swDesc.SampleDesc.Quality = 0;
	swDesc.Scaling = DXGI_SCALING_STRETCH;
	swDesc.Stereo = false;
	swDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	result = _dxgiFactory->CreateSwapChainForHwnd(
		_cmdQue.Get(),
		_hwnd,
		&swDesc,
		nullptr,
		nullptr,
		(IDXGISwapChain1**)(_swapchain.ReleaseAndGetAddressOf())
	);
	if (FAILED(result)) {
		return false;
	}

	if (!CreateRenderTargetView()) {
		return false;
	}

	//�[�x�o�b�t�@�쐬
	if (!CreateDepthBuffer()) {
		return false;
	}
	//�[�x�o�b�t�@�r���[�쐬
	if (!CreateDSV()) {
		return false;
	}
	//�[�x�o�b�t�@���摜�Ƃ��Ďg���r���[���쐬
	if (!CreateDepthSRVForTest()) {
		return false;
	}

	//�R�}���h���X�g�̍쐬
	if (!CreateCommandList()) {
		return false;
	}

	//�t�F���X�̍쐬
	_fenceValue = 0;//��r�p
	result = _dev->CreateFence(
		_fenceValue, //�����l
		D3D12_FENCE_FLAG_NONE, //���Ƀt���O�Ȃ�
		IID_PPV_ARGS(_fence.ReleaseAndGetAddressOf()));//_fence�����
	assert(SUCCEEDED(result));


	if (!CreateTransformConstantBuffer()) {
		return false;
	}

	if (!CreateWhiteTexture()) {
		return false;
	}
	if (!CreateBlackTexture()) {
		return false;
	}
	if (!CreateGradationTexture()) {
		return false;
	}
	if (!CreateTransformBufferView()) {
		return false;
	}

	if (!CreatePrimitives()) {
		return false;
	}
	if (!CreatePrimitiveRootSignature()) {
		return false;
	}
	if (!CreatePrimitivePipeline()) {
		return false;
	}

	if (!CreateDistortion()) {
		return false;
	}

	//�y���Q���ڗp
	if (!CreatePera2Resource()) {
		return false;
	}

	

	//�y���|���p
	if (!CreatePera1ResourceAndView()) {
		return false;
	}
	if (!CreateConstantBufferForPera()) {
		return false;
	}
	if (!CreatePeraVertex()) {
		return false;
	}
	if (!CreatePeraPipeline()) {
		return false;
	}
	if (!CreatePostSetting()) {
		return false;
	}

	//�k���o�b�t�@�p
	if (!CreateShrinkBufferAndView()) {
		return false;
	}

	//�A���r�G���g�I�N���[�W�����p
	if (!CreateAmbientOcclusion()) {
		return false;
	}



	return true;

}
DirectX::XMMATRIX
Dx12Wrapper::ViewMatrix()const {
	return _mappedScene->view;
}

DirectX::XMMATRIX
Dx12Wrapper::ProjMatrix()const {
	return _mappedScene->proj;
}

ComPtr<ID3D12DescriptorHeap>
Dx12Wrapper::CreateOneDescriptorHeapSRV()
{
	ComPtr<ID3D12DescriptorHeap> ret;
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	desc.NodeMask = 0;
	desc.NumDescriptors = 1;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	_dev->CreateDescriptorHeap(&desc, IID_PPV_ARGS(ret.ReleaseAndGetAddressOf()));
	return ret;
}

ComPtr<ID3D12DescriptorHeap>
Dx12Wrapper::CreateDescriptorHeapForImgUi() {
	return CreateOneDescriptorHeapSRV();
}


ComPtr<ID3D12DescriptorHeap>
Dx12Wrapper::CreateDescriptorHeapForSpriteFont() {
	return CreateOneDescriptorHeapSRV();
}


bool 
Dx12Wrapper::CreatePeraVertex() {
	struct PeraVertex {
		XMFLOAT3 pos;
		XMFLOAT2 uv;
	};
	PeraVertex pv[4] = { {{-1,-1,0.1},{0,1}},
						{{-1,1,0.1},{0,0}},
						{{1,-1,0.1},{1,1}},
						{{1,1,0.1},{1,0}} };

	auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(pv));
	auto result = _dev->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_peraVB.ReleaseAndGetAddressOf()));
	if (!CheckResult(result)) {
		assert(0);
		return false;
	}

	//�}�b�v�Y��Ă܂����B
	PeraVertex* mappedPera = nullptr;
	_peraVB->Map(0, nullptr, (void**)&mappedPera);
	copy(begin(pv), end(pv), mappedPera);
	_peraVB->Unmap(0, nullptr);

	_peraVBV.BufferLocation = _peraVB->GetGPUVirtualAddress();
	_peraVBV.SizeInBytes = sizeof(pv);
	_peraVBV.StrideInBytes = sizeof(PeraVertex);
	return true;
}

ComPtr<ID3D12Resource> 
Dx12Wrapper::WhiteTexture() {
	return _whiteTex;
}
ComPtr<ID3D12Resource> 
Dx12Wrapper::BlackTexture() {
	return _blackTex;
}
ComPtr<ID3D12Resource> 
Dx12Wrapper::GradTexture() {
	return _gradTex;
}

bool 
Dx12Wrapper::CreateRenderTargetView() {
	DXGI_SWAP_CHAIN_DESC1 swDesc = {};
	auto result = _swapchain->GetDesc1(&swDesc);

	//�����_�[�^�[�Q�b�g�r���[�p�̃q�[�v���쐬
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	desc.NodeMask = 0;
	desc.NumDescriptors = swDesc.BufferCount;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	result = _dev->CreateDescriptorHeap(&desc,
						IID_PPV_ARGS(&_rtvDescHeap));
	assert(SUCCEEDED(result));
	if (FAILED(result)) {
		return false;
	}
	_backBuffers.resize(swDesc.BufferCount);

	auto handle=_rtvDescHeap->GetCPUDescriptorHandleForHeapStart();
	auto incSize=_dev->GetDescriptorHandleIncrementSize(
							D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	//�X���b�v�`�F�C������A�t���b�v�Ώۂ̃��\�[�X���擾
	for (int i = 0; i < swDesc.BufferCount; ++i) {
		//GetBuffer�̓X���b�v�`�F�C���������Ă�i�Ԗڂ�
		//���\�[�X��������ɓ���Ă����
		result = _swapchain->GetBuffer(i, IID_PPV_ARGS(&_backBuffers[i]));
		assert(SUCCEEDED(result));
		if (FAILED(result)) {
			return false;
		}
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		_dev->CreateRenderTargetView(_backBuffers[i], &rtvDesc, handle);
		handle.ptr += incSize;
	}

	return true;

}

bool 
Dx12Wrapper::CreateCommandList() {
	auto result = _dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
										IID_PPV_ARGS(_cmdAlloc.ReleaseAndGetAddressOf()));
	assert(SUCCEEDED(result));
	if (FAILED(result)) {
		return false;
	}
	result = _dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
		_cmdAlloc.Get(), nullptr, IID_PPV_ARGS(_cmdList.ReleaseAndGetAddressOf()));
	assert(SUCCEEDED(result));
	if (FAILED(result)) {
		return false;
	}
	return true;
}

bool 
Dx12Wrapper::PreDrawShadow() {
	auto handle = _dsvHeap->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	
	_cmdList->OMSetRenderTargets(0, nullptr, false, &handle);
	
	_cmdList->ClearDepthStencilView(handle,
		D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	auto wsize = Application::Instance().GetWindowSize();

	ID3D12DescriptorHeap* heaps[] = { _sceneHeap.Get() };

	heaps[0] = _sceneHeap.Get();
	_cmdList->SetDescriptorHeaps(1, heaps);
	auto sceneHandle = _sceneHeap->GetGPUDescriptorHandleForHeapStart();
	_cmdList->SetGraphicsRootDescriptorTable(1, sceneHandle);

	D3D12_VIEWPORT vp = CD3DX12_VIEWPORT(0.0f, 0.0f, shadow_difinition, shadow_difinition);
	_cmdList->RSSetViewports(1, &vp);//�r���[�|�[�g

	CD3DX12_RECT rc(0, 0, shadow_difinition, shadow_difinition);
	_cmdList->RSSetScissorRects(1, &rc);//�V�U�[(�؂蔲��)��`

	return true;
}

bool 
Dx12Wrapper::CreateAmbientOcclusion() {
	auto& bbuff = _backBuffers[0];
	auto resDesc = bbuff->GetDesc();
	resDesc.Format = DXGI_FORMAT_R32_FLOAT;
	//�܂���ʃT�C�Y�Ɠ����o�b�t�@�����
	D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	
	D3D12_CLEAR_VALUE clr;
	clr.Color[0] = clr.Color[0] = clr.Color[2] = clr.Color[3] = 1.0f;
	clr.Format = DXGI_FORMAT_R32_FLOAT;
	auto result = _dev->CreateCommittedResource(&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			IID_PPV_ARGS(_ssaoBuffer.ReleaseAndGetAddressOf()));

	if (!CheckResult(result)) {
		return false;
	}

	//�����_�[�^�[�Q�b�g�p
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = 1;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	result=_dev->CreateDescriptorHeap(&desc, IID_PPV_ARGS(_ssaoRTVDH.GetAddressOf()));
	if (!CheckResult(result)) {
		return false;
	}
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	_dev->CreateRenderTargetView(_ssaoBuffer.Get(), &rtvDesc,_ssaoRTVDH->GetCPUDescriptorHandleForHeapStart());

	//�V�F�[�_���\�[�X�r���[�p
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	desc.NumDescriptors = 1;
	desc.NodeMask = 0;
	result = _dev->CreateDescriptorHeap(&desc, IID_PPV_ARGS(_ssaoSRVDH.ReleaseAndGetAddressOf()));
	if (!CheckResult) {
		return false;
	}
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	_dev->CreateShaderResourceView(_ssaoBuffer.Get(), &srvDesc, _ssaoSRVDH->GetCPUDescriptorHandleForHeapStart());


	return true;
}



bool 
Dx12Wrapper::CreatePostSetting() {
	auto bufferSize = AligmentedValue(sizeof(PostSetting), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	//�܂��̓o�b�t�@���
	auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
	auto result = _dev->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_postSetting.ReleaseAndGetAddressOf()));
	if (!CheckResult(result)) {
		return false;
	}
	//�f�X�N���v�^�q�[�v
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	desc.NodeMask = 0;
	desc.NumDescriptors = 1;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	result = _dev->CreateDescriptorHeap(&desc, IID_PPV_ARGS(_postSettingDH.ReleaseAndGetAddressOf()));
	if (!CheckResult(result)) {
		return false;
	}

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = _postSetting->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = bufferSize;

	_dev->CreateConstantBufferView(&cbvDesc, _postSettingDH->GetCPUDescriptorHandleForHeapStart());

	result = _postSetting->Map(0, nullptr, (void**)&_mappedPostSetting);
	if (!CheckResult(result)) {
		return false;
	}
	return true;

}

void
Dx12Wrapper::SetFocusPos(float x , float y) {
	auto wsize=Application::Instance().GetWindowSize();
	_mappedPostSetting->focusPos.x = (float)x/(float)wsize.width;
	_mappedPostSetting->focusPos.y = (float)y/(float)wsize.height;
}

void 
Dx12Wrapper::SetOutline(bool flgOnOff) {
	_mappedPostSetting->outlineFlg = flgOnOff;
}
void Dx12Wrapper::SetNormalOutline(bool flgOnOff) {
	_mappedPostSetting->normalOutlineFlg = flgOnOff;
}
bool 
Dx12Wrapper::PreDrawToPera1(float clsClr[4]) {
	for (auto& peraRes : _peraResources) {
		Barrier(peraRes.Get(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET);
	}

	CD3DX12_CPU_DESCRIPTOR_HANDLE handles[3] = {};
	handles[0]= _peraRTVHeap->GetCPUDescriptorHandleForHeapStart();
	handles[1].InitOffsetted(handles[0], _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
	handles[2].InitOffsetted(handles[0], _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV)*2);
	auto handle = _dsvHeap->GetCPUDescriptorHandleForHeapStart();
	_cmdList->OMSetRenderTargets(3, handles, false, &handle);
	//�N���A�J���[		 R   G   B   A
	clsClr[0] = clsClr[1] = clsClr[2] = 0.5f;
	_cmdList->ClearRenderTargetView(handles[0], clsClr, 0, nullptr);
	float nmcol[4] = {};
	_cmdList->ClearRenderTargetView(handles[1], nmcol, 0, nullptr);
	_cmdList->ClearRenderTargetView(handles[2], nmcol, 0, nullptr);
	
	_cmdList->ClearDepthStencilView(_dsvHeap->GetCPUDescriptorHandleForHeapStart(),
		D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	_cmdList->SetDescriptorHeaps(1, _sceneHeap.GetAddressOf());
	auto sceneHandle = _sceneHeap->GetGPUDescriptorHandleForHeapStart();
	_cmdList->SetGraphicsRootDescriptorTable(1, sceneHandle);

	_cmdList->SetDescriptorHeaps(1, _postSettingDH.GetAddressOf());
	_cmdList->SetGraphicsRootDescriptorTable(4, _postSettingDH->GetGPUDescriptorHandleForHeapStart());

	return true;
}

void 
Dx12Wrapper::SetRimLight(bool flgOnOff, float strength) {
	_mappedPostSetting->rimFlg = flgOnOff;
	_mappedPostSetting->rimStrength = strength;
}

void 
Dx12Wrapper::SetDOF(bool dofFlg) {
	_mappedPostSetting->dofFlg = dofFlg;
}

void 
Dx12Wrapper::SetBloomColor(float col[4]) {
	_mappedPostSetting->bloomColor = XMFLOAT4(col[0], col[1], col[2], col[3]);
}

void 
Dx12Wrapper::SetBloom(bool flg) {
	_mappedPostSetting->bloomFlg= flg;
}

void 
Dx12Wrapper::SetAA(bool flg) {
	_mappedPostSetting->antiAlias = flg;
}
void Dx12Wrapper::SetDirectionalLight(bool flg) {
	_mappedPostSetting->directionalLight = flg;
}
void
Dx12Wrapper::SetDebugDisplay(bool flg) {
	_mappedPostSetting->debugDispFlg = flg;
}

bool
Dx12Wrapper::Clear() {
	for (auto& peraRes : _peraResources) {
		Barrier(peraRes.Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}


	//�o�b�N�o�b�t�@�̃C���f�b�N�X���擾����
	auto bbIdx=_swapchain->GetCurrentBackBufferIndex();

	Barrier(_backBuffers[bbIdx],
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET);

	auto rtvHeapPointer=_rtvDescHeap->GetCPUDescriptorHandleForHeapStart();
	rtvHeapPointer.ptr+=bbIdx*_dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	
	_cmdList->OMSetRenderTargets(1, &rtvHeapPointer, false, nullptr);
	
	//�N���A�J���[		 R   G   B   A
	float clsClr[4] = { 0.2,0.5,0.5,1.0 };
	_cmdList->ClearRenderTargetView(rtvHeapPointer, clsClr, 0, nullptr);
	//_cmdList->ClearDepthStencilView(_dsvHeap->GetCPUDescriptorHandleForHeapStart(),
	//	D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	return true;
}

void Dx12Wrapper::Barrier(ID3D12Resource* p,
	D3D12_RESOURCE_STATES before,
	D3D12_RESOURCE_STATES after){
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(p, before, after, 0);
	_cmdList->ResourceBarrier(1, &barrier);
}

void 
Dx12Wrapper::Flip() {
	auto bbIdx = _swapchain->GetCurrentBackBufferIndex();

	Barrier(_backBuffers[bbIdx],
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PRESENT);

	_cmdList->Close();
	ID3D12CommandList* cmds[] = { _cmdList.Get() };
	_cmdQue->ExecuteCommandLists(1, cmds);
	
	WaitForCommandQueue();

	_cmdAlloc->Reset();
	_cmdList->Reset(_cmdAlloc.Get(), nullptr);

	//Present�֐����ADxLib�ɂ�����ScreenFlip�݂����Ȃ���ł��B
	//Present�̑��������u���񐂒�������҂��v�ł��B
	//���������M�����Ă̂̓u���E���ǂ̎���ɑ��������������
	//����1/60�������񂾂��ǁA���͂���Ȃ̂Ȃ��̂ő҂��Ȃ�
	auto result = _swapchain->Present(0, 0);
	assert(SUCCEEDED(result));
}

void Dx12Wrapper::WaitForCommandQueue()
{
	//���ɏ���Ƃ���_fenceValue��0�������Ƃ��܂�
	_cmdQue->Signal(_fence.Get(), ++_fenceValue);
	//���̖��ߒ���ł�_fenceValue��1�ŁA
	//GetCompletedValue�͂܂�0�ł��B
	if (_fence->GetCompletedValue() < _fenceValue) {
		//�����܂��I����ĂȂ��Ȃ�A�C�x���g�҂����s��
		//�����̂��߂̃C�x���g�H���Ƃ��̂��߂�_fenceValue
		auto event = CreateEvent(nullptr, false, false, nullptr);
		//�t�F���X�ɑ΂��āACompletedValue��_fenceValue��
		//�Ȃ�����w��̃C�x���g�𔭐�������Ƃ������߁�
		_fence->SetEventOnCompletion(_fenceValue, event);
		//���܂��C�x���g�������Ȃ�
		//���C�x���g����������܂ő҂�
		WaitForSingleObject(event, INFINITE);
		CloseHandle(event);
	}
}

void 
Dx12Wrapper::SetFov(float angle) {
	_fov = angle;
}
void 
Dx12Wrapper::SetEyePosition(float x, float y, float z) {
	_eye.x = x;
	_eye.y = y;
	_eye.z = z;
}
void 
Dx12Wrapper::MoveEyePosition(float x, float y, float z) {
	_eye.x += x;
	_eye.y += y;
	_eye.z += z;
	_target.x += x;
	_target.y += y;
	_target.z += z;

}

void 
Dx12Wrapper::SetAOTryCount(uint32_t trycount) {
	_mappedPostSetting->tryCount = trycount;
}

void
Dx12Wrapper::SetAORadius(float radius) {
	_mappedPostSetting->aoRadius = radius;
}

bool 
Dx12Wrapper::CreateShrinkBufferAndView() {
	float col[4] = { 0.0f,0.0f,0.0f,0.0f };
	auto resDesc = _peraResources[0]->GetDesc();
	resDesc.Width /= 2;
	auto result = S_OK;
	auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	auto optimizedClearValue = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_R8G8B8A8_UNORM, col);
	for (auto& buf : _shrinkBuffers) {
		auto result = _dev->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			&optimizedClearValue,
			IID_PPV_ARGS(buf.ReleaseAndGetAddressOf()));
		if (!CheckResult(result)) {
			return false;
		}
	}
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = 2;//bloom�p,��ʊE�[�x�p
	desc.NodeMask = 0;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	result = _dev->CreateDescriptorHeap(&desc, IID_PPV_ARGS(_shrinkRTVDH.ReleaseAndGetAddressOf()));
	if (!CheckResult(result)) {
		return false;
	}
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	auto handle=_shrinkRTVDH->GetCPUDescriptorHandleForHeapStart();
	for (auto& buf : _shrinkBuffers) {
		_dev->CreateRenderTargetView(buf.Get(),
			&rtvDesc,
			handle);
		handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	//SRV�p
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	result = _dev->CreateDescriptorHeap(&desc,
		IID_PPV_ARGS(_shrinkSRVDH.ReleaseAndGetAddressOf()));
	if (!CheckResult(result)) {
		return false;
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	handle=_shrinkSRVDH->GetCPUDescriptorHandleForHeapStart();
	for (auto& buf : _shrinkBuffers) {
		_dev->CreateShaderResourceView(buf.Get(),
			&srvDesc,
			handle);
		handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	return true;
}
void 
Dx12Wrapper::DrawToShrinkBuffer() {
	CD3DX12_CPU_DESCRIPTOR_HANDLE handles[2] = {};
	handles[0]=_shrinkRTVDH->GetCPUDescriptorHandleForHeapStart();
	handles[1].InitOffsetted(handles[0], _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
	
	_cmdList->OMSetRenderTargets(2, handles, false, nullptr);
	float clsclr[4] = { 0.0,0.0,0.0,0.0 };
	for (auto& handle : handles) {
		_cmdList->ClearRenderTargetView(handle, clsclr, 0, nullptr);
	}
	_cmdList->SetGraphicsRootSignature(_peraRS.Get());
	_cmdList->SetPipelineState(_shrinkPipeline.Get());
	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	_cmdList->IASetVertexBuffers(0, 1, &_peraVBV);
	_cmdList->SetDescriptorHeaps(1,_peraSRVHeap.GetAddressOf());
	_cmdList->SetGraphicsRootDescriptorTable(1, _peraSRVHeap->GetGPUDescriptorHandleForHeapStart());
	auto desc=_shrinkBuffers[(int)ShrinkType::bloom]->GetDesc();
	int width=desc.Width;
	int height=desc.Height/2;
	D3D12_VIEWPORT vp;
	vp.TopLeftX = vp.TopLeftY = 0;
	vp.Width = width;
	vp.Height = height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	D3D12_RECT rc;
	rc.left = rc.top = 0;
	rc.right = vp.Width;
	rc.bottom = vp.Height;
	for (int i = 0; i < 8; ++i) {
		_cmdList->RSSetViewports(1, &vp);
		_cmdList->RSSetScissorRects(1, &rc);
		_cmdList->DrawInstanced(4, 1, 0, 0);

		vp.TopLeftY += vp.Height;
		rc.top = vp.TopLeftY;
		vp.Height /= 2;
		vp.Width /= 2;
		rc.bottom = rc.top + vp.Height;
		
	}
}

bool 
Dx12Wrapper::CreateDepthBuffer() {
	D3D12_HEAP_PROPERTIES heapprop = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	auto wsize = Application::Instance().GetWindowSize();
	D3D12_RESOURCE_DESC resdesc = 
		CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_TYPELESS, wsize.width, wsize.height);
	resdesc.Flags= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE cv = {};
	cv.DepthStencil.Depth = 1.0f;
	cv.Format = DXGI_FORMAT_D32_FLOAT;

	auto result = _dev->CreateCommittedResource(&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&cv,
		IID_PPV_ARGS(_depthBuffer.ReleaseAndGetAddressOf()));

	assert(SUCCEEDED(result));
	if (FAILED(result)) {
		return false;
	}

	resdesc.Width = shadow_difinition;
	resdesc.Height = shadow_difinition;
	result = _dev->CreateCommittedResource(&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&cv,
		IID_PPV_ARGS(_lightDepthBuffer.ReleaseAndGetAddressOf()));

	if (FAILED(result)) {
		return false;
	}
	return true;

}
bool 
Dx12Wrapper::CreateDSV() {
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NodeMask = 0;
	desc.NumDescriptors = 2;//0�Ԃ͒ʏ�[�x�A1�Ԃ̓��C�g�[�x(�e�p)
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	auto result=_dev->CreateDescriptorHeap(&desc, IID_PPV_ARGS(_dsvHeap.ReleaseAndGetAddressOf()));
	assert(SUCCEEDED(result));
	if (FAILED(result)) {
		return false;
	}
	D3D12_DEPTH_STENCIL_VIEW_DESC viewDesc = {};
	viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	viewDesc.Flags = D3D12_DSV_FLAG_NONE;
	viewDesc.Format = DXGI_FORMAT_D32_FLOAT;

	auto handle=_dsvHeap->GetCPUDescriptorHandleForHeapStart();

	_dev->CreateDepthStencilView(_depthBuffer.Get(),
		&viewDesc,
		handle);

	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	
	_dev->CreateDepthStencilView(_lightDepthBuffer.Get(),
		&viewDesc,
		handle);

	return true;
}

void Dx12Wrapper::DrawPrimitiveShapes() {
	auto wsize = Application::Instance().GetWindowSize();

	D3D12_VIEWPORT vp = CD3DX12_VIEWPORT(0.0f, 0.0f, wsize.width, wsize.height);
	_cmdList->RSSetViewports(1, &vp);//�r���[�|�[�g

	CD3DX12_RECT rc(0, 0, wsize.width, wsize.height);
	_cmdList->RSSetScissorRects(1, &rc);//�V�U�[(�؂蔲��)��`

	_cmdList->SetGraphicsRootSignature(_primitveRS.Get());
	_cmdList->SetPipelineState(_primitivePipeline.Get());
	_cmdList->SetDescriptorHeaps(1, _depthSRVHeap.GetAddressOf());
	auto handle = _depthSRVHeap->GetGPUDescriptorHandleForHeapStart();
	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	_cmdList->SetGraphicsRootDescriptorTable(3, handle);




	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	for (int i = 0; i < _primitivesIBV.size(); ++i) {
		_cmdList->IASetIndexBuffer(&_primitivesIBV[i]);
		_cmdList->IASetVertexBuffers(0, 1, &_primitivesVBV[i]);
		_cmdList->DrawIndexedInstanced(_primitivesIBV[i].SizeInBytes/2, 1, 0, 0, 0);
	}

}

void
Dx12Wrapper::DrawAmbientOcclusion() {
	auto renderTargetDescriptors = _ssaoRTVDH->GetCPUDescriptorHandleForHeapStart();
	_cmdList->OMSetRenderTargets(1, &renderTargetDescriptors, false, nullptr);
	auto wsize = Application::Instance().GetWindowSize();

	D3D12_VIEWPORT vp = CD3DX12_VIEWPORT(0.0f, 0.0f, wsize.width, wsize.height);
	_cmdList->RSSetViewports(1, &vp);//�r���[�|�[�g

	CD3DX12_RECT rc(0, 0, wsize.width, wsize.height);
	_cmdList->RSSetScissorRects(1, &rc);//�V�U�[(�؂蔲��)��`

	_cmdList->SetGraphicsRootSignature(_peraRS.Get());

	_cmdList->SetDescriptorHeaps(1, _sceneHeap.GetAddressOf());
	auto handle = _sceneHeap->GetGPUDescriptorHandleForHeapStart();
	_cmdList->SetGraphicsRootDescriptorTable(6, _sceneHeap->GetGPUDescriptorHandleForHeapStart());

	_cmdList->SetDescriptorHeaps(1, _depthSRVHeap.GetAddressOf());
	_cmdList->SetGraphicsRootDescriptorTable(3, _depthSRVHeap->GetGPUDescriptorHandleForHeapStart());


	_cmdList->SetDescriptorHeaps(1, _peraSRVHeap.GetAddressOf());
	_cmdList->SetGraphicsRootDescriptorTable(1, _peraSRVHeap->GetGPUDescriptorHandleForHeapStart());

	_cmdList->SetDescriptorHeaps(1, _postSettingDH.GetAddressOf());
	_cmdList->SetGraphicsRootDescriptorTable(4, _postSettingDH->GetGPUDescriptorHandleForHeapStart());

	_cmdList->SetPipelineState(_ssaoPipeline.Get());
	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	_cmdList->IASetVertexBuffers(0, 1, &_peraVBV);
	_cmdList->DrawInstanced(4, 1, 0, 0);

}

//�`��
void
Dx12Wrapper::DrawToPera1(shared_ptr<PMDRenderer> renderer) {
	auto wsize = Application::Instance().GetWindowSize();

	D3D12_VIEWPORT vp = CD3DX12_VIEWPORT(0.0f, 0.0f, wsize.width, wsize.height);
	_cmdList->RSSetViewports(1, &vp);//�r���[�|�[�g

	CD3DX12_RECT rc(0, 0, wsize.width, wsize.height);
	_cmdList->RSSetScissorRects(1, &rc);//�V�U�[(�؂蔲��)��`

	_cmdList->SetDescriptorHeaps(1, _depthSRVHeap.GetAddressOf());
	auto handle = _depthSRVHeap->GetGPUDescriptorHandleForHeapStart();
	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	_cmdList->SetGraphicsRootDescriptorTable(3, handle);

	//_cmdList->SetGraphicsRootSignature(_PRIVILEGE_SET)
	//_cmdList->IASetVertexBuffers(0,1,&_primitivesVBV[0]);
	//_cmdList->IASetIndexBuffer(&_primitivesIBV[0]);
	//_cmdList->DrawIndexedInstanced(4, 1, 0, 0, 0);

}

bool 
Dx12Wrapper::CreatePeraPipeline() {
	D3D12_DESCRIPTOR_RANGE range[8] = {};
	range[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;//b
	range[0].BaseShaderRegister = 0;//0
	range[0].NumDescriptors = 1;
	range[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	range[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;//t
	range[1].BaseShaderRegister = 0;//0
	range[1].NumDescriptors = 3;//t0,t1,t2//�W���A�@���A���P�x
	range[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	range[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;//t
	range[2].BaseShaderRegister = 3;//
	range[2].NumDescriptors = 1;//t3
	range[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	range[3].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;//t
	range[3].BaseShaderRegister = 4;//
	range[3].NumDescriptors = 2;//2�g����(t4,t5)�[�x�A���C�g�[�x
	range[3].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	range[4].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;//b
	range[4].BaseShaderRegister = 1;//1
	range[4].NumDescriptors = 1;//b1
	range[4].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	//�k���o�b�t�@�p�����W�𑝂₷
	range[5].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;//t
	range[5].BaseShaderRegister = 6;//1
	range[5].NumDescriptors = 2;//t6(���P�x),t7(DOF�p�k��)
	range[5].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	range[6].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;//b
	range[6].BaseShaderRegister = 2;//1
	range[6].NumDescriptors = 1;//b2
	range[6].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	//SSAO
	range[7].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;//t
	range[7].BaseShaderRegister = 8;//1
	range[7].NumDescriptors = 1;//t8(SSAO)
	range[7].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rp[8] = {};
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

	rp[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rp[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rp[6].DescriptorTable.pDescriptorRanges = &range[6];
	rp[6].DescriptorTable.NumDescriptorRanges = 1;

	rp[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rp[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rp[7].DescriptorTable.pDescriptorRanges = &range[7];
	rp[7].DescriptorTable.NumDescriptorRanges = 1;


	D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
	rsDesc.NumParameters = 8;
	rsDesc.pParameters = rp;
	D3D12_STATIC_SAMPLER_DESC sampler = CD3DX12_STATIC_SAMPLER_DESC(0);
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	rsDesc.pStaticSamplers = &sampler;
	rsDesc.NumStaticSamplers = 1;
	rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	

	ComPtr<ID3DBlob> rsBlob;
	ComPtr<ID3DBlob> errBlob;
	auto result=D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, rsBlob.ReleaseAndGetAddressOf(), errBlob.ReleaseAndGetAddressOf());
	if (!CheckResult(result, errBlob.Get())) {
		assert(0);
		return false;
	}
	result = _dev->CreateRootSignature(0, rsBlob->GetBufferPointer(), rsBlob->GetBufferSize(), IID_PPV_ARGS(_peraRS.ReleaseAndGetAddressOf()));
	if (!CheckResult(result)) {
		assert(0);
		return false;
	}

	ComPtr<ID3DBlob> vs;
	ComPtr<ID3DBlob> ps;
	result = D3DCompileFromFile(L"PeraVertexShader.hlsl", 
		nullptr,//�}�N����`(D3D_SHADER_MACRO�̃|�C���^)
		D3D_COMPILE_STANDARD_FILE_INCLUDE,//�C���N���[�h��`(ID3DInclude�̃|�C���^)
		"PeraVS", "vs_5_0", 0, 0, vs.ReleaseAndGetAddressOf(), errBlob.ReleaseAndGetAddressOf());
	if (!CheckResult(result,errBlob.Get())) {
		assert(0);
		return false;
	}
	result = D3DCompileFromFile(L"PeraPixelShader.hlsl", 
		nullptr,//�}�N����`(D3D_SHADER_MACRO�̃|�C���^)
		D3D_COMPILE_STANDARD_FILE_INCLUDE,//�C���N���[�h��`(ID3DInclude�̃|�C���^)
		"PeraPS", "ps_5_0", 0, 0, ps.ReleaseAndGetAddressOf(), errBlob.ReleaseAndGetAddressOf());
	if (!CheckResult(result, errBlob.Get())) {
		assert(0);
		return false;
	}
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpsDesc = {};
	gpsDesc.pRootSignature = _peraRS.Get();
	gpsDesc.VS = CD3DX12_SHADER_BYTECODE(vs.Get());
	gpsDesc.PS = CD3DX12_SHADER_BYTECODE(ps.Get());
	gpsDesc.DepthStencilState.DepthEnable = false;
	gpsDesc.DepthStencilState.StencilEnable = false;
	D3D12_INPUT_ELEMENT_DESC layout[2] = {
		{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
		{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
	};
	gpsDesc.InputLayout.NumElements = _countof(layout);
	gpsDesc.InputLayout.pInputElementDescs = layout;
	gpsDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	gpsDesc.BlendState.RenderTarget[0].BlendEnable = true;
	gpsDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	gpsDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	gpsDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	gpsDesc.NumRenderTargets = 1;
	gpsDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	gpsDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	gpsDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	gpsDesc.SampleDesc.Count = 1;
	gpsDesc.SampleDesc.Quality = 0;
	gpsDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	result = _dev->CreateGraphicsPipelineState(&gpsDesc, IID_PPV_ARGS(_peraPipeline.ReleaseAndGetAddressOf()));
	if (!CheckResult(result)) {
		assert(0);
		return false;
	}

	result = D3DCompileFromFile(L"ShrinkPixelShader.hlsl", 
		nullptr,//�}�N����`(D3D_SHADER_MACRO�̃|�C���^)
		D3D_COMPILE_STANDARD_FILE_INCLUDE,//�C���N���[�h��`(ID3DInclude�̃|�C���^)
		"ShrinkPS", "ps_5_0", 0, 0, ps.ReleaseAndGetAddressOf(), errBlob.ReleaseAndGetAddressOf());
	if (!CheckResult(result, errBlob.Get())) {
		assert(0);
		return false;
	}
	gpsDesc.PS = CD3DX12_SHADER_BYTECODE(ps.Get());
	gpsDesc.NumRenderTargets = 2;
	gpsDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	gpsDesc.RTVFormats[1] = DXGI_FORMAT_R8G8B8A8_UNORM;
	result = _dev->CreateGraphicsPipelineState(&gpsDesc, IID_PPV_ARGS(_shrinkPipeline.ReleaseAndGetAddressOf()));
	if (!CheckResult(result)) {
		assert(0);
		return false;
	}

	

	result = D3DCompileFromFile(L"PeraPixelShader.hlsl", 
		nullptr,//�}�N����`(D3D_SHADER_MACRO�̃|�C���^)
		D3D_COMPILE_STANDARD_FILE_INCLUDE,//�C���N���[�h��`(ID3DInclude�̃|�C���^)
		"VerticalBlurPS", "ps_5_0", 0, 0, ps.ReleaseAndGetAddressOf(), errBlob.ReleaseAndGetAddressOf());
	if (!CheckResult(result, errBlob.Get())) {
		assert(0);
		return false;
	}
	gpsDesc.PS = CD3DX12_SHADER_BYTECODE(ps.Get());
	result = _dev->CreateGraphicsPipelineState(&gpsDesc, IID_PPV_ARGS(_peraPipeline2.ReleaseAndGetAddressOf()));
	if (!CheckResult(result)) {
		assert(0);
		return false;
	}

	result = D3DCompileFromFile(L"SSAOPixelShader.hlsl", 
		nullptr,//�}�N����`(D3D_SHADER_MACRO�̃|�C���^)
		D3D_COMPILE_STANDARD_FILE_INCLUDE,//�C���N���[�h��`(ID3DInclude�̃|�C���^)
		"SsaoPS", "ps_5_0", 0, 0, ps.ReleaseAndGetAddressOf(), errBlob.ReleaseAndGetAddressOf());
	if (!CheckResult(result, errBlob.Get())) {
		assert(0);
		return false;
	}
	gpsDesc.PS = CD3DX12_SHADER_BYTECODE(ps.Get());
	gpsDesc.NumRenderTargets = 1;
	gpsDesc.RTVFormats[0] = DXGI_FORMAT_R32_FLOAT;
	gpsDesc.RTVFormats[1] = DXGI_FORMAT_UNKNOWN;
	gpsDesc.BlendState.RenderTarget[0].BlendEnable = false;
	result = _dev->CreateGraphicsPipelineState(&gpsDesc, IID_PPV_ARGS(_ssaoPipeline.ReleaseAndGetAddressOf()));
	if (!CheckResult(result)) {
		assert(0);
		return false;
	}


	return true;
}

//�`��
void 
Dx12Wrapper::Draw(shared_ptr<PMDRenderer> renderer) {
	auto wsize = Application::Instance().GetWindowSize();
	
	D3D12_VIEWPORT vp=CD3DX12_VIEWPORT(0.0f,0.0f,wsize.width,wsize.height);
	_cmdList->RSSetViewports(1, &vp);//�r���[�|�[�g

	CD3DX12_RECT rc(0, 0, wsize.width, wsize.height);
	_cmdList->RSSetScissorRects(1,&rc);//�V�U�[(�؂蔲��)��`

	_cmdList->SetGraphicsRootSignature(_peraRS.Get());
	_cmdList->SetDescriptorHeaps(1, _peraSRVHeap.GetAddressOf());
	auto handle=_peraSRVHeap->GetGPUDescriptorHandleForHeapStart();
	
	//�ŏI�\�����̓y���Q���ڂȂ̂ŁA�C���N�������g����
	//handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	_cmdList->SetGraphicsRootDescriptorTable(1, handle);
	
	_cmdList->SetDescriptorHeaps(1, _peraCBVHeap.GetAddressOf());
	_cmdList->SetGraphicsRootDescriptorTable(0, _peraCBVHeap->GetGPUDescriptorHandleForHeapStart());

	_cmdList->SetDescriptorHeaps(1, _distSRVHeap.GetAddressOf());
	_cmdList->SetGraphicsRootDescriptorTable(2, _distSRVHeap->GetGPUDescriptorHandleForHeapStart());

	//�[�x�o�b�t�@�e�N�X�`��
	_cmdList->SetDescriptorHeaps(1, _depthSRVHeap.GetAddressOf());
	_cmdList->SetGraphicsRootDescriptorTable(3, _depthSRVHeap->GetGPUDescriptorHandleForHeapStart());


	_cmdList->SetPipelineState(_peraPipeline.Get());
	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	_cmdList->IASetVertexBuffers(0, 1, &_peraVBV);

	_cmdList->SetDescriptorHeaps(1, _postSettingDH.GetAddressOf());
	_cmdList->SetGraphicsRootDescriptorTable(4, _postSettingDH->GetGPUDescriptorHandleForHeapStart());

	_cmdList->SetDescriptorHeaps(1, _shrinkSRVDH.GetAddressOf());
	_cmdList->SetGraphicsRootDescriptorTable(5, _shrinkSRVDH->GetGPUDescriptorHandleForHeapStart());

	_cmdList->SetDescriptorHeaps(1, _ssaoSRVDH.GetAddressOf());
	_cmdList->SetGraphicsRootDescriptorTable(7, _ssaoSRVDH->GetGPUDescriptorHandleForHeapStart());

	_cmdList->DrawInstanced(4, 1, 0, 0);
}

void Dx12Wrapper::SetCameraSetting()
{
	auto wsize = Application::Instance().GetWindowSize();

	auto eyePos = XMLoadFloat3(&_eye);
	auto targetPos = XMLoadFloat3(&_target);
	auto upVec = XMLoadFloat3(&_up);

	_mappedScene->eye = _eye;
	_mappedScene->view = XMMatrixLookAtLH(eyePos,targetPos,upVec);
	_mappedScene->proj = XMMatrixPerspectiveFovLH(
		_fov,
		static_cast<float>(wsize.width) / static_cast<float>(wsize.height),
		1.0f,
		50.0f);

	_mappedScene->invProj = XMMatrixInverse(nullptr, _mappedScene->proj);

	auto plane = XMFLOAT4(0, 1, 0, 0);//����
	XMVECTOR planeVec = XMLoadFloat4(&plane);
	auto light = XMFLOAT4(-1, 1, -1, 0);
	XMVECTOR lightVec = XMLoadFloat4(&light);
	_mappedScene->shadow = XMMatrixShadow(planeVec, lightVec);

	float armLength;//�J�����A�[����(���_���璍���_�̒���)
	armLength=XMVector3Length(XMVectorSubtract( targetPos, eyePos)).m128_f32[0];
	auto lightPos= targetPos+XMVector3Normalize(lightVec)*armLength;
	_mappedScene->lightCamera = XMMatrixLookAtLH(lightPos, targetPos, upVec)*
		//XMMatrixPerspectiveFovLH(XM_PIDIV4, 1280.0f/720.0f, 1.0f, 100.0f);
		XMMatrixOrthographicLH(40,40,1.0f,100.0f);
}

bool 
Dx12Wrapper::CreatePrimitives() {


	constexpr XMFLOAT3 center(0, 0, 0);
	constexpr float width=40.0f;
	constexpr float height=40.0f;

	PrimitiveVertex plane[4] = {
		{{center.x - width / 2.0f, center.y,center.z - height / 2.0f},{0,1,0}},
		{{center.x - width / 2.0f, center.y,center.z + height / 2.0f},{0,1,0}},
		{{center.x + width / 2.0f, center.y,center.z - height / 2.0f},{0,1,0}},
		{{center.x + width / 2.0f, center.y,center.z + height / 2.0f},{0,1,0}},
	};
	uint16_t indexes[]={ 0,1,2,1,3,2 };

	auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(plane));
	//���_�o�b�t�@�쐬
	ID3D12Resource* vbuff=nullptr;
	auto result = _dev->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&vbuff));
	_primitivesVB.push_back(vbuff);
	if (!CheckResult(result)) {
		return false;
	}
	D3D12_VERTEX_BUFFER_VIEW vbv = {};
	vbv.BufferLocation = vbuff->GetGPUVirtualAddress();
	vbv.SizeInBytes = sizeof(plane);
	vbv.StrideInBytes = sizeof(PrimitiveVertex);
	_primitivesVBV.push_back(vbv);


	PrimitiveVertex* mappedVertex=nullptr;
	//�}�b�v���ăR�s�[
	vbuff->Map(0, nullptr, (void**)&mappedVertex);
	copy(begin(plane), end(plane), mappedVertex);
	vbuff->Unmap(0, nullptr);

	heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	resDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(indexes));
	//�C���f�b�N�X�o�b�t�@�쐬
	ID3D12Resource* ibuff = nullptr;
	result = _dev->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&ibuff));
	_primitivesIB.push_back(ibuff);
	if (!CheckResult(result)) {
		return false;
	}
	D3D12_INDEX_BUFFER_VIEW ibv = {};
	ibv.BufferLocation = ibuff->GetGPUVirtualAddress();
	ibv.Format = DXGI_FORMAT_R16_UINT;
	ibv.SizeInBytes = sizeof(indexes);
	_primitivesIBV.push_back(ibv);

	uint16_t* mappedIndex;

	ibuff->Map(0, nullptr, (void**)&mappedIndex);
	copy(begin(indexes), end(indexes), mappedIndex);
	ibuff->Unmap(0, nullptr);

	constexpr uint32_t div = 16;

	PrimitiveVertex cone[div*3+1];//�Ō�̂̓t�^�Ԃ�
	float radius = 5.0f;
	auto angle = 0.0f;
	constexpr float cylinder_height = 8.0f;
	//��i
	for (int i = 0; i < div; ++i) {
		
		cone[i].pos.x = cos(angle)*radius;
		cone[i].pos.y = cylinder_height;
		cone[i].pos.z = sin(angle)*radius;

		cone[i].normal.x = cos(angle);
		cone[i].normal.y = 0;
		cone[i].normal.z = sin(angle);
		angle += XM_2PI / static_cast<float>(div);
	}
	//���i
	for (int i = 0; i < div; ++i) {

		cone[i + div].pos.x = cos(angle)*radius;
		cone[i + div].pos.y = 0;
		cone[i + div].pos.z = sin(angle)*radius;

		cone[i + div].normal.x = cos(angle);
		cone[i + div].normal.y = 0;
		cone[i + div].normal.z = sin(angle);
		angle += XM_2PI / static_cast<float>(div);
	}
	//�t�^
	for (int i = 0; i < div; ++i) {

		cone[div*2+i].pos.x = cos(angle)*radius;
		cone[div * 2 + i].pos.y = cylinder_height;
		cone[div * 2 + i].pos.z = sin(angle)*radius;

		cone[div * 2 + i].normal.x = 0;
		cone[div * 2 + i].normal.y = 1;
		cone[div * 2 + i].normal.z = 0;
		angle += XM_2PI / static_cast<float>(div);
	}
	cone[div * 3].pos = XMFLOAT3(0, cylinder_height, 0);
	cone[div * 3].normal = XMFLOAT3(0, 1, 0);

	uint16_t coneIdxes[div * (3*(2+1))];
	uint16_t idx = 0;
	for (int i = 0; i < div; ++i) {
		//��i
		coneIdxes[idx] = i;
		++idx;
		coneIdxes[idx] = (i+1)%div;
		++idx;
		coneIdxes[idx] = (i+1)%div+div;
		++idx;
		//���i
		coneIdxes[idx] = i+div;
		++idx;
		coneIdxes[idx] = i;
		++idx;
		coneIdxes[idx] = (i + 1) % div + div;
		++idx;
	}
	//�t�^
	for (int i = 0; i < div; ++i) {
		coneIdxes[idx] = div * 3;
		++idx;
		coneIdxes[idx] = i + div * 2;
		++idx;
		coneIdxes[idx] = (i+1)%div+div*2;
		++idx;
	}

	heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	resDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(cone));
	//���_�o�b�t�@�쐬
	ID3D12Resource* coneVBuff = nullptr;
	result = _dev->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&coneVBuff));
	_primitivesVB.push_back(coneVBuff);
	if (!CheckResult(result)) {
		return false;
	}
	vbv = {};
	vbv.BufferLocation = coneVBuff->GetGPUVirtualAddress();
	vbv.SizeInBytes = sizeof(cone);
	vbv.StrideInBytes = sizeof(PrimitiveVertex);
	_primitivesVBV.push_back(vbv);

	
	heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	resDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(coneIdxes));
	//�C���f�b�N�X�o�b�t�@�쐬
	ID3D12Resource* coneIB = nullptr;
	result = _dev->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&coneIB));
	_primitivesIB.push_back(ibuff);
	if (!CheckResult(result)) {
		return false;
	}
	ibv = {};
	ibv.BufferLocation = coneIB->GetGPUVirtualAddress();
	ibv.Format = DXGI_FORMAT_R16_UINT;
	ibv.SizeInBytes = sizeof(coneIdxes);
	_primitivesIBV.push_back(ibv);

	mappedVertex = nullptr;
	//�}�b�v���ăR�s�[
	coneVBuff->Map(0, nullptr, (void**)&mappedVertex);
	copy(begin(cone), end(cone), mappedVertex);
	coneVBuff->Unmap(0, nullptr);

	mappedIndex=nullptr;

	coneIB->Map(0, nullptr, (void**)&mappedIndex);
	copy(begin(coneIdxes), end(coneIdxes), mappedIndex);
	coneIB->Unmap(0, nullptr);

	return true;
}

bool
Dx12Wrapper::CreatePrimitiveRootSignature() {
	D3D12_DESCRIPTOR_RANGE range[5] = {};
	//�����W0�͍��W�ϊ�(�J�����ƃ��[���h�̂ӂ���)b1
	range[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;//b
	range[0].BaseShaderRegister = 1;//1
	range[0].NumDescriptors = 1;
	range[0].OffsetInDescriptorsFromTableStart = 0;
	range[0].RegisterSpace = 0;

	//�����W1�̓}�e���A��
	range[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;//b
	range[1].BaseShaderRegister = 0;//0
	range[1].NumDescriptors = 1;
	range[1].OffsetInDescriptorsFromTableStart = 0;
	range[1].RegisterSpace = 0;

	//�����W�Q�̓e�N�X�`��
	range[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;//t
	range[2].BaseShaderRegister = 0;//0
	range[2].NumDescriptors = 4;
	range[2].OffsetInDescriptorsFromTableStart = 1;
	range[2].RegisterSpace = 0;

	//�����W3�̓A�N�^�[�̍��W�ϊ�(���[���h�ƃ{�[��)
	range[3].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;//b
	range[3].BaseShaderRegister = 2;//2
	range[3].NumDescriptors = 2;
	range[3].OffsetInDescriptorsFromTableStart = 0;
	range[3].RegisterSpace = 0;

	range[4] = CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4);

	CD3DX12_ROOT_PARAMETER rp[4] = {};
	//�}�e���A���p(�e�N�X�`�����܂�)
	rp[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rp[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rp[0].DescriptorTable.NumDescriptorRanges = 2;
	rp[0].DescriptorTable.pDescriptorRanges = &range[1];

	//�J�������W�ϊ��p
	rp[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rp[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rp[1].DescriptorTable.NumDescriptorRanges = 1;
	rp[1].DescriptorTable.pDescriptorRanges = &range[0];

	//�A�N�^�[���W�ϊ��p
	rp[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rp[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rp[2].DescriptorTable.NumDescriptorRanges = 1;
	rp[2].DescriptorTable.pDescriptorRanges = &range[3];

	rp[3].InitAsDescriptorTable(1, &range[4]);


	D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
	rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rsDesc.NumParameters = 4;
	rsDesc.pParameters = rp;

	D3D12_STATIC_SAMPLER_DESC sampler[3] = {};
	sampler[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	sampler[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler[0].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
	sampler[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler[0].MaxAnisotropy = 0;
	sampler[0].MaxLOD = D3D12_FLOAT32_MAX;
	sampler[0].MinLOD = 0;
	sampler[0].MipLODBias = 0.0f;
	sampler[0].ShaderRegister = 0;
	sampler[0].RegisterSpace = 0;

	sampler[1] = sampler[0];
	sampler[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler[1].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	sampler[1].ShaderRegister = 1;

	sampler[2] = sampler[0];
	sampler[2].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler[2].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler[2].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler[2].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	sampler[2].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
	sampler[2].MinLOD = -D3D12_FLOAT32_MAX;
	sampler[2].ShaderRegister = 2;
	sampler[2].MaxAnisotropy = 1;


	rsDesc.NumStaticSamplers = 3;
	rsDesc.pStaticSamplers = sampler;
	ID3DBlob* rsBlob = nullptr;//���[�g�V�O�l�`�����\�����邽�߂̃u���u
	ID3DBlob* errBlob = nullptr;
	auto result = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rsBlob, &errBlob);
	if (!CheckResult(result, errBlob)) {
		return false;
	}

	result = _dev->CreateRootSignature(0, //�m�[�h�}�X�N
		rsBlob->GetBufferPointer(), //���[�g�V�O�l�`������邽�߂̃o�C�i��
		rsBlob->GetBufferSize(),// ���̃T�C�Y
		IID_PPV_ARGS(_primitveRS.ReleaseAndGetAddressOf()));
	if (!CheckResult(result, errBlob)) {
		return false;
	}
	return true;
}

bool 
Dx12Wrapper::CreatePrimitivePipeline() {
	ID3DBlob* vsBlob = nullptr;
	ID3DBlob* errBlob = nullptr;
	auto result = D3DCompileFromFile(
		L"BasicVertexShader.hlsl",//�V�F�[�_�t�@�C���p�X
		nullptr,//�}�N����`(D3D_SHADER_MACRO�̃|�C���^)
		D3D_COMPILE_STANDARD_FILE_INCLUDE,//�C���N���[�h��`(ID3DInclude�̃|�C���^)
		"PrimitiveVS",//�G���g���|�C���g�֐���
		"vs_5_0",//�V�F�[�_�̃o�[�W����
		0,//�t���O(���܂�Ӗ��Ȃ�)
		0,//�t���O(���܂�Ӗ��Ȃ�)
		&vsBlob,//�V�F�[�_�R�[�h���R���p�C�������򂪓���
		&errBlob);//�����BLOB(��)�Ȃ񂾂��ǁA���̓G���[�����񂪓���
	if (!CheckResult(result, errBlob)) {
		return false;
	}

	ID3DBlob* psBlob = nullptr;
	result = D3DCompileFromFile(
		L"BasicPixelShader.hlsl",
		nullptr,//�}�N����`(D3D_SHADER_MACRO�̃|�C���^)
		D3D_COMPILE_STANDARD_FILE_INCLUDE,//�C���N���[�h��`(ID3DInclude�̃|�C���^)
		"PrimitivePS", "ps_5_0",
		0, 0, &psBlob, &errBlob);
	if (!CheckResult(result, errBlob)) {
		return false;
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC plsDesc = {};

	//���_�̎d�l
	D3D12_INPUT_ELEMENT_DESC inputdesc[] = {
		{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
		{"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
	};

	plsDesc.InputLayout.NumElements = _countof(inputdesc);
	plsDesc.InputLayout.pInputElementDescs = inputdesc;
	plsDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;


	//���_�V�F�[�_
	plsDesc.VS = CD3DX12_SHADER_BYTECODE(vsBlob);

	//���X�^���C�U
	plsDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

	plsDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;//�J�����O

	//�s�N�Z���V�F�[�_
	plsDesc.PS = CD3DX12_SHADER_BYTECODE(psBlob);


	//�o��
	plsDesc.NumRenderTargets = 2;//�����_�[�^�[�Q�b�g��
	//���Ŏw�肵�������_�[�^�[�Q�b�g���́u�K���v�ݒ肵�Ȃ����
	//�Ȃ�Ȃ���
	plsDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	plsDesc.RTVFormats[1] = DXGI_FORMAT_R8G8B8A8_UNORM;

	plsDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	plsDesc.BlendState.RenderTarget[0].BlendEnable = false;//���̂Ƃ���false
	plsDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	plsDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	//SrcBlend=SRC_ALPHA�Ƃ����̂�Src*���Ƃ�������
	plsDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	plsDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	plsDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	plsDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;



	//�f�v�X�X�e���V���X�e�[�g
	//���̓f�v�X�Ƃ��g�p���Ȃ�(�\���b�h���f�����ɕK�v)
	plsDesc.DepthStencilState.DepthEnable = true;
	plsDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	plsDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

	plsDesc.DepthStencilState.StencilEnable = false;
	plsDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	//�S�̓I�Ȑݒ�
	plsDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	plsDesc.NodeMask = 0;
	plsDesc.SampleDesc.Count = 1;
	plsDesc.SampleDesc.Quality = 0;
	plsDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	plsDesc.pRootSignature = _primitveRS.Get();

	result = _dev->CreateGraphicsPipelineState(&plsDesc, IID_PPV_ARGS(_primitivePipeline.ReleaseAndGetAddressOf()));
	if (!CheckResult(result)) {
		return false;
	}

	return true;

}

bool
Dx12Wrapper::CreateTextureFromImageData(const DirectX::Image* img,ComPtr<ID3D12Resource>& buff,bool isDiscrete) {
	//�܂�WriteToSubresource�����ōl����B
	D3D12_HEAP_PROPERTIES heapprop = {};

	if (isDiscrete) {
		//�O���{���f�B�X�N���[�g�̏ꍇ��DEFAULT�ō���Ă�����
		//�ォ����UPLOAD�o�b�t�@�𒆊ԃo�b�t�@(UPLOAD)�Ƃ��ėp��
		//CopyTextureRegion�œ]������
		heapprop = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	}
	else {//��̌^(UMA)
		heapprop.Type = D3D12_HEAP_TYPE_CUSTOM;//�e�N�X�`���p
		heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
		heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
		heapprop.CreationNodeMask = 0;
		heapprop.VisibleNodeMask = 0;
	}

	//�ŏI�������ݐ惊�\�[�X�̐ݒ�
	D3D12_RESOURCE_DESC resdesc =
	CD3DX12_RESOURCE_DESC::Tex2D(img->format,img->width,img->height);

	auto result = S_OK;
	if (isDiscrete) {
		result = _dev->CreateCommittedResource(&heapprop,
			D3D12_HEAP_FLAG_NONE,
			&resdesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(buff.ReleaseAndGetAddressOf()));
	}else{
		result = _dev->CreateCommittedResource(&heapprop,
			D3D12_HEAP_FLAG_NONE,
			&resdesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			IID_PPV_ARGS(buff.ReleaseAndGetAddressOf()));
	}

	assert(SUCCEEDED(result));
	if (FAILED(result)) {
		return false;
	}

	return true;
}

bool 
Dx12Wrapper::CreateWhiteTexture() {
	D3D12_HEAP_PROPERTIES heapprop = {};
	heapprop.Type = D3D12_HEAP_TYPE_CUSTOM;//�e�N�X�`���p
	heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
	heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	heapprop.CreationNodeMask = 0;
	heapprop.VisibleNodeMask = 0;
	
	D3D12_RESOURCE_DESC resdesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, 4, 4);

	auto result = _dev->CreateCommittedResource(&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(_whiteTex.ReleaseAndGetAddressOf()));

	assert(SUCCEEDED(result));
	if (FAILED(result)) {
		return false;
	}

	////��
	vector<uint8_t> col(4*4*4);
	fill(col.begin(), col.end(), 0xff);
	result = _whiteTex->WriteToSubresource(0, nullptr, col.data(),4*4,col.size());
	assert(SUCCEEDED(result));
	if (FAILED(result)) {
		return false;
	}

	return true;

}

bool
Dx12Wrapper::CreateBlackTexture() {
	D3D12_HEAP_PROPERTIES heapprop = {};
	heapprop.Type = D3D12_HEAP_TYPE_CUSTOM;//�e�N�X�`���p
	heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
	heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	heapprop.CreationNodeMask = 0;
	heapprop.VisibleNodeMask = 0;

	D3D12_RESOURCE_DESC resdesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, 4, 4);
	
	auto result = _dev->CreateCommittedResource(&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(_blackTex.ReleaseAndGetAddressOf()));

	assert(SUCCEEDED(result));
	if (FAILED(result)) {
		return false;
	}

	//��
	vector<uint8_t> col(4 * 4 * 4);
	fill(col.begin(), col.end(), 0);
	result = _blackTex->WriteToSubresource(0, nullptr, col.data(), 4 * 4, col.size());
	if (!CheckResult(result)) {
		return false;
	}
	return true;
}

bool
Dx12Wrapper::CreateGradationTexture() {
	D3D12_HEAP_PROPERTIES heapprop = {};
	heapprop.Type = D3D12_HEAP_TYPE_CUSTOM;//�e�N�X�`���p
	heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
	heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	heapprop.CreationNodeMask = 0;
	heapprop.VisibleNodeMask = 0;

	D3D12_RESOURCE_DESC resdesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, 4, 256);

	auto result = _dev->CreateCommittedResource(&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(_gradTex.ReleaseAndGetAddressOf()));

	assert(SUCCEEDED(result));
	if (FAILED(result)) {
		return false;
	}
	struct Color {
		uint8_t r, g, b, a;
		Color(uint8_t inr,
			uint8_t ing, 
			uint8_t inb, 
			uint8_t ina ):r(inr),
			g(ing) ,
			b(inb) ,
			a(ina) {}
		Color() {}
	};
	//�������O���f
	vector<Color> col(4 * 256);
	auto it = col.begin();
	for (int i = 255; i >=0; --i) {
		fill(it,it+4, Color(i,i,i,255));
		it += 4;
	}
	result = _gradTex->WriteToSubresource(0, nullptr, col.data(), sizeof(Color) * 4, sizeof(Color)*col.size());
	assert(SUCCEEDED(result));
	if (FAILED(result)) {
		return false;
	}

	return true;
}


bool 
Dx12Wrapper::CreateDistortion() {
	if (!LoadPictureFromFile(L"normal/crack_n.png", _distBuff)) {
		return false;
	}
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = 1;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	auto result=_dev->CreateDescriptorHeap(&heapDesc,
		IID_PPV_ARGS(&_distSRVHeap));

	if (!CheckResult(result)) {
		assert(0);
		return false;
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC resDesc = {};
	resDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	resDesc.Format = _distBuff->GetDesc().Format;
	resDesc.Texture2D.MipLevels = 1;
	resDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	//���Ƃ̓r���[�����
	_dev->CreateShaderResourceView(
		_distBuff.Get(),
		&resDesc,
		_distSRVHeap->GetCPUDescriptorHandleForHeapStart());
	return true;

}

XMVECTOR 
Dx12Wrapper::GetCameraPosition() {
	return XMLoadFloat3(&_eye);
}

bool
Dx12Wrapper::LoadPictureFromFile(wstring filepath, ComPtr<ID3D12Resource>& buff) {

	//�����p�X�w�肪������}�b�v�ɂ��łɑ��݂���
	//�f�[�^����󂯎��(�t���C�E�F�C�g�p�^�[��)
	auto it = _textureTable.find(filepath);
	if (it != _textureTable.end()) {
		buff = _textureTable[filepath];
		return true;
	}


	TexMetadata metadata = {};
	ScratchImage scratchImg = {};
	HRESULT result = S_OK;
	//DXT���k�̍ۂɂ͓���ȏ������K�v
	//(4x4�s�N�Z���P�u���b�N�ƂȂ邽�ߍ���1/4�ɂȂ�)
	bool isDXT=false;
	auto ext = GetExtension(filepath);
	if (ext == L"tga") {
		result = LoadFromTGAFile(filepath.c_str(),
			&metadata,
			scratchImg);
	}
	else if (ext == L"dds") {
		result = LoadFromDDSFile(filepath.c_str(),
			DDS_FLAGS_NONE,
			&metadata,
			scratchImg);
		isDXT = true;
	}
	else {
		result = LoadFromWICFile(filepath.c_str(),
			WIC_FLAGS_NONE,
			&metadata,
			scratchImg);
	}
	assert(SUCCEEDED(result));
	if (FAILED(result)) {
		return false;
	}
	auto img = scratchImg.GetImage(0,0,0);
	
	bool isDescrete = true;

	if (!CreateTextureFromImageData(img,buff, isDescrete)) {
		return false;
	}

	if (!isDescrete) {
		result = buff->WriteToSubresource(0, 
			nullptr, 
			img->pixels, 
			img->rowPitch, 
			img->slicePitch);
	}
	else {
		
		//�]���̂��߂̒��ԃo�b�t�@���쐬����
		D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		
		D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(
			AligmentedValue(img->rowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT)*img->height);
		Microsoft::WRL::ComPtr<ID3D12Resource> internalBuffer = nullptr;
		result = _dev->CreateCommittedResource(&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(internalBuffer.ReleaseAndGetAddressOf()));
		assert(SUCCEEDED(result));
		if (FAILED(result)) {
			return false;
		}
		uint8_t* mappedInternal = nullptr;
		internalBuffer->Map(0, nullptr, (void**)&mappedInternal);
		auto address = img->pixels;
		uint32_t height = isDXT ? img->height/4 : img->height;
		for (int i = 0; i < height; ++i) {
			copy_n(address, img->rowPitch, mappedInternal);
			address += img->rowPitch;
			mappedInternal += AligmentedValue(img->rowPitch,
				D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
		}
		internalBuffer->Unmap(0, nullptr);

		D3D12_TEXTURE_COPY_LOCATION src = {}, dst = {};
		src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		src.pResource = internalBuffer.Get();
		src.PlacedFootprint.Offset = 0;
		src.PlacedFootprint.Footprint.Width = img->width;
		src.PlacedFootprint.Footprint.Height = img->height;
		src.PlacedFootprint.Footprint.RowPitch =
			AligmentedValue(img->rowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
		src.PlacedFootprint.Footprint.Depth = metadata.depth;
		src.PlacedFootprint.Footprint.Format = img->format;

		dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dst.SubresourceIndex = 0;
		dst.pResource = buff.Get();

		_cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

		Barrier(buff.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		_cmdList->Close();
		ID3D12CommandList* cmds[] = { _cmdList.Get() };
		_cmdQue->ExecuteCommandLists(1, cmds);
		WaitForCommandQueue();
		_cmdAlloc->Reset();
		_cmdList->Reset(_cmdAlloc.Get(), nullptr);
	}
	
	assert(SUCCEEDED(result));
	if (FAILED(result)) {
		return false;
	}

	_textureTable[filepath] = buff;

	return SUCCEEDED(result);
}
bool 
Dx12Wrapper::CreateTransformConstantBuffer() {
	D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

	D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(
		AligmentedValue(sizeof(SceneMatrix), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)
	);

	auto result = _dev->CreateCommittedResource(&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_sceneCB.ReleaseAndGetAddressOf()));

	assert(SUCCEEDED(result));
	if (FAILED(result)) {
		return false;
	}

	return true;
}

bool 
Dx12Wrapper::CreateTransformBufferView()
{
	//�萔�o�b�t�@�r���[�̍쐬
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 1;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	auto result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(_sceneHeap.ReleaseAndGetAddressOf()));
	if (!CheckResult(result)) {
		return false;
	}

	auto handle = _sceneHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_CONSTANT_BUFFER_VIEW_DESC viewDesc = {};
	viewDesc.BufferLocation = _sceneCB->GetGPUVirtualAddress();
	viewDesc.SizeInBytes = _sceneCB->GetDesc().Width;
	_dev->CreateConstantBufferView(&viewDesc, handle);

	_sceneCB->Map(0, nullptr, (void**)&_mappedScene);
	SetCameraSetting();
	
	return true;
}

bool 
Dx12Wrapper::CreatePera1ResourceAndView() {
	auto& bbuff=_backBuffers[0];
	auto resDesc=bbuff->GetDesc();

	D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Color[0] = clearValue.Color[1] = clearValue.Color[2] = 0.5f;
	clearValue.Color[3] = 1.0f;
	clearValue.Format = resDesc.Format;
	HRESULT result = S_OK;
	for (auto& peraRes : _peraResources) {
		result = _dev->CreateCommittedResource(&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			&clearValue,
			IID_PPV_ARGS(peraRes.ReleaseAndGetAddressOf()));

		if (!CheckResult(result)) {
			assert(0);
			return false;
		}
	}

	//���Ƃ��ƍ���Ă���q�[�v�̏��ł����ꖇ���
	auto heapDesc = _rtvDescHeap->GetDesc();
	//�����_�[�^�[�Q�b�g�r���[(RTV)�����
	//���������̑O�ɂŃX�N���v�^�q�[�v���K�v
	heapDesc.NumDescriptors = 4;//3��4//�W���A�@���A���P�x�A�����Ăڂ����p
	result = _dev->CreateDescriptorHeap(&heapDesc, 
		IID_PPV_ARGS(_peraRTVHeap.ReleaseAndGetAddressOf()));
	if (!CheckResult(result)) {
		assert(0);
		return false;
	}

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	//�܂��f�X�N���v�^�q�[�v�݂̂ŁA�r���[������Ă��Ȃ��̂ō��
	//�����_�[�^�[�Q�b�g�r���[���Q�����
	auto handle = _peraRTVHeap->GetCPUDescriptorHandleForHeapStart();
	for (auto& peraRes : _peraResources) {
		_dev->CreateRenderTargetView(peraRes.Get(),
			&rtvDesc, handle);
		handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}
	_dev->CreateRenderTargetView(_peraResource2.Get(),
		&rtvDesc, handle);

	//�V�F�[�_���\�[�X�r���[�r���[�����
	//���������̑O�ɂŃX�N���v�^�q�[�v���K�v
	heapDesc.NumDescriptors = 4;//3��4�@�W���A�@���A���P�x�A�����Ăڂ����p
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	
	result = _dev->CreateDescriptorHeap(&heapDesc,
		IID_PPV_ARGS(_peraSRVHeap.ReleaseAndGetAddressOf()));



	if (!CheckResult(result)) {
		assert(0);
		return false;
	}
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = rtvDesc.Format;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	handle = _peraSRVHeap->GetCPUDescriptorHandleForHeapStart();
	for (auto& peraRes : _peraResources) {
		_dev->CreateShaderResourceView(peraRes.Get(),
			&srvDesc,
			handle);
		handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}
	_dev->CreateShaderResourceView(_peraResource2.Get(),
		&srvDesc,
		handle);
	return true;

}


// �y���|���Q���ڗp
bool 
Dx12Wrapper::CreatePera2Resource()
{
	auto wsize=Application::Instance().GetWindowSize();
	auto resDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM,
		wsize.width,
		wsize.height);
	resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Color[0] = clearValue.Color[1] = clearValue.Color[2] = 0.5f;
	clearValue.Color[3]=1.0f;
	clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	auto result = _dev->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&clearValue,
		IID_PPV_ARGS(_peraResource2.ReleaseAndGetAddressOf()));
	if (!CheckResult(result)) {
		assert(0);
		return false;
	}



	return true;
}

void 
Dx12Wrapper::DrawToPera2() {
	Barrier(_peraResource2.Get(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_RENDER_TARGET);

	auto rtvHeapPointer = _peraRTVHeap->GetCPUDescriptorHandleForHeapStart();
	rtvHeapPointer.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	_cmdList->OMSetRenderTargets(1, &rtvHeapPointer, false, nullptr);

	auto wsize = Application::Instance().GetWindowSize();

	D3D12_VIEWPORT vp = CD3DX12_VIEWPORT(0.0f, 0.0f, wsize.width, wsize.height);
	_cmdList->RSSetViewports(1, &vp);//�r���[�|�[�g

	CD3DX12_RECT rc(0, 0, wsize.width, wsize.height);
	_cmdList->RSSetScissorRects(1, &rc);//�V�U�[(�؂蔲��)��`

	_cmdList->SetGraphicsRootSignature(_peraRS.Get());
	_cmdList->SetDescriptorHeaps(1, _peraSRVHeap.GetAddressOf());

	auto handle=_peraSRVHeap->GetGPUDescriptorHandleForHeapStart();
	_cmdList->SetGraphicsRootDescriptorTable(1, handle);

	_cmdList->SetDescriptorHeaps(1, _peraCBVHeap.GetAddressOf());
	_cmdList->SetGraphicsRootDescriptorTable(0, _peraCBVHeap->GetGPUDescriptorHandleForHeapStart());

	_cmdList->SetPipelineState(_peraPipeline.Get());
	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	_cmdList->IASetVertexBuffers(0, 1, &_peraVBV);
	_cmdList->DrawInstanced(4, 1, 0, 0);

	Barrier(_peraResource2.Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

}

void 
Dx12Wrapper::SetAO(bool aoFlg) {
	_mappedPostSetting->aoFlg = aoFlg;
}


D3D12_VIEWPORT
Dx12Wrapper::GetViewPort()const {
	auto wsize = Application::Instance().GetWindowSize();
	D3D12_VIEWPORT vp;
	vp.TopLeftX = vp.TopLeftY = 0;
	vp.Width = wsize.width;
	vp.Height = wsize.height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	return vp;
}
