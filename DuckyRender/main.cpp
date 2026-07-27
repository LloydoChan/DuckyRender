#include <Windows.h>

#include "SimplestApp.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	std::unique_ptr<SimplestApp> appInstance = std::make_unique<SimplestApp>();
	if (!appInstance->Init(1920, 1080, L"ducky!")) return 1;
	appInstance->AppMainLoop();
	return 0;
}