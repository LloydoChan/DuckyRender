#pragma once

class D3DDeviceManager;

class DuckyImGui
{
public:
    bool Init(
        HWND windowHandle,
        D3DDeviceManager* deviceManager,
        UINT descriptorHeapHandle,
        UINT frameCount,
        DXGI_FORMAT renderTargetFormat);

    void BeginFrame();

    void Render(
        ID3D12GraphicsCommandList* commandList);

    void Shutdown();

    bool WantsMouse() const;
    bool WantsKeyboard() const;

private:
    bool mInitialized = false;

    UINT mDescriptorHeap = 0;
    ID3D12DescriptorHeap* mDescriptorHeapMem = nullptr;

    D3D12_CPU_DESCRIPTOR_HANDLE mFontCpuHandle{};
    D3D12_GPU_DESCRIPTOR_HANDLE mFontGpuHandle{};
};