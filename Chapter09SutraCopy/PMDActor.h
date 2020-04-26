#pragma once
#include <vector>
#include <wrl.h>
#include <DirectXMath.h>
#include <d3d12.h>
#include <d3dx12.h>

class PMDActor
{
public:
	PMDActor(class Dx12Wrapper& dx12, class PMDRenderer& renderer, const std::string& modelPath);
	void Draw();

private:
	class Dx12Wrapper& _dx12;
	class PMDRenderer& _renderer;

	float _angle = 0.0f;

	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	// XMMatrix���Ƀq�[�v�Ɋm�ۂ���Ƃ��ɁAXMMatrix��SIMD�v�Z�̂��߂�
	// 16�o�C�g�A���C�������g�ɂȂ��Ă��邽�߁A�Ή��ł���悤��new��
	// _aligned_malloc�Ŏ��������N���X��p�ӂ���
	struct Transform
	{
		void* operator new(size_t size);
		DirectX::XMMATRIX world;
	};

	Transform _transform;
	Transform* _mappedTransform = nullptr;
	ComPtr<ID3D12DescriptorHeap> _transformDescHeap = nullptr;

	// �o�b�t�@�͕`��ɗp����̂ŕێ���������K�v������
	ComPtr<ID3D12Resource> _vertBuff = nullptr;
	ComPtr<ID3D12Resource> _idxBuff = nullptr;
	ComPtr<ID3D12Resource> _materialBuff = nullptr;
	ComPtr<ID3D12Resource> _transformBuff = nullptr;

	std::vector<ComPtr<ID3D12Resource>> _textureResources;
	std::vector<ComPtr<ID3D12Resource>> _sphResources;
	std::vector<ComPtr<ID3D12Resource>> _spaResources;
	std::vector<ComPtr<ID3D12Resource>> _toonResources;

	// �V�F�[�_�ɓn�����߂ɕK�v�Ȃ��̂����I�������f�[�^
	struct MaterialForHlsl
	{
		DirectX::XMFLOAT3 diffuse;
		float alpha;
		DirectX::XMFLOAT3 specular;
		float specularity;
		DirectX::XMFLOAT3 ambient;
	};

	// PMDMaterial�̂����AMaterialForHlsl�ȊO�̃}�e���A�����������Ă���
	// ���߂̃f�[�^
	struct AdditionalMaterial
	{
		std::string texPath;
		int toonIdx;
		bool edgeFlg;
	};

	// MaterialForHlsl��AdditionalMaterial���܂Ƃ߂�����
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

	HRESULT LoadPMDFileAndCreateBuffers(const std::string& path);
	HRESULT CreateTransformConstantBuffer();
};

