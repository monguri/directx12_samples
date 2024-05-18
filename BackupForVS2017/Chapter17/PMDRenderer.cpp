#include "PMDRenderer.h"
#include"Helper.h"
#include"Dx12Wrapper.h"
#include"PMDActor.h"
#include<d3dx12.h>
#include<d3dcompiler.h>

using namespace std;

PMDRenderer::PMDRenderer(shared_ptr<Dx12Wrapper> dx):_dx(dx)
{
}


PMDRenderer::~PMDRenderer()
{
}


void 
PMDRenderer::Update() {
	for (auto& actor : _actors) {
		actor->Update();
	}
}


void 
PMDRenderer::DrawFromLight() {
	
	for (auto& actor : _actors) {
		actor->Draw(true);
	}
}

void 
PMDRenderer::BeforeDrawFromLight() {
	auto cmdlist = _dx->CmdList();
	cmdlist->SetPipelineState(_plsShadow.Get());
	cmdlist->SetGraphicsRootSignature(_rootSignature.Get());
}

void 
PMDRenderer::BeforeDraw() {
	auto cmdlist = _dx->CmdList();
	cmdlist->SetPipelineState(_pls.Get());
	cmdlist->SetGraphicsRootSignature(_rootSignature.Get());

}
void 
PMDRenderer::Draw() {
	for (auto& actor : _actors) {
		actor->Draw();
	}
}

void 
PMDRenderer::Init() {
	CreateRootSignature();
	CreatePipeline();
}

void 
PMDRenderer::AnimationStart() {
	for (auto& actor : _actors) {
		actor->StartAmimation();
	}
}

void 
PMDRenderer::AddActor(std::shared_ptr<PMDActor> actor) {
	_actors.emplace_back(shared_ptr<PMDActor>( actor));
}
void 
PMDRenderer::AddActor(const char* filepath) {
	AddActor(make_shared<PMDActor>(_dx,filepath));
}

ID3D12RootSignature* 
PMDRenderer::RootSignature() {
	return _rootSignature.Get();
}
ID3D12PipelineState* 
PMDRenderer::Pipeline() {
	return _pls.Get();
}

bool 
PMDRenderer::CreateRootSignature() {
	D3D12_DESCRIPTOR_RANGE range[6] = {};
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
	range[3].NumDescriptors = 2;//b2,b3
	range[3].OffsetInDescriptorsFromTableStart = 0;
	range[3].RegisterSpace = 0;

	//t4(�[�x)
	range[4] = CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4);
	
	//b4(�\���ݒ�)
	range[5]= CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 4);

	CD3DX12_ROOT_PARAMETER rp[5] = {};
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

	rp[4].InitAsDescriptorTable(1, &range[5]);

	D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
	rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rsDesc.NumParameters = 5;
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

	//�J�����ۂ̃��[�g�V�O�l�`�������
	result = _dx->Device()->CreateRootSignature(0, //�m�[�h�}�X�N
		rsBlob->GetBufferPointer(), //���[�g�V�O�l�`������邽�߂̃o�C�i��
		rsBlob->GetBufferSize(),// ���̃T�C�Y
		IID_PPV_ARGS(_rootSignature.ReleaseAndGetAddressOf()));
	if (!CheckResult(result, errBlob)) {
		return false;
	}
	return true;


}
bool 
PMDRenderer::CreatePipeline() {

	ID3DBlob* vsBlob = nullptr;
	ID3DBlob* errBlob = nullptr;
	auto result = D3DCompileFromFile(
		L"BasicVertexShader.hlsl",//�V�F�[�_�t�@�C���p�X
		nullptr,//�}�N����`(D3D_SHADER_MACRO�̃|�C���^)
		D3D_COMPILE_STANDARD_FILE_INCLUDE,//�C���N���[�h��`(ID3DInclude�̃|�C���^)
		"BasicVS",//�G���g���|�C���g�֐���
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
		"BasicPS", "ps_5_0",
		0, 0, &psBlob, &errBlob);
	if (!CheckResult(result, errBlob)) {
		return false;
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC plsDesc = {};

	//���_�̎d�l
	D3D12_INPUT_ELEMENT_DESC inputdesc[] = {
		{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
		{"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
		{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
		{"BONENO",0,DXGI_FORMAT_R16G16_UINT,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
		{"WEIGHT",0,DXGI_FORMAT_R8_UINT,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
	};

	plsDesc.InputLayout.NumElements = _countof(inputdesc);
	plsDesc.InputLayout.pInputElementDescs = inputdesc;
	plsDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;


	//���_�V�F�[�_
	plsDesc.VS = CD3DX12_SHADER_BYTECODE(vsBlob);

	//���X�^���C�U
	plsDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

	plsDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;//�J�����O
	plsDesc.RasterizerState.DepthBias = 0.1f;
	plsDesc.RasterizerState.SlopeScaledDepthBias = 0.01f;

	//�s�N�Z���V�F�[�_
	plsDesc.PS = CD3DX12_SHADER_BYTECODE(psBlob);


	//�o��
	plsDesc.NumRenderTargets = 3;//�����_�[�^�[�Q�b�g��
	//���Ŏw�肵�������_�[�^�[�Q�b�g���́u�K���v�ݒ肵�Ȃ����
	//�Ȃ�Ȃ���
	plsDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	plsDesc.RTVFormats[1] = DXGI_FORMAT_R8G8B8A8_UNORM;
	plsDesc.RTVFormats[2] = DXGI_FORMAT_R8G8B8A8_UNORM;

	plsDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	plsDesc.BlendState.RenderTarget[0].BlendEnable = true;//���̂Ƃ���false
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
	plsDesc.pRootSignature = _rootSignature.Get();

	result = _dx->Device()->CreateGraphicsPipelineState(&plsDesc, IID_PPV_ARGS(&_pls));
	if (!CheckResult(result)) {
		return false;
	}

	result = D3DCompileFromFile(L"BasicVertexShader.hlsl", 
		nullptr,//�}�N����`(D3D_SHADER_MACRO�̃|�C���^)
		D3D_COMPILE_STANDARD_FILE_INCLUDE,//�C���N���[�h��`(ID3DInclude�̃|�C���^)
		"ShadowVS", "vs_5_0", 0, 0, &vsBlob, &errBlob);
	if (!CheckResult(result, errBlob)) {
		return false;
	}
	result = D3DCompileFromFile(L"BasicPixelShader.hlsl", 
		nullptr,//�}�N����`(D3D_SHADER_MACRO�̃|�C���^)
		D3D_COMPILE_STANDARD_FILE_INCLUDE,//�C���N���[�h��`(ID3DInclude�̃|�C���^)
		"ShadowPS", "ps_5_0", 0, 0, &psBlob, &errBlob);
	if (!CheckResult(result, errBlob)) {
		return false;
	}
	plsDesc.VS = CD3DX12_SHADER_BYTECODE(vsBlob);
	plsDesc.PS = CD3DX12_SHADER_BYTECODE(psBlob);
	result = _dx->Device()->CreateGraphicsPipelineState(&plsDesc, IID_PPV_ARGS(_plsShadow.ReleaseAndGetAddressOf()));

	if (!CheckResult(result)) {
		return false;
	}
	return true;
}