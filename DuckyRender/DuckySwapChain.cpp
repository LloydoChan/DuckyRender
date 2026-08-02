#include "DuckySwapChain.h"
#include "D3DDeviceManager.h"
#include "DuckyTools.h"

bool DuckySwapChain::Init(D3DDeviceManager* DeviceManager, IDXGIFactory6* factory, HWND hWnd, UINT Width, UINT Height, std::wofstream* LogFile)
{
	mHWnd = hWnd;
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
	ComPtr<IDXGISwapChain1> swapChain1;

	HRESULT hResult = factory->CreateSwapChainForHwnd(DeviceManager->GetCommandQueue(), hWnd, &swapChainDesc, nullptr, nullptr, swapChain1.ReleaseAndGetAddressOf());
	if (FAILED(hResult) && !OutputErrorFromHResult(hResult, "problem creating swapchain: ", *LogFile)) return false;

	hResult = swapChain1.As(&mSwapChain);
	if (FAILED(hResult) && !OutputErrorFromHResult(hResult, "problem exchanging swap chain pointers ", *LogFile)) return false;

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};

	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 2;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	mRtvHeaps = nullptr;

	ID3D12Device* device = DeviceManager->GetDevice();

	hResult = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mRtvHeaps));

	if (FAILED(hResult) && !OutputErrorFromHResult(hResult, "problem creating desc heap: ", *LogFile)) return false;


	D3D12_CPU_DESCRIPTOR_HANDLE handle = mRtvHeaps->GetCPUDescriptorHandleForHeapStart();
	D3D12_RENDER_TARGET_VIEW_DESC srv{};
	srv.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	srv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	srv.Texture2D.MipSlice = 0;

	for (UINT idx = 0; idx < swapChainDesc.BufferCount; idx++)
	{
		ComPtr<ID3D12Resource> ptr;
		mBackBuffers.push_back(ptr);
		hResult = mSwapChain->GetBuffer(idx, IID_PPV_ARGS(&mBackBuffers[idx]));

		if (FAILED(hResult) && !OutputErrorFromHResult(hResult, "problem getting back buffer: ", *LogFile)) return false;

		device->CreateRenderTargetView(mBackBuffers[idx].Get(), &srv, handle);
		handle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	return true;
}

bool DuckySwapChain::Resize(D3DDeviceManager* DeviceManager, UINT32 width, UINT32 height, std::wofstream* LogFile)
{
    if (width == 0 || height == 0)
    {
        return true;
    }

    constexpr UINT bufferCount = 2;
    constexpr DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
    constexpr UINT flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    // All references to the old back buffers must be gone.
    for (ComPtr<ID3D12Resource>& backBuffer :mBackBuffers)
    {
        backBuffer.Reset();
    }

    mBackBuffers.clear();

    HRESULT hResult = mSwapChain->ResizeBuffers(bufferCount, width, height, format, flags);

    if (FAILED(hResult) && !OutputErrorFromHResult(hResult, "couldn't resize swap-chain buffers: ", *LogFile)) return false;

    ID3D12Device* device = DeviceManager->GetDevice();

    const UINT rtvIncrement = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = mRtvHeaps->GetCPUDescriptorHandleForHeapStart();

    mBackBuffers.resize(bufferCount);

    for (UINT index = 0; index < bufferCount; ++index)
    {
		hResult = mSwapChain->GetBuffer(index, IID_PPV_ARGS(mBackBuffers[index].ReleaseAndGetAddressOf()));
        if (FAILED(hResult) && !OutputErrorFromHResult(hResult, "couldn't retrieve resized back buffer: ", *LogFile))return false;
        
        device->CreateRenderTargetView(mBackBuffers[index].Get(), nullptr, rtvHandle);
		rtvHandle.ptr += rtvIncrement;
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