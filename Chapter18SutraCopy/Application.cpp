#include "Application.h"
#include "Dx12Wrapper.h"
#include "PMDRenderer.h"
#include "PMDActor.h"
#include <Effekseer.h>
#include <EffekseerRendererDX12.h>
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx12.h"
#include <SpriteFont.h>
#include <ResourceUploadBatch.h>

EffekseerRenderer::Renderer* _efkRenderer = nullptr;
Effekseer::Manager* _efkManager = nullptr;
EffekseerRenderer::SingleFrameMemoryPool* _efkMemoryPool = nullptr;
EffekseerRenderer::CommandList* _efkCmdList = nullptr;
Effekseer::Effect* _effect = nullptr;
Effekseer::Handle _efkHandle;

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _heapForSpriteFont = nullptr;
DirectX::GraphicsMemory* _gmemory = nullptr;
DirectX::SpriteFont* _spriteFont = nullptr;
DirectX::SpriteBatch* _spriteBatch = nullptr;


void DebugOutputFormatString(const char* format, ...)
{
#ifdef _DEBUG
	va_list valist;
	va_start(valist, format);
	printf(format, valist);
	va_end(valist);
#endif
}

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (msg == WM_DESTROY)
	{
		PostQuitMessage(0);
		return 0;
	}

	// �E�B���h�E���b�Z�[�W���܂���Imgui�̃n���h���Ŏ󂯎��
	ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam);
	// ����Window�̃f�t�H���g�n���h���Ŏ󂯎��
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

const unsigned int window_width = 1280;
const unsigned int window_height = 720;

Application::Application()
{
}

Application::~Application()
{
}

Application& Application::Instance()
{
	static Application instance;
	return instance;
}

