#pragma once
#include <Windows.h>
#include <DirectXMath.h>
#include <fstream>

using namespace DirectX;

class D3DDeviceManager;

const XMFLOAT4 BaseColorFallback{ 1.f, 1.f, 1.f, 1.f };
const XMFLOAT4 NormalFallback{ 0.5f, 0.5f, 1.f, 1.f };

class DuckyApp
{
public:

	virtual ~DuckyApp();

	virtual bool Init(UINT WindowWidth, UINT WindowHeight, const wchar_t* WindowName);
	static LRESULT CALLBACK StaticWindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
	virtual LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) = 0;
	virtual void HandleInput(UINT msg, WPARAM wParam, LPARAM lParam) = 0;
	virtual void AppMainLoop() = 0;
protected:
	
	std::unique_ptr<D3DDeviceManager> mDeviceManager;

	HINSTANCE mHInstance = nullptr;
	LPCWSTR mLpszClassName = nullptr;

	std::wofstream mLogFile;

	HANDLE mFenceEvent = nullptr;

	UINT mClientWidth  = 0;
	UINT mClientHeight = 0;

	bool mMinimized = false;

	size_t mBaseColorFallbackHandle = 0;
	size_t mNormalColorFallbackHandle = 0;

	std::wstring mInputFilePath;
};