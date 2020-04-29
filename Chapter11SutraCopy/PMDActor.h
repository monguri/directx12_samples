#pragma once
#include <vector>
#include <map>
#include <unordered_map>
#include <wrl.h>
#include <DirectXMath.h>
#include <d3d12.h>
#include <d3dx12.h>

class PMDActor
{
public:
	PMDActor(class Dx12Wrapper& dx12, class PMDRenderer& renderer, const std::string& modelPath);
	HRESULT LoadVMDFile(const std::string& path);
	void PlayAnimation();
	void Update();
	void Draw();

private:
	class Dx12Wrapper& _dx12;
	class PMDRenderer& _renderer;

	float _angle = 0.0f;

	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	// XMMatrixをにヒープに確保するときに、XMMatrixはSIMD計算のために
	// 16バイトアラインメントになっているため、対応できるようにnewを
	// _aligned_mallocで実装したクラスを用意する
	struct Transform
	{
		void* operator new(size_t size);
		DirectX::XMMATRIX world;
	};

	Transform _transform;
	DirectX::XMMATRIX* _mappedMatrices = nullptr;
	ComPtr<ID3D12DescriptorHeap> _transformDescHeap = nullptr;

	// バッファは描画に用いるので保持し続ける必要がある
	ComPtr<ID3D12Resource> _vertBuff = nullptr;
	ComPtr<ID3D12Resource> _idxBuff = nullptr;
	ComPtr<ID3D12Resource> _materialBuff = nullptr;
	ComPtr<ID3D12Resource> _transformBuff = nullptr;

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

	struct BoneNode
	{
		unsigned short boneIdx;
		DirectX::XMFLOAT3 startPos;
		std::vector<BoneNode*> children;
	};

	std::map<std::string, BoneNode> _boneNodeTable;

	std::vector<DirectX::XMMATRIX> _boneMatrices;

	struct KeyFrame {
		unsigned int frameNo;
		DirectX::XMVECTOR quaternion;
		DirectX::XMFLOAT2 p1, p2;

		KeyFrame(unsigned int fno, const DirectX::XMVECTOR& q, const DirectX::XMFLOAT2& cp1, const DirectX::XMFLOAT2& cp2) : frameNo(fno), quaternion(q), p1(cp1), p2(cp2) {}
	};

	std::unordered_map<std::string, std::vector<KeyFrame>> _motiondata;
	DWORD _startTime = 0;
	unsigned int _duration = 0;

	HRESULT LoadPMDFileAndCreateMeshBuffers(const std::string& path);
	void RecursiveMatrixMultiply(const BoneNode& node, const DirectX::XMMATRIX& mat);
	void MotionUpdate();
	HRESULT CreateTransformConstantBuffer();
	HRESULT CreateMaterialBuffers();
};