bool Application::Init()
{
	HRESULT result = CoInitializeEx(0, COINIT_MULTITHREADED);
	DebugOutputFormatString("Show window test.");

	CreateGameWindow();

	_dx12.reset(new Dx12Wrapper(_hwnd));
	_pmdRenderer.reset(new PMDRenderer(*_dx12));

	//
	// �t�H���g�pDirectXTK������
	//

	// GraphicsMemory�I�u�W�F�N�g�̏�����
	_gmemory = new DirectX::GraphicsMemory(_dx12->Device().Get());

	// SpriteBatch�I�u�W�F�N�g�̏�����
	DirectX::ResourceUploadBatch resUploadBatch(_dx12->Device().Get());
	resUploadBatch.Begin();

	DirectX::RenderTargetState rtState(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D32_FLOAT);
	DirectX::SpriteBatchPipelineStateDescription pd(rtState);
	_spriteBatch = new DirectX::SpriteBatch(_dx12->Device().Get(), resUploadBatch, pd);
	if (_spriteBatch == nullptr)
	{
		assert(false);
		return false;
	}
	
	// SpriteFont�I�u�W�F�N�g�̏�����
	_heapForSpriteFont = _dx12->CreateDescriptorHeapForSpriteFont();
	if (_heapForSpriteFont == nullptr)
	{
		assert(false);
		return false;
	}

	_spriteFont = new DirectX::SpriteFont(
		_dx12->Device().Get(),
		resUploadBatch,
		L"font/meiryo.spritefont",
		_heapForSpriteFont ->GetCPUDescriptorHandleForHeapStart(),
		_heapForSpriteFont ->GetGPUDescriptorHandleForHeapStart()
	);
	if (_spriteFont == nullptr)
	{
		assert(false);
		return false;
	}

	const std::future<void>& future = resUploadBatch.End(_dx12->CmdQue().Get());
	_dx12->WaitForCommandQueue();
	future.wait();
	_spriteBatch->SetViewport(_dx12->GetViewPort());

	// imgui�̏�����
	if (ImGui::CreateContext() == nullptr)
	{
		assert(false);
		return false;
	}

	bool bInResult = ImGui_ImplWin32_Init(_hwnd);
	if (!bInResult)
	{
		assert(false);
		return false;
	}

	bInResult = ImGui_ImplDX12_Init(
		_dx12->Device().Get(),
		1,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		_dx12->GetHeapForImgui().Get(),
		_dx12->GetHeapForImgui()->GetCPUDescriptorHandleForHeapStart(),
		_dx12->GetHeapForImgui()->GetGPUDescriptorHandleForHeapStart()
	);
	if (!bInResult)
	{
		assert(false);
		return false;
	}

	//
	// Effekseer�̏�����
	//
	DXGI_FORMAT bbFormat[] = {DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM};
	_efkRenderer = EffekseerRendererDX12::Create(
		_dx12->Device().Get(),
		_dx12->CmdQue().Get(),
		2,
		bbFormat,
		1,
		false,
		false,
		10000
	);
	if (_efkRenderer == nullptr)
	{
		assert(false);
		return false;
	}

	_efkManager = Effekseer::Manager::Create(10000);
	if (_efkManager == nullptr)
	{
		assert(false);
		return false;
	}

	// ����n���w��
	_efkManager->SetCoordinateSystem(Effekseer::CoordinateSystem::LH);

	// �e�����_�����v���Z�b�g�̂��̂ɐݒ�
	_efkManager->SetSpriteRenderer(_efkRenderer->CreateSpriteRenderer());
	_efkManager->SetRibbonRenderer(_efkRenderer->CreateRibbonRenderer());
	_efkManager->SetRingRenderer(_efkRenderer->CreateRingRenderer());
	_efkManager->SetTrackRenderer(_efkRenderer->CreateTrackRenderer());
	_efkManager->SetModelRenderer(_efkRenderer->CreateModelRenderer());

	_efkManager->SetTextureLoader(_efkRenderer->CreateTextureLoader());
	_efkManager->SetModelLoader(_efkRenderer->CreateModelLoader());

	// DirectX12��p�̐ݒ�
	_efkMemoryPool = EffekseerRendererDX12::CreateSingleFrameMemoryPool(_efkRenderer);
	if (_efkMemoryPool == nullptr)
	{
		assert(false);
		return false;
	}

	_efkCmdList = EffekseerRendererDX12::CreateCommandList(_efkRenderer, _efkMemoryPool);
	if (_efkCmdList == nullptr)
	{
		assert(false);
		return false;
	}
	_efkRenderer->SetCommandList(_efkCmdList);

	SyncronizeEffekseerCamera();

	// efk�t�@�C���̓ǂݍ���
	_effect = Effekseer::Effect::Create(_efkManager, (const EFK_CHAR*)L"effect/10/SimpleLaser.efk", 1.0f, (const EFK_CHAR*)L"effect/10");
	if (_effect == nullptr)
	{
		assert(false);
		return false;
	}

	// efk�̍Đ�
	_efkHandle = _efkManager->Play(_effect, 0, 0, 0);

	// ���f���𕡐��z�u
	_actor = std::make_shared<PMDActor>(*_dx12, "model/�����~�N.pmd");
	_actor->LoadVMDFile("motion/yagokoro.vmd");
	_actor->Move(-10.0f, 0.0f, 0.0f);
	_pmdRenderer->AddActor(_actor);

	const std::shared_ptr<PMDActor>& ruka = std::make_shared<PMDActor>(*_dx12, "model/�������J.pmd");
	ruka->LoadVMDFile("motion/yagokoro.vmd");
	_pmdRenderer->AddActor(ruka);

	const std::shared_ptr<PMDActor>& haku = std::make_shared<PMDActor>(*_dx12, "model/�㉹�n�N.pmd");
	haku->LoadVMDFile("motion/yagokoro.vmd");
	haku->Move(-5.0f, 0.0f, 5.0f);
	_pmdRenderer->AddActor(haku);

	const std::shared_ptr<PMDActor>& rin = std::make_shared<PMDActor>(*_dx12, "model/��������.pmd");
	rin->LoadVMDFile("motion/yagokoro.vmd");
	rin->Move(10.0f, 0.0f, 10.0f);
	_pmdRenderer->AddActor(rin);

	const std::shared_ptr<PMDActor>& meiko = std::make_shared<PMDActor>(*_dx12, "model/�特���C�R.pmd");
	meiko->LoadVMDFile("motion/yagokoro.vmd");
	meiko->Move(-10.0f, 0.0f, 10.0f);
	_pmdRenderer->AddActor(meiko);

	const std::shared_ptr<PMDActor>& kaito = std::make_shared<PMDActor>(*_dx12, "model/�J�C�g.pmd");
	kaito->LoadVMDFile("motion/yagokoro.vmd");
	kaito->Move(10.0f, 0.0f, 0.0f);
	_pmdRenderer->AddActor(kaito);

	_actor->StartAnimation();

	return true;
}

