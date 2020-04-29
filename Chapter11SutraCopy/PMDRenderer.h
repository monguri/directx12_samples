#pragma once
#include <wrl.h>
#include <d3d12.h>

// �����_���[�Ƃ����APMD�̋��ʏ����N���X�ƌ��������K�؁B
// �p�C�v���C���A���[�g�V�O�l�`���ݒ�A���̑����ʊ֐��B
class PMDRenderer
{
public:
	PMDRenderer(class Dx12Wrapper& dx12);
	void PrepareDraw();

	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	ComPtr<ID3D12Resource> GetGrayGradientTexture();
	ComPtr<ID3D12Resource> GetWhiteTexture();
	ComPtr<ID3D12Resource> GetBlackTexture();

private:
	class Dx12Wrapper& _dx12;

	ComPtr<ID3D12PipelineState> _pipelinestate = nullptr;
	ComPtr<ID3D12RootSignature> _rootsignature = nullptr;

	ComPtr<ID3D12Resource> _gradTex = nullptr;
	ComPtr<ID3D12Resource> _whiteTex = nullptr;
	ComPtr<ID3D12Resource> _blackTex = nullptr;

	HRESULT CreateRootSignature();
	HRESULT CreateGraphicsPipeline();

	ComPtr<ID3D12Resource> CreateGrayGradientTexture();
	ComPtr<ID3D12Resource> CreateWhiteTexture();
	ComPtr<ID3D12Resource> CreateBlackTexture();
};

