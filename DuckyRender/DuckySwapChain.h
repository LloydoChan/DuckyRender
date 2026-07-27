#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <vector>
#include <wrl/client.h>
#include <fstream>

using Microsoft::WRL::ComPtr;

class D3DDeviceManager;

class DuckySwapChain
{
    public:
        bool Init(D3DDeviceManager* DeviceManager, IDXGIFactory6* factory, HWND hWnd, UINT Width, UINT Height, std::wofstream* LogFile);
        void Present() { mSwapChain->Present(1, 0); };
        void Resize(UINT32 width, UINT32 height);

        UINT32 GetCurrentBackBufferIndex() const { return  mSwapChain->GetCurrentBackBufferIndex(); };
        ID3D12Resource* GetCurrentBackBuffer() const;
        D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRtv(D3DDeviceManager* DeviceManager) const;
    private:
        IDXGISwapChain3* mSwapChain;
        ComPtr<ID3D12DescriptorHeap> mRtvHeaps = nullptr;
        std::vector<ComPtr<ID3D12Resource>> mBackBuffers;



};