#include <Windows.h>
#include <winrt/base.h>
#include <tchar.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <fstream>
#include <comdef.h>
#include <vector>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "WindowsApp.lib")

const int WINDOW_WIDTH = 1920;
const int WINDOW_HEIGHT = 1080;

LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (msg == WM_DESTROY)
	{
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	std::wofstream logFile("log.txt");

	RECT wrc = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	WNDCLASSEX w = {};

	w.cbSize = sizeof(WNDCLASSEX);
	w.lpfnWndProc = (WNDPROC)WindowProcedure;
	w.lpszClassName = _T("DuckyRender");
	w.hInstance = GetModuleHandle(nullptr);

	RegisterClassEx(&w);

	HWND hwnd = CreateWindow(w.lpszClassName,
		_T("Ducky!!"),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		wrc.right - wrc.left,
		wrc.bottom - wrc.top,
		nullptr,
		nullptr,
		w.hInstance,
		nullptr);

	ID3D12Device* device = nullptr;
	HRESULT hResult = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&device));

	if (hResult != S_OK)
	{
		const winrt::hstring message = winrt::hresult_error(hResult).message().c_str();
		logFile << L"problem creating device: " << message << std::endl;
		return 1;
	}

	IDXGIFactory6* factory = nullptr;
	hResult = CreateDXGIFactory1(IID_PPV_ARGS(&factory));

	if (hResult != S_OK)
	{
		const winrt::hstring message = winrt::hresult_error(hResult).message().c_str();
		logFile << L"problem creating factory: " << message << std::endl;
		return 1;
	}

	std::vector<IDXGIAdapter*> adapters;
	IDXGIAdapter* tmpAdapter = nullptr;
	
	for (int i = 0; 
		 factory->EnumAdapterByGpuPreference(i, 
											 DXGI_GPU_PREFERENCE::DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, 
											IID_PPV_ARGS(&tmpAdapter)) != DXGI_ERROR_NOT_FOUND; 
		 i++)
	{
		adapters.push_back(tmpAdapter);
		DXGI_ADAPTER_DESC desc;
		tmpAdapter->GetDesc(&desc);
		logFile << "found adapter " << desc.Description << std::endl;
	}

	if (adapters.size() == 0)
	{
		logFile << "no adapters found " << std::endl;
		return 1;
	}

	// enumerated by high perf so take first adapter in list
	IDXGIAdapter* chosenAdapter = adapters[0];


	// create lists and queues
	ID3D12CommandAllocator* cmdAllocator = nullptr;
	ID3D12GraphicsCommandList* cmdList = nullptr;

	hResult = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAllocator));

	if (hResult != S_OK)
	{
		const winrt::hstring message = winrt::hresult_error(hResult).message().c_str();
		logFile << L"problem creating command allocator " << message << std::endl;
		return 1;
	}

	hResult = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAllocator, nullptr, IID_PPV_ARGS(&cmdList));

	if (hResult != S_OK)
	{
		const winrt::hstring message = winrt::hresult_error(hResult).message().c_str();
		logFile << L"problem creating command list " << message << std::endl;
		return 1;
	}

	ID3D12CommandQueue* commandQueue = nullptr;
	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	cmdQueueDesc.NodeMask = 0;
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	hResult = device->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&commandQueue));

	if (hResult != S_OK)
	{
		const winrt::hstring message = winrt::hresult_error(hResult).message().c_str();
		logFile << L"problem creating command queue: " << message << std::endl;
		return 1;
	}

	IDXGISwapChain1* swapChain = nullptr;

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};

	swapChainDesc.Width = WINDOW_WIDTH;
	swapChainDesc.Height = WINDOW_HEIGHT;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.Stereo = false;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
	swapChainDesc.BufferCount = 2;

	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	hResult = factory->CreateSwapChainForHwnd(commandQueue, hwnd, &swapChainDesc, nullptr, nullptr, (IDXGISwapChain1**)&swapChain);

	if (hResult != S_OK)
	{
		const winrt::hstring message = winrt::hresult_error(hResult).message().c_str();
		logFile << L"problem creating swapchain: " << message << std::endl;
		return 1;
	}

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};

	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 2;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	ID3D12DescriptorHeap* rtvHeaps = nullptr;

	hResult = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeaps));

	if (hResult != S_OK)
	{
		const winrt::hstring message = winrt::hresult_error(hResult).message().c_str();
		logFile << L"problem creating descriptor heap: " << message << std::endl;
		return 1;
	}

	std::vector<ID3D12Resource*> backBuffers(swapChainDesc.BufferCount);

	D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeaps->GetCPUDescriptorHandleForHeapStart();

	for (int idx = 0; idx < swapChainDesc.BufferCount; idx++)
	{
		hResult = swapChain->GetBuffer(idx, IID_PPV_ARGS(&backBuffers[idx]));

		if (hResult != S_OK)
		{
			const winrt::hstring message = winrt::hresult_error(hResult).message().c_str();
			logFile << L"problem getting back buffer: " << message << std::endl;
			return 1;
		}

		device->CreateRenderTargetView(backBuffers[idx], nullptr, handle);
		handle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	ShowWindow(hwnd, SW_SHOW);

	MSG msg = {};

	while (true)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (msg.message == WM_QUIT)
		{
			break;
		}

		static int bufferIndex = 0;

		// d3d12 code
		auto rtvHeap = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
		rtvHeap.ptr += bufferIndex * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		float clearColor[] = {0.f, 0.f, 0.f, 1.f};

		cmdList->OMSetRenderTargets(1, &rtvHeap, true, nullptr);
		cmdList->ClearRenderTargetView(rtvHeap, clearColor, 0, nullptr);
		cmdList->Close();

		ID3D12CommandList* cmdLists[] = { cmdList };
		commandQueue->ExecuteCommandLists(1, cmdLists);

		cmdAllocator->Reset();
		cmdList->Reset(cmdAllocator, nullptr);

		swapChain->Present(1, 0);

		bufferIndex++;
		bufferIndex %= 2;
	}

	UnregisterClass(w.lpszClassName, w.hInstance);

	return 0;
}