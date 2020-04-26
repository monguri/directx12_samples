#pragma once
#include <wrl.h>
#include <d3d12.h>

// レンダラーといいつつ、PMDの共通処理クラスと言う方が適切。
// パイプライン、ルートシグネチャ設定、その他共通関数。
class PMDRenderer
{
public:
	PMDRenderer(class Dx12Wrapper& dx12);
	void PrepareDraw();

	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	ComPtr<ID3D12Resource> CreateGrayGradientTexture();
	ComPtr<ID3D12Resource> CreateWhiteTexture();
	ComPtr<ID3D12Resource> CreateBlackTexture();

private:
	class Dx12Wrapper& _dx12;

	ComPtr<ID3D12PipelineState> _pipelinestate = nullptr;
	ComPtr<ID3D12RootSignature> _rootsignature = nullptr;

	HRESULT CreateRootSignature();
	HRESULT CreateGraphicsPipeline();
};

