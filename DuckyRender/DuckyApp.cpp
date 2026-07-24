#include "DuckyApp.h"
#include "D3DDeviceManager.h"
#include <tchar.h>

#pragma comment(lib, "WindowsApp.lib")



bool DuckyApp::Init(UINT WindowWidth, UINT WindowHeight, const wchar_t* WindowName)
{
	int argc;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	for (int i = 0; i < argc; i++)
	{
		if (wcscmp(argv[i], L"-debug")) {
			ID3D12Debug* debugLayer = nullptr;
			D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer));
			debugLayer->EnableDebugLayer();
			debugLayer->Release();
		}
	}
	LocalFree(argv);

	RECT wrc = { 0, 0, WindowWidth, WindowHeight };
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	WNDCLASSEX w = {};

	w.cbSize = sizeof(WNDCLASSEX);
	w.lpfnWndProc = (WNDPROC)StaticWindowProcedure;
	mLpszClassName = w.lpszClassName = _T("DuckyApp");
	mHInstance     = w.hInstance = GetModuleHandle(nullptr);

	RegisterClassEx(&w);

	HWND hwnd = CreateWindow(w.lpszClassName,
		WindowName,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		wrc.right - wrc.left,
		wrc.bottom - wrc.top,
		nullptr,
		nullptr,
		w.hInstance,
		this); // need for wndproc

	RAWINPUTDEVICE rawInputDevice{};

	rawInputDevice.usUsagePage = 0x01; // Generic desktop controls
	rawInputDevice.usUsage = 0x02;     // Mouse
	rawInputDevice.dwFlags = 0;
	rawInputDevice.hwndTarget = hwnd;

	if (!RegisterRawInputDevices(
		&rawInputDevice,
		1,
		sizeof(rawInputDevice)))
	{
		throw std::runtime_error("Failed to register raw mouse input.");
	}

	mDeviceManager = new D3DDeviceManager();
	if (!mDeviceManager->Init(hwnd, WindowWidth, WindowHeight)) return false;

	ShowWindow(hwnd, SW_SHOW);
	return true;
}

LRESULT DuckyApp::StaticWindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	DuckyApp* app = nullptr;

	if (msg == WM_NCCREATE)
	{
		auto createStruct = reinterpret_cast<CREATESTRUCT*>(lparam);

		app = static_cast<DuckyApp*>(createStruct->lpCreateParams);

		SetWindowLongPtr(
			hwnd,
			GWLP_USERDATA,
			reinterpret_cast<LONG_PTR>(app));

		return DefWindowProc(hwnd, msg, wparam, lparam);
	}

	app = reinterpret_cast<DuckyApp*>(
		GetWindowLongPtr(hwnd, GWLP_USERDATA));

	if (app)
	{
		return app->WindowProcedure(hwnd, msg, wparam, lparam);
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);
}