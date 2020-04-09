#include <Windows.h>
#include <tchar.h>
#ifdef _DEBUG
#include <iostream>
#endif

using namespace std;

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

#ifdef _DEBUG
int main()
#else
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
#endif
{
	DebugOutputFormatString("Show window test.");

	WNDCLASSEX w = {};
	w.cbSize = sizeof(WNDCLASSEX);
	w.lpfnWndProc = (WNDPROC)WindowProcedure;
	w.lpszClassName = _T("DX12Sample");
	w.hInstance = GetModuleHandle(nullptr);

	RegisterClassEx(&w);

	RECT wrc = {0, 0, window_width, window_height};
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	HWND hwnd = CreateWindow(
		w.lpszClassName,
		_T("DX12ƒTƒ“ƒvƒ‹"),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		wrc.right - wrc.left,
		wrc.bottom - wrc.top,
		nullptr,
		nullptr,
		w.hInstance,
		nullptr
	);

	ShowWindow(hwnd, SW_SHOW);

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
	}

	UnregisterClass(w.lpszClassName, w.hInstance);
	return 0;
}

