#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <vector>
#include <memory>

// �����_���[�Ƃ����APMD�̋��ʏ����N���X�ƌ��������K�؁B
// �p�C�v���C���A���[�g�V�O�l�`���ݒ�A���̑����ʊ֐��B
class PMDRenderer
{
public:
	PMDRenderer(class Dx12Wrapper& dx12);

	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	ComPtr<ID3D12Resource> GetGrayGradientTexture();
	ComPtr<ID3D12Resource> GetWhiteTexture();
	ComPtr<ID3D12Resource> GetBlackTexture();
	void AddActor(std::shared_ptr<class PMDActor> actor);

	void Update();
	void BeforeDraw();
	void BeforeDrawFromLight();
	void Draw();
	void DrawFromLight();
	void AnimationStart();

private:
	class Dx12Wrapper& _dx12;
	std::vector<std::shared_ptr<class PMDActor>> _actors;

	ComPtr<ID3D12PipelineState> _pls = nullptr;
	ComPtr<ID3D12RootSignature> _rootsignature = nullptr;
	ComPtr<ID3D12PipelineState> _plsShadow = nullptr;

	HRESULT CreateRootSignature();
	HRESULT CreateGraphicsPipeline();
};

