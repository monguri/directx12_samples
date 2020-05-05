#include "Application.h"
#include "Dx12Wrapper.h"
#include "PMDRenderer.h"
#include "PMDActor.h"
#include <tchar.h>

void DebugOutputFormatString(const char* format, ...)
{
#ifdef _DEBUG
	va_list valist;
	va_start(valist, format);
	printf(format, valist);
	va_end(valist);
#endif
}

LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (msg == WM_DESTROY)
	{
		PostQuitMessage(0);
		return 0;
	}

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

	// モデルを複数配置
	_actor = std::make_shared<PMDActor>(*_dx12, "model/初音ミク.pmd");
	_actor->LoadVMDFile("motion/yagokoro.vmd");
	_actor->Move(-10.0f, 0.0f, 0.0f);
	_pmdRenderer->AddActor(_actor);

	const std::shared_ptr<PMDActor>& ruka = std::make_shared<PMDActor>(*_dx12, "model/巡音ルカ.pmd");
	ruka->LoadVMDFile("motion/yagokoro.vmd");
	_pmdRenderer->AddActor(ruka);

	const std::shared_ptr<PMDActor>& haku = std::make_shared<PMDActor>(*_dx12, "model/弱音ハク.pmd");
	haku->LoadVMDFile("motion/yagokoro.vmd");
	haku->Move(-5.0f, 0.0f, 5.0f);
	_pmdRenderer->AddActor(haku);

	const std::shared_ptr<PMDActor>& rin = std::make_shared<PMDActor>(*_dx12, "model/鏡音リン.pmd");
	rin->LoadVMDFile("motion/yagokoro.vmd");
	rin->Move(10.0f, 0.0f, 10.0f);
	_pmdRenderer->AddActor(rin);

	const std::shared_ptr<PMDActor>& meiko = std::make_shared<PMDActor>(*_dx12, "model/咲音メイコ.pmd");
	meiko->LoadVMDFile("motion/yagokoro.vmd");
	meiko->Move(-10.0f, 0.0f, 10.0f);
	_pmdRenderer->AddActor(meiko);

	const std::shared_ptr<PMDActor>& kaito = std::make_shared<PMDActor>(*_dx12, "model/カイト.pmd");
	kaito->LoadVMDFile("motion/yagokoro.vmd");
	kaito->Move(10.0f, 0.0f, 0.0f);
	_pmdRenderer->AddActor(kaito);

	_actor->StartAnimation();

	return true;
}

void Application::Run()
{
	ShowWindow(_hwnd, SW_SHOW);

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

		_pmdRenderer->Update();

		_pmdRenderer->BeforeDrawFromLight();
		_dx12->PreDrawShadow();
		_pmdRenderer->DrawFromLight();

		_pmdRenderer->BeforeDraw();
		_dx12->PreDrawToPera1();
		_pmdRenderer->Draw();
		_dx12->PostDrawToPera1();

		_dx12->DrawHorizontalBokeh();
		_dx12->Draw();
		_dx12->Flip();
	}
}

void Application::Terminate()
{
	UnregisterClass(_windowClass.lpszClassName, _windowClass.hInstance);
}

void Application::CreateGameWindow()
{
	// ウィンドウの生成
	_windowClass.cbSize = sizeof(WNDCLASSEX);
	_windowClass.lpfnWndProc = (WNDPROC)WindowProcedure;
	_windowClass.lpszClassName = _T("DX12Sample");
	_windowClass.hInstance = GetModuleHandle(nullptr);

	RegisterClassEx(&_windowClass);

	RECT wrc = {0, 0, window_width, window_height};
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	_hwnd = CreateWindow(
		_windowClass.lpszClassName,
		_T("DX12サンプル"),
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

