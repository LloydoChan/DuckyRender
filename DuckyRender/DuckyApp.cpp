#include "pch.h"
#include "DuckyApp.h"
#include "D3DDeviceManager.h"
#include "DuckyRenderTypes.h"

DebugVertex boxVertices[8] =
{
	{{-1,-1,-1}},
	{{ 1,-1,-1}},
	{{ 1, 1,-1}},
	{{-1, 1,-1}},

	{{-1,-1, 1}},
	{{ 1,-1, 1}},
	{{ 1, 1, 1}},
	{{-1, 1, 1}},
};


uint16_t boxIndices[24] =
{
	0,1, 1,2, 2,3, 3,0,
	4,5, 5,6, 6,7, 7,4,
	0,4, 1,5, 2,6, 3,7
};


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

void DuckyApp::StartGPUTimeStamp(ID3D12GraphicsCommandList* commandList, UINT frameIndex)
{
	const UINT startQuery = frameIndex * QueriesPerFrame;

	// Timestamp queries use EndQuery for both
	// the start and end timestamp.
	commandList->EndQuery(
		mQueryHeap.Get(),
		D3D12_QUERY_TYPE_TIMESTAMP,
		startQuery);
}

void DuckyApp::EndGPUTimeStamp(ID3D12GraphicsCommandList* commandList, UINT frameIndex)
{
	const UINT startQuery = frameIndex * QueriesPerFrame;

	const UINT endQuery = startQuery + 1;

	commandList->EndQuery(
		mQueryHeap.Get(),
		D3D12_QUERY_TYPE_TIMESTAMP,
		endQuery);

	const UINT64 destinationOffset = static_cast<UINT64>(startQuery) * sizeof(UINT64);

	commandList->ResolveQueryData(
		mQueryHeap.Get(),
		D3D12_QUERY_TYPE_TIMESTAMP,
		startQuery,
		QueriesPerFrame,
		mReadbackBuffer.Get(),
		destinationOffset);
}

double DuckyApp::GetGPUFrameMilliSeconds(UINT frameIndex)
{
	double outMilliseconds = 0.0;

	if (mMappedTimestamps == nullptr ||
		mTimeStampFrequency == 0 ||
		frameIndex >= mFrameCount)
	{
		return false;
	}

	const UINT startIndex = frameIndex * QueriesPerFrame;

	const UINT endIndex = startIndex + 1;

	const UINT64 startTicks = mMappedTimestamps[startIndex];

	const UINT64 endTicks = mMappedTimestamps[endIndex];

	if (endTicks < startTicks) return false;

	const UINT64 elapsedTicks = endTicks - startTicks;

	outMilliseconds = static_cast<double>(elapsedTicks) * 1000.0 / static_cast<double>(mTimeStampFrequency);

	return outMilliseconds;
}

bool DuckyApp::InitGPUStats()
{
	D3D12_QUERY_HEAP_DESC queryHeapDesc{};
	queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS;
	queryHeapDesc.Count = 2;
	queryHeapDesc.NodeMask = 0;

	ID3D12Device* device = mDeviceManager->GetDevice();

	HRESULT result = device->CreateQueryHeap(
		&queryHeapDesc,
		IID_PPV_ARGS(
			mPipelineStatsHeap.ReleaseAndGetAddressOf()));

	if (FAILED(result)) return false;

	const UINT64 bufferSize = sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS) * 2;

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_READBACK;

	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = bufferSize;
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
		IID_PPV_ARGS(mPipelineStatsReadback.ReleaseAndGetAddressOf()));

	if (FAILED(result)) return false;

	return true;
}

void DuckyApp::StartGpuStats(ID3D12GraphicsCommandList* CommandList, UINT FrameIndex)
{
	CommandList->BeginQuery(
		mPipelineStatsHeap.Get(),
		D3D12_QUERY_TYPE_PIPELINE_STATISTICS,
		FrameIndex);
}

void DuckyApp::EndGPUStats(ID3D12GraphicsCommandList* CommandList, UINT FrameIndex)
{
	CommandList->EndQuery(
		mPipelineStatsHeap.Get(),
		D3D12_QUERY_TYPE_PIPELINE_STATISTICS,
		FrameIndex);

	const UINT64 destinationOffset =
		static_cast<UINT64>(FrameIndex) *
		sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS);

	CommandList->ResolveQueryData(
		mPipelineStatsHeap.Get(),
		D3D12_QUERY_TYPE_PIPELINE_STATISTICS,
		FrameIndex,
		1,
		mPipelineStatsReadback.Get(),
		destinationOffset);
}

D3D12_QUERY_DATA_PIPELINE_STATISTICS DuckyApp::WriteOutGPUStats(UINT FrameIndex)
{
	D3D12_QUERY_DATA_PIPELINE_STATISTICS stats{};

	const SIZE_T offset = static_cast<SIZE_T>(FrameIndex) * sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS);

	D3D12_RANGE readRange{offset, offset + sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS)};

	void* mappedData = nullptr;

	HRESULT result = mPipelineStatsReadback->Map(0, &readRange, &mappedData);

	if (SUCCEEDED(result))
	{
		const auto* frameStats = reinterpret_cast<const D3D12_QUERY_DATA_PIPELINE_STATISTICS*>(static_cast<const std::byte*>(mappedData) +offset);

		stats = *frameStats;
		D3D12_RANGE writtenRange{ 0, 0 };

		mPipelineStatsReadback->Unmap(0, &writtenRange);
	}

	return stats;
}

bool DuckyApp::InitDebugDrawsVBAndIB()
{
	
	size_t vertexBufferSize = 8 * sizeof(DebugVertex);
	size_t indexBufferSize = 24 * sizeof(uint16_t);

	//now create vertex and index buffers
	mDebugVertices = mDeviceManager->CreateBuffer(vertexBufferSize);
	mDebugIndices = mDeviceManager->CreateBuffer(indexBufferSize);

	void* mappedVertices = nullptr;
	HRESULT result = mDebugVertices->Map(0, nullptr, &mappedVertices);

	if (FAILED(result)) return false;

	memcpy(mappedVertices, &boxVertices[0], sizeof(DebugVertex) * 8);

	mDebugVertices->Unmap(0, nullptr);


	void* mappedIndices = nullptr;

	result = mDebugIndices->Map(0, nullptr, &mappedIndices);

	if (FAILED(result)) return false;
	memcpy(mappedIndices, &boxIndices[0], sizeof(uint16_t) * 24);

	mDebugIndices->Unmap(0, nullptr);


	mVbDebugView.BufferLocation = mDebugVertices->GetGPUVirtualAddress();
	mVbDebugView.SizeInBytes = vertexBufferSize;
	mVbDebugView.StrideInBytes = sizeof(DebugVertex);

	mIbDebugView.BufferLocation = mDebugIndices->GetGPUVirtualAddress();
	mIbDebugView.SizeInBytes = indexBufferSize;
	mIbDebugView.Format = DXGI_FORMAT_R16_UINT;

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