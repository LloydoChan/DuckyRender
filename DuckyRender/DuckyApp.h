#pragma once
#include <Windows.h>
#include <DirectXMath.h>
#include <fstream>

class D3DDeviceManager;

class DuckyApp
{
public:
	virtual bool Init(UINT WindowWidth, UINT WindowHeight, const wchar_t* WindowName);
	static LRESULT CALLBACK StaticWindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
	virtual LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) = 0;
	virtual void HandleInput(UINT msg, WPARAM wParam, LPARAM lParam) = 0;
	virtual void AppMainLoop() = 0;
protected:
	
	D3DDeviceManager* mDeviceManager = nullptr;

	HINSTANCE mHInstance = nullptr;
	LPCWSTR mLpszClassName = nullptr;

	std::wofstream mLogFile;

	HANDLE mFenceEvent = nullptr;

	UINT mClientWidth  = 0;
	UINT mClientHeight = 0;
};