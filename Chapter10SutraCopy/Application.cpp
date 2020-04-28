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

	// TODO:���ߑł��łȂ��O����t�@�C�������w�肵����
	std::string strModelPath = "model/�����~�N.pmd";
	//std::string strModelPath = "model/�����~�Nmetal.pmd";
	//std::string strModelPath = "model/�������J.pmd";
	_pmdActor.reset(new PMDActor(*_dx12, *_pmdRenderer, strModelPath));
	_pmdActor->LoadVMDFile("motion/pose.vmd");

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

		_dx12->BeginDraw();
		_pmdRenderer->PrepareDraw();
		_dx12->SetCamera(); // PrepareDraw()�Ńp�C�v���C���ƃ��[�g�V�O�l�`����PMD�p�ɂɐݒ肷��̂ł��̂��Ƃł���K�v������
		_pmdActor->Draw();
		_dx12->EndDraw();
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
	_windowClass.lpszClassName = _T("DX12Sample");
	_windowClass.hInstance = GetModuleHandle(nullptr);

	RegisterClassEx(&_windowClass);

	RECT wrc = {0, 0, window_width, window_height};
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	_hwnd = CreateWindow(
		_windowClass.lpszClassName,
		_T("DX12�T���v��"),
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
