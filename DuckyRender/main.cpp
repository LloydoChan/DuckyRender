#include "pch.h"
#include "SimplestApp.h"
#include "DuckyTools.h"

const int HORIZONTAL_RES = 1920;
const int VERTICAL_RES = 1080;

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	DuckyLog::Initialise();
	std::unique_ptr<SimplestApp> appInstance = std::make_unique<SimplestApp>(HORIZONTAL_RES, VERTICAL_RES);
	if (!appInstance->Init(HORIZONTAL_RES, VERTICAL_RES, L"ducky!")) return 1;
	appInstance->AppMainLoop();
	DuckyLog::Shutdown();
	return 0;
}