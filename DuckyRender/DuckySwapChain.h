#pragma once

using Microsoft::WRL::ComPtr;

class D3DDeviceManager;

class DuckySwapChain
{
    public:
        bool Init(D3DDeviceManager* DeviceManager, IDXGIFactory6* factory, HWND hWnd, UINT Width, UINT Height);
        void Present() { mSwapChain->Present(1, 0); };
        bool Resize(D3DDeviceManager* DeviceManager, UINT32 width, UINT32 height);

        UINT32 GetCurrentBackBufferIndex() const { return  mSwapChain->GetCurrentBackBufferIndex(); };
        ID3D12Resource* GetCurrentBackBuffer() const;
        D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRtv(D3DDeviceManager* DeviceManager) const;
    private:
        ComPtr<IDXGISwapChain3> mSwapChain;
        ComPtr<ID3D12DescriptorHeap> mRtvHeaps = nullptr;
        std::vector<ComPtr<ID3D12Resource>> mBackBuffers;

        HWND mHWnd = nullptr;

};