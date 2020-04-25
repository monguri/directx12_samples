#pragma once
#include <vector>
#include <wrl.h>
#include <DirectXMath.h>
#include <d3d12.h>
#include <d3dx12.h>

class PMDRenderer
{
public:
	PMDRenderer(class Dx12Wrapper& dx12);
	void Draw();

private:
	class Dx12Wrapper& _dx12;

	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	ComPtr<ID3D12Resource> _vertBuff = nullptr;
	ComPtr<ID3D12Resource> _idxBuff = nullptr;
	ComPtr<ID3D12Resource> _materialBuff = nullptr;

	std::vector<ComPtr<ID3D12Resource>> _textureResources;
	std::vector<ComPtr<ID3D12Resource>> _sphResources;
	std::vector<ComPtr<ID3D12Resource>> _spaResources;
	std::vector<ComPtr<ID3D12Resource>> _toonResources;

	// シェーダに渡すために必要なものだけ選択したデータ
	struct MaterialForHlsl
	{
		DirectX::XMFLOAT3 diffuse;
		float alpha;
		DirectX::XMFLOAT3 specular;
		float specularity;
		DirectX::XMFLOAT3 ambient;
	};

	// PMDMaterialのうち、MaterialForHlsl以外のマテリアル情報をもっておく
	// ためのデータ
	struct AdditionalMaterial
	{
		std::string texPath;
		int toonIdx;
		bool edgeFlg;
	};

	// MaterialForHlslとAdditionalMaterialをまとめたもの
	struct Material
	{
		unsigned int indicesNum;
		MaterialForHlsl material;
		AdditionalMaterial additional;
	};

	std::vector<Material> _materials;

	ComPtr<ID3D12DescriptorHeap> _materialDescHeap = nullptr;

	D3D12_VERTEX_BUFFER_VIEW _vbView;
	D3D12_INDEX_BUFFER_VIEW _ibView;

	ComPtr<ID3D12PipelineState> _pipelinestate = nullptr;
	ComPtr<ID3D12RootSignature> _rootsignature = nullptr;

	ComPtr<ID3D12Resource> CreateGrayGradientTexture();
	ComPtr<ID3D12Resource> CreateWhiteTexture();
	ComPtr<ID3D12Resource> CreateBlackTexture();
	HRESULT LoadPMDFileAndCreateBuffers(const std::string& path);
	HRESULT CreateRootSignature();
	HRESULT CreateGraphicsPipeline();
};

