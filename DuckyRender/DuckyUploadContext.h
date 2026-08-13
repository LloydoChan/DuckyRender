#pragma once

class DuckyUploadContext
{
public:

    ~DuckyUploadContext();
    bool Init(ID3D12Device* device, ID3D12CommandQueue* queue);

    bool Begin();
    ID3D12GraphicsCommandList* GetCommandList() { return mCommandList.Get(); }
    bool SubmitAndWait();
    bool UploadBuffer( ID3D12Resource* destination, ID3D12Resource* upload, size_t size,D3D12_RESOURCE_STATES finalState);

    bool UploadData(D3DDeviceManager* deviceManager, ID3D12Resource* destination, const void* sourceData, size_t size, D3D12_RESOURCE_STATES finalState);

private:
    ID3D12CommandQueue* mQueue = nullptr;

    ComPtr<ID3D12CommandAllocator> mAllocator;
    ComPtr<ID3D12GraphicsCommandList> mCommandList;
    ComPtr<ID3D12Fence> mFence;

    HANDLE mFenceEvent = nullptr;
    uint64_t mFenceValue = 0;

    std::vector<ComPtr<ID3D12Resource>> mPendingUploads;
};