#include <Windows.h>

#include "SimplestApp.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	DuckyApp* appInstance = new SimplestApp();
	if (!appInstance->Init(1920, 1080, L"ducky!")) return 1;
	appInstance->AppMainLoop();
	return 0;
}