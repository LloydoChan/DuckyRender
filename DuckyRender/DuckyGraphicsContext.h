#pragma once
#include "DuckyDescriptor.h"

using Microsoft::WRL::ComPtr;

struct ID3D12GraphicsCommandList;
struct ID3D12CommandAllocator;
class D3DDeviceManager;
struct ID3D12CommandQueue;
struct ID3D12Fence;
struct ID3D12PipelineState;

const unsigned int FrameCount = 2;

class DuckyGraphicsContext
{
public:
    bool Init(D3DDeviceManager* DeviceManager, UINT64 CBVCapacity, std::wofstream* LogFile);
    bool BeginFrame(UINT CurrentFrame, ID3D12Fence* Fence, ID3D12PipelineState* PipelineState, HANDLE event, std::wofstream* LogFile);
    bool EndFrame(UINT CurrentFrame, ID3D12CommandQueue* Queue, ID3D12Fence* Fence);
    bool WaitForGpu(ID3D12CommandQueue* queue, ID3D12Fence* fence, HANDLE eventHandle);

    ID3D12GraphicsCommandList* GetCommandList()
    {
        return mCommandList.Get();
    }

    ConstantBufferAllocator* GetBufferAllocator(UINT CurrentFrame) { return &mFrames[CurrentFrame].mBufferAllocator; }

private:
    struct FrameContext
    {
        ComPtr<ID3D12CommandAllocator> mCmdAllocator;
        UINT64 fenceValue = 0;
        ConstantBufferAllocator mBufferAllocator;
    };

    UINT64 mGlobalFenceValue = 0;
    FrameContext mFrames[FrameCount];
    ComPtr<ID3D12GraphicsCommandList> mCommandList;
};