#include "pch.h"
#include "DuckyImGui.h"
#include "D3DDeviceManager.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx12.h"

bool DuckyImGui::Init(
    HWND windowHandle,
    D3DDeviceManager* deviceManager,
    UINT descriptorHeapHandle,
    UINT frameCount,
    DXGI_FORMAT renderTargetFormat)
{
    if (mInitialized) return true;
   
    if (windowHandle == nullptr || deviceManager == nullptr) return false;
   
    ID3D12Device* device = deviceManager->GetDevice();
    mDescriptorHeap = deviceManager->GetDescriptorHeapHandleInt();
    mDescriptorHeapMem = deviceManager->GetDescriptorHeapHandle();

    if (device == nullptr) return false;

    DescriptorAllocation fontDescriptor = deviceManager->AllocateCbvSrvUavDescriptor(mDescriptorHeap);

    if (!fontDescriptor.IsValid())
    {
        mDescriptorHeap = 0;
        return false;
    }

    mFontCpuHandle = fontDescriptor.cpu;
    mFontGpuHandle = fontDescriptor.gpu;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    if (!ImGui_ImplWin32_Init(windowHandle))
    {
        ImGui::DestroyContext();
        mDescriptorHeap = 0;
        return false;
    }



    if (!ImGui_ImplDX12_Init(
        device,
        static_cast<int>(2),
        renderTargetFormat,
        deviceManager->GetDescriptorHeapHandle(),
        mFontCpuHandle,
        mFontGpuHandle))
    {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        mDescriptorHeap = 0;
        return false;
    }

    mInitialized = true;
    return true;
}

void DuckyImGui::BeginFrame()
{
    if (!mInitialized)
    {
        return;
    }

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void DuckyImGui::Render(ID3D12GraphicsCommandList* commandList)
{
    if (!mInitialized ||
        commandList == nullptr ||
        mDescriptorHeapMem == nullptr)
    {
        return;
    }

    ImGui::Render();

    ID3D12DescriptorHeap* descriptorHeaps[] =
    {
        mDescriptorHeapMem
    };

    commandList->SetDescriptorHeaps(1, descriptorHeaps);

    ImGui_ImplDX12_RenderDrawData(
        ImGui::GetDrawData(),
        commandList);
}

bool DuckyImGui::WantsMouse() const
{
    return mInitialized && ImGui::GetIO().WantCaptureMouse;
}

bool DuckyImGui::WantsKeyboard() const
{
    return mInitialized && ImGui::GetIO().WantCaptureKeyboard;
}

void DuckyImGui::Shutdown()
{
    if (!mInitialized)
    {
        return;
    }

    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    mInitialized = false;
}