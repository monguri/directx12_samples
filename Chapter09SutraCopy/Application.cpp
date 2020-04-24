#include "Application.h"

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
	return true;
}

void Application::Run()
{
}

void Application::Terminate()
{
}

