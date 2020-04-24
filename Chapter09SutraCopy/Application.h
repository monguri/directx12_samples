#pragma once
class Application
{
private:
	Application();
	~Application();
	Application(const Application&) = delete;
	void operator=(const Application&) = delete;

public:
	static Application& Instance();

	bool Init();
	void Run();
	void Terminate();
};

