#include "PMDRenderer.h"
#include "Dx12Wrapper.h"
#include "PMDActor.h"
#include <d3dcompiler.h>

using namespace Microsoft::WRL;

PMDRenderer::PMDRenderer(Dx12Wrapper& dx12) : _dx12(dx12)
{
	HRESULT result = CreateRootSignature();
	if (FAILED(result))
	{
		assert(false);
		return;
	}

	result = CreateGraphicsPipeline();
	if (FAILED(result))
	{
		assert(false);
		return;
	}

	return;
}

HRESULT PMDRenderer::CreateRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE descTblRange[4] = {}; // VS用のCBVとPS用のCBVとテクスチャ用のSRV
	// SceneData b0
	descTblRange[0].Init(
		D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
		1,
		0
	);
	// Transform b1
	descTblRange[1].Init(
		D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
		1,
		1
	);
	// MaterialForHlsl b2
	descTblRange[2].Init(
		D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
		1,
		2
	);
	// PS用の通常テクスチャとsphとspatとCLUT
	descTblRange[3].Init(
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		4, // 通常テクスチャとsphとspatとCLUT
		0
	);

	CD3DX12_ROOT_PARAMETER rootParams[3] = {};
	rootParams[0].InitAsDescriptorTable(1, &descTblRange[0]); // SceneData
	rootParams[1].InitAsDescriptorTable(1, &descTblRange[1]); // Transform
	rootParams[2].InitAsDescriptorTable(2, &descTblRange[2]); // Material

	// サンプラ用のルートシグネチャ設定
	CD3DX12_STATIC_SAMPLER_DESC samplerDescs[2] = {};
	samplerDescs[0].Init(0);
	samplerDescs[1].Init(
		1,
		D3D12_FILTER_ANISOTROPIC,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP
	);

	// ルートシグネチャ作成
	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Init(
		3,
		rootParams,
		2,
		samplerDescs,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	// TODO:これはグラフィックスパイプラインステートが完成したら解放されてもよい？
	ComPtr<ID3DBlob> rootSigBlob = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT result = D3D12SerializeRootSignature(
		&rootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1_0,
		&rootSigBlob,
		&errorBlob
	);
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	result = _dx12.Device()->CreateRootSignature(
		0,
		rootSigBlob->GetBufferPointer(),
		rootSigBlob->GetBufferSize(),
		IID_PPV_ARGS(_rootsignature.ReleaseAndGetAddressOf())
	);
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	return result;
}

