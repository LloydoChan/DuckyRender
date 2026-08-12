#pragma once

class DuckyUploadContext
{
public:
    bool Init(ID3D12Device* device, ID3D12CommandQueue* queue);

    bool Begin();

    ID3D12GraphicsCommandList* GetCommandList() { return mCommandList.Get(); }

    bool SubmitAndWait();

private:
    ID3D12CommandQueue* mQueue = nullptr;

    ComPtr<ID3D12CommandAllocator> mAllocator;
    ComPtr<ID3D12GraphicsCommandList> mCommandList;
    ComPtr<ID3D12Fence> mFence;

    HANDLE mFenceEvent = nullptr;
    uint64_t mFenceValue = 0;
};