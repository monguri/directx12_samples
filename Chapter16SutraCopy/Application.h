#pragma once
#include <Windows.h>
#include <memory>

class Application
{
public:
	static Application& Instance();

	bool Init();
	void Run();
	void Terminate();
	SIZE GetWindowSize() const;

private:
	WNDCLASSEX _windowClass;
	HWND _hwnd;
	std::shared_ptr<class Dx12Wrapper> _dx12;
	std::shared_ptr<class PMDRenderer> _pmdRenderer;
	std::shared_ptr<class PMDActor> _actor;

	Application();
	~Application();
	Application(const Application&) = delete;
	void operator=(const Application&) = delete;

	void CreateGameWindow();
};

