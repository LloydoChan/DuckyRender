#include "pch.h"
#include "DuckyApp.h"
#include "D3DDeviceManager.h"

DuckyApp::~DuckyApp() = default;

bool DuckyApp::Init(UINT WindowWidth, UINT WindowHeight, const wchar_t* WindowName)
{
	mLogFile.open("log.txt");

	int argc;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	for (int i = 0; i < argc; i++)
	{
		if (wcscmp(argv[i], L"-debug") == 0) {
			ID3D12Debug* debugLayer = nullptr;
			D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer));
			debugLayer->EnableDebugLayer();
			debugLayer->Release();
		}

		if (wcscmp(argv[i], L"-input") == 0) {
			// get the folder name
			std::wstring path = L"..//Assets//CookedAssets//";
			std::wstring asset(argv[i + 1]);
			std::wstring suffix = L"//CookedData.Ducky";

			mInputFilePath = path + asset + suffix;
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

	mDeviceManager = std::make_unique<D3DDeviceManager>();
	if (!mDeviceManager->Init(hwnd, WindowWidth, WindowHeight, &mLogFile)) return false;

	SetWindowLongPtr(
		hwnd,
		GWL_STYLE,
		WS_POPUP | WS_VISIBLE);

	HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);

	MONITORINFO mi = {};
	mi.cbSize = sizeof(mi);
	GetMonitorInfo(monitor, &mi);

	SetWindowPos(
		hwnd,
		HWND_TOP,
		mi.rcMonitor.left,
		mi.rcMonitor.top,
		mi.rcMonitor.right - mi.rcMonitor.left,
		mi.rcMonitor.bottom - mi.rcMonitor.top,
		SWP_FRAMECHANGED);

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