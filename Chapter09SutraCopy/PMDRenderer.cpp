#include "PMDRenderer.h"
#include "Dx12Wrapper.h"
#include <d3dcompiler.h>

using namespace DirectX;
using namespace Microsoft::WRL;

std::pair<std::string, std::string> SplitFileName(const std::string& path, const char splitter = '*')
{
	size_t idx = path.find(splitter);
	std::pair<std::string, std::string> ret;
	ret.first = path.substr(0, idx);
	ret.second = path.substr(idx + 1, path.length() - idx - 1);
	return ret;
}

std::string GetTexturePathFromModelAndTexPath(const std::string& modelPath, const char* texPath)
{
	// �t�H���_��؂肪/�ł�\�ł��Ή��ł���悤�ɂ���B
	// rfind�͌�����Ȃ�������epos(-1�A0xffffffff)��Ԃ��B
	int pathIndex1 = (int)modelPath.rfind('/');
	int pathIndex2 = (int)modelPath.rfind('\\');
	int pathIndex = max(pathIndex1, pathIndex2);
	const std::string& folderPath = modelPath.substr(0, pathIndex + 1);
	return folderPath + texPath;
}

PMDRenderer::PMDRenderer(Dx12Wrapper& dx12) : _dx12(dx12)
{
	std::string strModelPath = "model/�����~�N.pmd";
	//std::string strModelPath = "model/�����~�Nmetal.pmd";
	//std::string strModelPath = "model/�������J.pmd";
	HRESULT result = LoadPMDFileAndCreateBuffers(strModelPath);
	if (FAILED(result))
	{
		assert(false);
		return;
	}

	result = CreateRootSignature();
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

void PMDRenderer::PrepareDraw()
{
	_dx12.CommandList()->SetPipelineState(_pipelinestate.Get());
	_dx12.CommandList()->SetGraphicsRootSignature(_rootsignature.Get());
	_dx12.CommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void PMDRenderer::Draw()
{
	_dx12.CommandList()->IASetVertexBuffers(0, 1, &_vbView);
	_dx12.CommandList()->IASetIndexBuffer(&_ibView);

	ID3D12DescriptorHeap* mdh[] = {_materialDescHeap.Get()};
	_dx12.CommandList()->SetDescriptorHeaps(1, mdh);

	// �}�e���A���Z�N�V�������ƂɃ}�e���A����؂�ւ��ĕ`��
	CD3DX12_GPU_DESCRIPTOR_HANDLE materialH(_materialDescHeap->GetGPUDescriptorHandleForHeapStart());
	unsigned int idxOffset = 0;
	UINT cbvsrvIncSize = _dx12.Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 5; // CBV�ƒʏ�e�N�X�`����sph��spa��CLUT��SRV

	for (const Material& m : _materials)
	{
		_dx12.CommandList()->SetGraphicsRootDescriptorTable(1, materialH);
		_dx12.CommandList()->DrawIndexedInstanced(m.indicesNum, 1, idxOffset, 0, 0);
		materialH.ptr += cbvsrvIncSize;
		idxOffset += m.indicesNum;
	}
}

ComPtr<ID3D12Resource> PMDRenderer::CreateGrayGradientTexture()
{
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
	HRESULT result = _dx12.Device()->CreateCommittedResource(
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

ComPtr<ID3D12Resource> PMDRenderer::CreateWhiteTexture()
{
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
	HRESULT result = _dx12.Device()->CreateCommittedResource(
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

ComPtr<ID3D12Resource> PMDRenderer::CreateBlackTexture()
{
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
	HRESULT result = _dx12.Device()->CreateCommittedResource(
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

HRESULT PMDRenderer::LoadPMDFileAndCreateBuffers(const std::string& path)
{
	// PMD�w�b�_�i�[�f�[�^
	struct PMDHeader
	{
		float version;
		char model_name[20];
		char comment[256];
	};

	char signature[3];
	PMDHeader pmdheader = {};
	FILE* fp = nullptr;
	errno_t error = fopen_s(&fp, path.c_str(), "rb");
	fread(signature, sizeof(signature), 1, fp);
	fread(&pmdheader, sizeof(pmdheader), 1, fp);

	unsigned int vertNum;
	fread(&vertNum, sizeof(vertNum), 1, fp);

#pragma pack(1)
	// PMD�}�e���A���f�[�^�ǂݏo���p
	// PMD�̓t�H���V�F�[�f�B���O�̖͗l
	struct PMDMaterial
	{
		XMFLOAT3 diffuse;
		float alpha;
		float specularity;
		XMFLOAT3 specular;
		XMFLOAT3 ambient;
		unsigned char toonIdx;
		unsigned char edgeFlg;
		// �{�������ł��̍\���̂�2�o�C�g�̃p�f�B���O����������
		unsigned int indicesNum;
		char texFilePath[20];
	}; // pack(1)���Ȃ����70�o�C�g�̂͂���72�o�C�g�ɂȂ�
#pragma pack()

	constexpr unsigned int pmdvertex_size = 38;
	std::vector<unsigned char> vertices(vertNum * pmdvertex_size);
	fread(vertices.data(), vertices.size(), 1, fp);

	unsigned int indicesNum;
	fread(&indicesNum, sizeof(indicesNum), 1, fp);

	HRESULT result = _dx12.Device()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(vertices.size()),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_vertBuff.ReleaseAndGetAddressOf())
	);
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	// ���_�o�b�t�@�ւ̃f�[�^��������
	unsigned char* vertMap = nullptr;
	result = _vertBuff->Map(0, nullptr, (void**)&vertMap);
	if (FAILED(result))
	{
		assert(false);
		return result;
	}
	std::copy(vertices.begin(), vertices.end(), vertMap);
	_vertBuff->Unmap(0, nullptr);

	// ���_�o�b�t�@�[�r���[�̗p��
	_vbView.BufferLocation = _vertBuff->GetGPUVirtualAddress();
	_vbView.SizeInBytes = (UINT)vertices.size();
	_vbView.StrideInBytes = pmdvertex_size;

	std::vector<unsigned short> indices(indicesNum);
	fread(indices.data(), indices.size() * sizeof(indices[0]), 1, fp);

	// �}�e���A�����̓ǂݏo��
	unsigned int materialNum;
	fread(&materialNum, sizeof(materialNum), 1, fp);

	_materials.resize(materialNum);
	_textureResources.resize(materialNum);
	_sphResources.resize(materialNum);
	_spaResources.resize(materialNum);
	_toonResources.resize(materialNum);

	ComPtr<ID3D12Resource> whiteTex = CreateWhiteTexture();
	ComPtr<ID3D12Resource> blackTex = CreateBlackTexture();
	ComPtr<ID3D12Resource> gradTex = CreateGrayGradientTexture();
	assert(whiteTex != nullptr);
	assert(blackTex != nullptr);
	assert(gradTex != nullptr);

	{
		std::vector<PMDMaterial> pmdMaterials(materialNum);
		fread(pmdMaterials.data(), pmdMaterials.size() * sizeof(PMDMaterial), 1, fp);

		for (int i = 0; i < pmdMaterials.size(); ++i)
		{
			_materials[i].indicesNum = pmdMaterials[i].indicesNum;
			_materials[i].material.diffuse = pmdMaterials[i].diffuse;
			_materials[i].material.alpha = pmdMaterials[i].alpha;
			_materials[i].material.specular = pmdMaterials[i].specular;
			_materials[i].material.specularity = pmdMaterials[i].specularity;
			_materials[i].material.ambient = pmdMaterials[i].ambient;
			_materials[i].additional.toonIdx = pmdMaterials[i].toonIdx;

			_textureResources[i] = nullptr;
			_sphResources[i] = nullptr;
			_spaResources[i] = nullptr;
			_toonResources[i] = nullptr;

			// �g�D�[���V�F�[�f�B���O�p��CLUT�e�N�X�`�����\�[�X�̃��[�h
			std::string toonFilePath = "toon/";
			char toonFileName[16];
			sprintf_s(toonFileName, 16, "toon%02d.bmp", pmdMaterials[i].toonIdx + 1); // ���̑����Z����255+1��256�ň����邪�A����toon00.bmp�͂Ȃ����߂��̂܂܂ɂ��Ă���
			toonFilePath += toonFileName;
			_toonResources[i] = _dx12.LoadTextureFromFile(toonFilePath);

			if (strlen(pmdMaterials[i].texFilePath) > 0)
			{
				// �ʏ�e�N�X�`���Asph�Aspa�̃��\�[�X�̃��[�h
				std::string texFileName = pmdMaterials[i].texFilePath;
				std::string sphFileName = "";
				std::string spaFileName = "";

				if (std::count(texFileName.begin(), texFileName.end(), '*') > 0)
				{
					const std::pair<std::string, std::string>& namepair = SplitFileName(texFileName);
					if (Dx12Wrapper::GetExtension(namepair.first) == "sph")
					{
						sphFileName = namepair.first;
						texFileName = namepair.second;
					}
					else if (Dx12Wrapper::GetExtension(namepair.first) == "spa")
					{
						spaFileName = namepair.first;
						texFileName = namepair.second;
					}
					else
					{
						texFileName = namepair.first;
						if (Dx12Wrapper::GetExtension(namepair.second) == "sph")
						{
							sphFileName = namepair.second;
						}
						else if (Dx12Wrapper::GetExtension(namepair.second) == "spa")
						{
							spaFileName = namepair.second;
						}
					}
				}
				else
				{
					if (Dx12Wrapper::GetExtension(texFileName) == "sph")
					{
						sphFileName = texFileName;
						texFileName = "";
					}
					else if (Dx12Wrapper::GetExtension(texFileName) == "spa")
					{
						spaFileName = texFileName;
						texFileName = "";
					}
				}

				if (texFileName.length() > 0)
				{
					const std::string& texFilePath = GetTexturePathFromModelAndTexPath(path, texFileName.c_str());
					_textureResources[i] = _dx12.LoadTextureFromFile(texFilePath);
				}

				if (sphFileName.length() > 0)
				{
					const std::string& sphFilePath = GetTexturePathFromModelAndTexPath(path, sphFileName.c_str());
					_sphResources[i] = _dx12.LoadTextureFromFile(sphFilePath);
				}

				if (spaFileName.length() > 0)
				{
					const std::string& spaFilePath = GetTexturePathFromModelAndTexPath(path, spaFileName.c_str());
					_spaResources[i] = _dx12.LoadTextureFromFile(spaFilePath);
				}
			}

			if (_toonResources[i] == nullptr)
			{
				_toonResources[i] = gradTex;
			}

			if (_textureResources[i] == nullptr)
			{
				_textureResources[i] = whiteTex;
			}

			if (_sphResources[i] == nullptr)
			{
				_sphResources[i] = whiteTex;
			}

			if (_spaResources[i] == nullptr)
			{
				_spaResources[i] = blackTex;
			}
		}
	}

	fclose(fp);

	result = _dx12.Device()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(indices.size() * sizeof(indices[0])),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_idxBuff.ReleaseAndGetAddressOf())
	);
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	// �C���f�b�N�X�o�b�t�@�ւ̃f�[�^��������
	unsigned short* idxMap = nullptr;
	result = _idxBuff->Map(0, nullptr, (void**)&idxMap);
	if (FAILED(result))
	{
		assert(false);
		return result;
	}
	std::copy(indices.begin(), indices.end(), idxMap);
	_idxBuff->Unmap(0, nullptr);

	// �C���f�b�N�X�o�b�t�@�[�r���[�̗p��
	_ibView.BufferLocation = _idxBuff->GetGPUVirtualAddress();
	_ibView.Format = DXGI_FORMAT_R16_UINT;
	_ibView.SizeInBytes = (UINT)(indices.size() * sizeof(indices[0]));

	// �}�e���A���o�b�t�@���쐬
	// sizeof(MaterialForHlsl)��44�o�C�g��256�ŃA���C�������g���Ă���̂�256�B
	// ���Ȃ���������Ȃ�
	// TODO:�萔�o�b�t�@���}�e���A������������Ă��邩��A��ɂ܂Ƃ߂��Ȃ����H
	size_t materialBuffSize = (sizeof(MaterialForHlsl) + 0xff) & ~0xff;
	result = _dx12.Device()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(materialBuffSize * materialNum),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_materialBuff.ReleaseAndGetAddressOf())
	);
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	char* mapMaterial = nullptr;
	result = _materialBuff->Map(0, nullptr, (void**)&mapMaterial);
	for (const Material& m : _materials)
	{
		*((MaterialForHlsl*)mapMaterial) = m.material;
		mapMaterial += materialBuffSize;
	}
	_materialBuff->Unmap(0, nullptr);

	// �f�B�X�N���v�^�q�[�v��CBV�쐬
	D3D12_DESCRIPTOR_HEAP_DESC materialDescHeapDesc = {};
	materialDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	materialDescHeapDesc.NodeMask = 0;
	materialDescHeapDesc.NumDescriptors = materialNum * 5; // MaterialForHlsl��CBV�ƒʏ�e�N�X�`����sph��spa��CLUT��SRV��5����
	materialDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	result = _dx12.Device()->CreateDescriptorHeap(&materialDescHeapDesc, IID_PPV_ARGS(_materialDescHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(false);
		return result;
	}

	D3D12_CONSTANT_BUFFER_VIEW_DESC matCBVDesc = {};
	matCBVDesc.BufferLocation = _materialBuff->GetGPUVirtualAddress();
	matCBVDesc.SizeInBytes = (UINT)materialBuffSize;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	// Format�̓e�N�X�`���ɂ��

	D3D12_CPU_DESCRIPTOR_HANDLE matDescHeapH = _materialDescHeap->GetCPUDescriptorHandleForHeapStart();
	UINT incSize = _dx12.Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	for (unsigned int i = 0; i < materialNum; ++i)
	{
		_dx12.Device()->CreateConstantBufferView(
			&matCBVDesc,
			matDescHeapH
		);

		matDescHeapH.ptr += incSize;
		matCBVDesc.BufferLocation += materialBuffSize;

		srvDesc.Format = _textureResources[i]->GetDesc().Format;
		_dx12.Device()->CreateShaderResourceView(
			_textureResources[i].Get(),
			&srvDesc,
			matDescHeapH
		);

		matDescHeapH.ptr += incSize;

		srvDesc.Format = _sphResources[i]->GetDesc().Format;
		_dx12.Device()->CreateShaderResourceView(
			_sphResources[i].Get(),
			&srvDesc,
			matDescHeapH
		);

		matDescHeapH.ptr += incSize;

		srvDesc.Format = _spaResources[i]->GetDesc().Format;
		_dx12.Device()->CreateShaderResourceView(
			_spaResources[i].Get(),
			&srvDesc,
			matDescHeapH
		);

		matDescHeapH.ptr += incSize;

		srvDesc.Format = _toonResources[i]->GetDesc().Format;
		_dx12.Device()->CreateShaderResourceView(
			_toonResources[i].Get(),
			&srvDesc,
			matDescHeapH
		);

		matDescHeapH.ptr += incSize;
	}

	return result;
}

HRESULT PMDRenderer::CreateRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE descTblRange[3] = {}; // VS�p��CBV��PS�p��CBV�ƃe�N�X�`���p��SRV
	// VS�p�̍s��
	descTblRange[0].Init(
		D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
		1,
		0
	);
	// PS�p��MaterialData
	descTblRange[1].Init(
		D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
		1,
		1
	);
	// PS�p�̒ʏ�e�N�X�`����sph��spat��CLUT
	descTblRange[2].Init(
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		4, // �ʏ�e�N�X�`����sph��spat��CLUT
		0
	);

	CD3DX12_ROOT_PARAMETER rootParams[2] = {};
	rootParams[0].InitAsDescriptorTable(1, &descTblRange[0]);
	rootParams[1].InitAsDescriptorTable(2, &descTblRange[1]);

	// �T���v���p�̃��[�g�V�O�l�`���ݒ�
	CD3DX12_STATIC_SAMPLER_DESC samplerDescs[2] = {};
	samplerDescs[0].Init(0);
	samplerDescs[1].Init(
		1,
		D3D12_FILTER_ANISOTROPIC,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP
	);

	// ���[�g�V�O�l�`���쐬
	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Init(
		2,
		rootParams,
		2,
		samplerDescs,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	// TODO:����̓O���t�B�b�N�X�p�C�v���C���X�e�[�g������������������Ă��悢�H
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
	// �V�F�[�_�̏���
	// TODO:����̓O���t�B�b�N�X�p�C�v���C���X�e�[�g������������������Ă��悢�H
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
	if (FAILED(result))
	{
		if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
		{
			::OutputDebugStringA("�t�@�C������������܂���B");
		}
		else
		{
			std::string errstr;
			errstr.resize(errorBlob->GetBufferSize());
			std::copy_n((char*)errorBlob->GetBufferPointer(), errorBlob->GetBufferSize(), errstr.begin());
			OutputDebugStringA(errstr.c_str());
		}

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
	if (FAILED(result))
	{
		if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
		{
			::OutputDebugStringA("�t�@�C������������܂���B");
		}
		else
		{
			std::string errstr;
			errstr.resize(errorBlob->GetBufferSize());
			std::copy_n((char*)errorBlob->GetBufferPointer(), errorBlob->GetBufferSize(), errstr.begin());
			OutputDebugStringA(errstr.c_str());
		}

		assert(false);
		return result;
	}

	// ���_���C�A�E�g�̐ݒ�
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

	// �O���t�B�b�N�X�p�C�v���C���X�e�[�g�쐬
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