HRESULT PMDRenderer::CreateGraphicsPipeline()
{
	// シェーダの準備
	// TODO:これはグラフィックスパイプラインステートが完成したら解放されてもよい？
	ComPtr<ID3DBlob> _vsBlob = nullptr;
	ComPtr<ID3DBlob> _psBlob = nullptr;

	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT result = D3DCompileFromFile(
		L"BasicVertexShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicVS",
		"vs_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		&_vsBlob,
		&errorBlob
	);
	if (!_dx12.CheckResult(result, errorBlob.Get())){
		assert(false);
		return result;
	}

	result = D3DCompileFromFile(
		L"BasicPixelShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicPS",
		"ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		&_psBlob,
		&errorBlob
	);
	if (!_dx12.CheckResult(result, errorBlob.Get())){
		assert(false);
		return result;
	}

	// 頂点レイアウトの設定
	D3D12_INPUT_ELEMENT_DESC posInputLayout;
	posInputLayout.SemanticName = "POSITION";
	posInputLayout.SemanticIndex = 0;
	posInputLayout.Format = DXGI_FORMAT_R32G32B32_FLOAT;
	posInputLayout.InputSlot = 0;
	posInputLayout.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	posInputLayout.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	posInputLayout.InstanceDataStepRate = 0;

	D3D12_INPUT_ELEMENT_DESC normalInputLayout;
	normalInputLayout.SemanticName = "NORMAL";
	normalInputLayout.SemanticIndex = 0;
	normalInputLayout.Format = DXGI_FORMAT_R32G32B32_FLOAT;
	normalInputLayout.InputSlot = 0;
	normalInputLayout.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	normalInputLayout.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	normalInputLayout.InstanceDataStepRate = 0;

	D3D12_INPUT_ELEMENT_DESC texcoordInputLayout;
	texcoordInputLayout.SemanticName = "TEXCOORD";
	texcoordInputLayout.SemanticIndex = 0;
	texcoordInputLayout.Format = DXGI_FORMAT_R32G32_FLOAT;
	texcoordInputLayout.InputSlot = 0;
	texcoordInputLayout.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	texcoordInputLayout.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	texcoordInputLayout.InstanceDataStepRate = 0;

	D3D12_INPUT_ELEMENT_DESC bonenoInputLayout;
	bonenoInputLayout.SemanticName = "BONE_NO";
	bonenoInputLayout.SemanticIndex = 0;
	bonenoInputLayout.Format = DXGI_FORMAT_R16G16_UINT;
	bonenoInputLayout.InputSlot = 0;
	bonenoInputLayout.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	bonenoInputLayout.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	bonenoInputLayout.InstanceDataStepRate = 0;

	D3D12_INPUT_ELEMENT_DESC weightInputLayout;
	weightInputLayout.SemanticName = "WEIGHT";
	weightInputLayout.SemanticIndex = 0;
	weightInputLayout.Format = DXGI_FORMAT_R8_UINT;
	weightInputLayout.InputSlot = 0;
	weightInputLayout.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	weightInputLayout.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	weightInputLayout.InstanceDataStepRate = 0;

	D3D12_INPUT_ELEMENT_DESC edgeflgInputLayout;
	edgeflgInputLayout.SemanticName = "EDGE_FLG";
	edgeflgInputLayout.SemanticIndex = 0;
	edgeflgInputLayout.Format = DXGI_FORMAT_R8_UINT;
	edgeflgInputLayout.InputSlot = 0;
	edgeflgInputLayout.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	edgeflgInputLayout.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	edgeflgInputLayout.InstanceDataStepRate = 0;

	D3D12_INPUT_ELEMENT_DESC inputLayouts[] = {
		posInputLayout,
		normalInputLayout,
		texcoordInputLayout,
		bonenoInputLayout,
		weightInputLayout,
		edgeflgInputLayout,
	};

	// グラフィックスパイプラインステート作成
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline = {};
	gpipeline.pRootSignature = _rootsignature.Get();
	gpipeline.VS = CD3DX12_SHADER_BYTECODE(_vsBlob.Get());
	gpipeline.PS = CD3DX12_SHADER_BYTECODE(_psBlob.Get());
	gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	gpipeline.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	gpipeline.DepthStencilState.DepthEnable = true;
	gpipeline.DepthStencilState.StencilEnable = false;
	gpipeline.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	gpipeline.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	gpipeline.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	gpipeline.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	gpipeline.InputLayout.pInputElementDescs = inputLayouts;
	gpipeline.InputLayout.NumElements = _countof(inputLayouts);
	gpipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	gpipeline.NumRenderTargets = 1;
	gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	gpipeline.SampleDesc.Count = 1;
	gpipeline.SampleDesc.Quality = 0;

	result = _dx12.Device()->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(_pipelinestate.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	return result;
}

void PMDRenderer::AddActor(std::shared_ptr<class PMDActor> actor)
{
	_actors.emplace_back(actor);
}

void PMDRenderer::Update()
{
	for (const std::shared_ptr<PMDActor>& actor : _actors)
	{
		actor->Update();
	}
}

void PMDRenderer::BeforeDraw()
{
	_dx12.CommandList()->SetPipelineState(_pipelinestate.Get());
	_dx12.CommandList()->SetGraphicsRootSignature(_rootsignature.Get());
	_dx12.CommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void PMDRenderer::Draw()
{
	for (const std::shared_ptr<PMDActor>& actor : _actors)
	{
		actor->Draw();
	}
}

void PMDRenderer::AnimationStart()
{
	for (const std::shared_ptr<PMDActor>& actor : _actors)
	{
		actor->StartAnimation();
	}
}

