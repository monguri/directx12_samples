#pragma once
#include <d3d12.h>
#include <d3dx12.h>
#include <DirectXMath.h>
#include <vector>
#include <map>
#include <wrl.h>

class Dx12Wrapper
{
public:
	Dx12Wrapper(HWND hwnd);
	void Draw(float& angle);

private:
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;
	ComPtr<ID3D12Resource> _depthBuffer = nullptr;
	ComPtr<ID3D12Resource> _vertBuff = nullptr;
	ComPtr<ID3D12Resource> _idxBuff = nullptr;
	ComPtr<ID3D12Resource> _materialBuff = nullptr;
	ComPtr<ID3D12Resource> _constBuff = nullptr;

	std::vector<ComPtr<ID3D12Resource>> _textureResources;
	std::vector<ComPtr<ID3D12Resource>> _sphResources;
	std::vector<ComPtr<ID3D12Resource>> _spaResources;
	std::vector<ComPtr<ID3D12Resource>> _toonResources;

	DirectX::XMMATRIX _worldMat;

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

	struct SceneData* _mapScene = nullptr;

	ComPtr<ID3D12DescriptorHeap> _basicDescHeap = nullptr;
	ComPtr<ID3D12DescriptorHeap> _materialDescHeap = nullptr;

	ComPtr<ID3D12Fence> _fence = nullptr;
	UINT _fenceVal = 0;

	D3D12_VERTEX_BUFFER_VIEW _vbView;
	D3D12_INDEX_BUFFER_VIEW _ibView;

	ComPtr<ID3D12PipelineState> _pipelinestate = nullptr;
	ComPtr<ID3D12RootSignature> _rootsignature = nullptr;

	std::vector<ComPtr<ID3D12Resource>> _backBuffers;

	ComPtr<ID3D12DescriptorHeap> _rtvHeaps = nullptr;
	ComPtr<ID3D12DescriptorHeap> _dsvHeap = nullptr;

	CD3DX12_VIEWPORT _viewport;
	CD3DX12_RECT _scissorrect;

	HRESULT CreateDXGIDevice();
	HRESULT CreateCommand();
	HRESULT CreateSwapChain();
	HRESULT CreateFinalRenderTarget(const struct DXGI_SWAP_CHAIN_DESC1& swapchainDesc);
	HRESULT CreateDepthStencil();
	HRESULT LoadPMDFileAndCreateBuffers(const std::string& path);
	HRESULT CreateRootSignature();
	HRESULT CreateGraphicsPipeline();
	HRESULT CreateCameraConstantBuffer();
};