void Application::Run()
{
	ShowWindow(_hwnd, SW_SHOW);

	constexpr float pi = 3.141592653589f;
	float fov = pi / 4.0f;

	MSG msg = {};
	while (true)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (msg.message == WM_QUIT)
		{
			break;
		}

		_dx12->SetCameraSetting();
		_pmdRenderer->Update();
		SyncronizeEffekseerCamera();

		//
		// �V���h�E�}�b�v�`��
		//
		_pmdRenderer->BeforeDrawFromLight();
		_dx12->PreDrawShadow();
		_pmdRenderer->DrawFromLight();

		//
		// �W�I���g���p�X ��������
		//
		_pmdRenderer->BeforeDraw();
		_dx12->PreDrawToPera1();

		_pmdRenderer->Draw();

		// �G�t�F�N�g�`��
		_efkManager->Update();
		_efkMemoryPool->NewFrame();

		EffekseerRendererDX12::BeginCommandList(_efkCmdList, _dx12->CommandList().Get());
		_efkRenderer->BeginRendering();
		_efkManager->Draw();
		_efkRenderer->EndRendering();
		EffekseerRendererDX12::EndCommandList(_efkCmdList);

		_dx12->PostDrawToPera1();
		// �W�I���g���p�X �����܂�

		//
		// �|�X�g�v���Z�X
		//
#if 0 // �y��2�ɕ`�悷��p�X�͍��͎g��Ȃ��̂ŃR�����g�A�E�g
		_dx12->DrawHorizontalBokeh();
#endif
		_dx12->DrawShrinkTextureForBlur();
		_dx12->DrawAmbientOcclusion();

		//
		// �ŏI�����_�[�^�[�Q�b�g�ւ̕`��
		//
		_dx12->Draw();

		//
		// ImGui�̕`��
		//
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("Rendering Test Menu");
		ImGui::SetWindowSize(ImVec2(400.0f, 500.0f), ImGuiCond_::ImGuiCond_FirstUseEver);

		static bool blnDebugDisp = false;
		ImGui::Checkbox("Debug Display", &blnDebugDisp);
		_dx12->SetDebugDisplay(blnDebugDisp);

		static bool blnSSAO = false;
		ImGui::Checkbox("SSAO on/off", &blnSSAO);
		_dx12->SetSSAO(blnSSAO);

		static bool blnShadowmap = false;
		ImGui::Checkbox("Self Shadow on/off", &blnShadowmap);
		_dx12->SetSelfShadow(blnShadowmap);

		if (ImGui::SliderFloat("Field of view", &fov, pi / 6.0f, pi * 5.0f / 6.0f))
		{
			_dx12->SetFov(fov);
		}

		static float lightVec[3] = {1.0f, -1.0f, 1.0f};
		if (ImGui::SliderFloat3("Light vector", lightVec, -1.0f, 1.0f))
		{
			_dx12->SetLightVector(lightVec);
		}
		
		static float bgCol[4] = {0.5f, 0.5f, 0.5f, 1.0f};
		ImGui::ColorPicker4("BG color", bgCol, ImGuiColorEditFlags_::ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_::ImGuiColorEditFlags_AlphaBar);
		_dx12->SetBackColor(bgCol);

		static float bloomCol[3] = {1.0f, 1.0f, 1.0f};
		ImGui::ColorPicker3("Bloom color", bloomCol);
		_dx12->SetBloomColor(bloomCol);

		ImGui::End();
		ImGui::Render();

		//
		// �t�H���g�̕`��
		//
		_dx12->CommandList().Get()->SetDescriptorHeaps(1, _heapForSpriteFont.GetAddressOf());
		_spriteBatch->Begin(_dx12->CommandList().Get());
		_spriteFont->DrawString(_spriteBatch, L"����ɂ��̓n���[", DirectX::XMFLOAT2(102, 102), DirectX::Colors::Black);
		_spriteFont->DrawString(_spriteBatch, L"����ɂ��̓n���[", DirectX::XMFLOAT2(100, 100), DirectX::Colors::Yellow);
		_spriteBatch->End();

		// ImGui���t�H���g����ɕ`�悷��ƃr���[�|�[�g��ImGui�̂��̂ɂȂ��Ă��܂��̂Ńt�H���g����ɕ`�悷��
		_dx12->CommandList()->SetDescriptorHeaps(1, _dx12->GetHeapForImgui().GetAddressOf());
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), _dx12->CommandList().Get());

		// �X���b�v
		_dx12->Flip();

		_gmemory->Commit(_dx12->CmdQue().Get());
	}
}

void Application::Terminate()
{
	UnregisterClass(_windowClass.lpszClassName, _windowClass.hInstance);
}

void Application::CreateGameWindow()
{
	// �E�B���h�E�̐���
	_windowClass.cbSize = sizeof(WNDCLASSEX);
	_windowClass.lpfnWndProc = (WNDPROC)WindowProcedure;
	_windowClass.lpszClassName = "DX12Sample";
	_windowClass.hInstance = GetModuleHandle(nullptr);

	RegisterClassEx(&_windowClass);

	RECT wrc = {0, 0, window_width, window_height};
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	_hwnd = CreateWindow(
		_windowClass.lpszClassName,
		"DX12�T���v��",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		wrc.right - wrc.left,
		wrc.bottom - wrc.top,
		nullptr,
		nullptr,
		_windowClass.hInstance,
		nullptr
	);
}

SIZE Application::GetWindowSize() const
{
	SIZE ret;
	ret.cx = window_width;
	ret.cy = window_height;
	return ret;
}

void Application::SyncronizeEffekseerCamera()
{
	Effekseer::Matrix44 fkViewMat;
	Effekseer::Matrix44 fkProjMat;
	const DirectX::XMMATRIX& view = _dx12->ViewMatrix();
	const DirectX::XMMATRIX& proj = _dx12->ProjMatrix();

	for (int i = 0; i < 4; ++i)
	{
		for (int j = 0; j < 4; ++j)
		{
			fkViewMat.Values[i][j] = view.r[i].m128_f32[j];
			fkProjMat.Values[i][j] = proj.r[i].m128_f32[j];
		}
	}

	_efkRenderer->SetCameraMatrix(fkViewMat);
	_efkRenderer->SetProjectionMatrix(fkProjMat);
}

