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

	mWindowHandle = CreateWindow(w.lpszClassName,
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
	rawInputDevice.hwndTarget = mWindowHandle;

	if (!RegisterRawInputDevices(
		&rawInputDevice,
		1,
		sizeof(rawInputDevice)))
	{
		throw std::runtime_error("Failed to register raw mouse input.");
	}

	mDeviceManager = std::make_unique<D3DDeviceManager>();
	if (!mDeviceManager->Init(mWindowHandle, WindowWidth, WindowHeight, &mLogFile)) return false;

	SetWindowLongPtr(
		mWindowHandle,
		GWL_STYLE,
		WS_POPUP | WS_VISIBLE);

	HMONITOR monitor = MonitorFromWindow(mWindowHandle, MONITOR_DEFAULTTOPRIMARY);

	MONITORINFO mi = {};
	mi.cbSize = sizeof(mi);
	GetMonitorInfo(monitor, &mi);

	SetWindowPos(
		mWindowHandle,
		HWND_TOP,
		mi.rcMonitor.left,
		mi.rcMonitor.top,
		mi.rcMonitor.right - mi.rcMonitor.left,
		mi.rcMonitor.bottom - mi.rcMonitor.top,
		SWP_FRAMECHANGED);

	ShowWindow(mWindowHandle, SW_SHOW);
	return true;
}

bool DuckyApp::InitGPUTimeStamps()
{
	mFrameCount = 2;

	HRESULT result = mCommandQueue->GetTimestampFrequency(&mTimeStampFrequency);

	if (FAILED(result) || mTimeStampFrequency == 0) return false;

	const UINT totalQueryCount = mFrameCount * QueriesPerFrame;

	D3D12_QUERY_HEAP_DESC queryHeapDesc{};
	queryHeapDesc.Type =
		D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
	queryHeapDesc.Count = totalQueryCount;
	queryHeapDesc.NodeMask = 0;

	ID3D12Device* device = mDeviceManager->GetDevice();

	result = device->CreateQueryHeap(
		&queryHeapDesc,
		IID_PPV_ARGS(
		mQueryHeap.ReleaseAndGetAddressOf()));

	if (FAILED(result)) return false;

	const UINT64 readbackSize = static_cast<UINT64>(totalQueryCount) * sizeof(UINT64);

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_READBACK;

	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = readbackSize;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	result = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(
		mReadbackBuffer.ReleaseAndGetAddressOf()));

	if (FAILED(result)) return false;

	D3D12_RANGE readRange{ 0, static_cast<SIZE_T>(readbackSize)};

	result = mReadbackBuffer->Map(
		0,
		&readRange,
		reinterpret_cast<void**>(
		&mMappedTimestamps));

	if (FAILED(result))
	{
		mMappedTimestamps = nullptr;
		mReadbackBuffer.Reset();
		mQueryHeap.Reset();
		return false;
	}

	return true;
}

void DuckyApp::StartTimeStamp()
{
	
}

void DuckyApp::EndTimeStamp()
{
	
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