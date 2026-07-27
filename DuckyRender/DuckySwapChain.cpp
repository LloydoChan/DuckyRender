#include "DuckySwapChain.h"
#include "D3DDeviceManager.h"
#include "DuckyTools.h"

bool DuckySwapChain::Init(D3DDeviceManager* DeviceManager, IDXGIFactory6* factory, HWND hWnd, UINT Width, UINT Height, std::wofstream* LogFile)
{
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};

	swapChainDesc.Width = Width;
	swapChainDesc.Height = Height;
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
	IDXGISwapChain3** swapPtr = &mSwapChain;

	HRESULT hResult = factory->CreateSwapChainForHwnd(DeviceManager->GetCommandQueue(), hWnd, &swapChainDesc, nullptr, nullptr, (IDXGISwapChain1**)swapPtr);
	factory->Release();

	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "problem creating swapchain: ", *LogFile)) return false;

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};

	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 2;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	mRtvHeaps = nullptr;

	ID3D12Device* device = DeviceManager->GetDevice();

	hResult = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mRtvHeaps));

	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "problem creating desc heap: ", *LogFile)) return false;


	D3D12_CPU_DESCRIPTOR_HANDLE handle = mRtvHeaps->GetCPUDescriptorHandleForHeapStart();

	for (UINT idx = 0; idx < swapChainDesc.BufferCount; idx++)
	{
		ComPtr<ID3D12Resource> ptr;
		mBackBuffers.push_back(ptr);
		hResult = mSwapChain->GetBuffer(idx, IID_PPV_ARGS(&mBackBuffers[idx]));

		if (hResult != S_OK && !OutputErrorFromHResult(hResult, "problem getting back buffer: ", *LogFile)) return false;

		device->CreateRenderTargetView(mBackBuffers[idx].Get(), nullptr, handle);
		handle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	return true;
}

ID3D12Resource* DuckySwapChain::GetCurrentBackBuffer() const
{
	UINT bbIdx = mSwapChain->GetCurrentBackBufferIndex();
	return mBackBuffers[bbIdx].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE DuckySwapChain::GetCurrentRtv(D3DDeviceManager* DeviceManager) const
{
	// d3d12 code
	UINT bbIdx = mSwapChain->GetCurrentBackBufferIndex();
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHeap = mRtvHeaps->GetCPUDescriptorHandleForHeapStart();
	rtvHeap.ptr += bbIdx * DeviceManager->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	return rtvHeap;
}
