#include "pch.h"
#include "DuckyUploadContext.h"

DuckyUploadContext::~DuckyUploadContext()
{
    if (mFenceEvent)
    {
        CloseHandle(mFenceEvent);
        mFenceEvent = nullptr;
    }
}

bool DuckyUploadContext::Init(ID3D12Device* device, ID3D12CommandQueue* queue)
{
    mQueue = queue;
    HRESULT hResult = device->CreateFence(mFenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(mFence.ReleaseAndGetAddressOf()));
    if (FAILED(hResult)) return false;

    mFenceEvent = CreateEvent(nullptr, false, false, nullptr);
    if (mFenceEvent == nullptr) return false;

    hResult = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mAllocator));
    if (FAILED(hResult)) return false;

    hResult = device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        mAllocator.Get(),
        nullptr,
        IID_PPV_ARGS(&mCommandList));

    if (FAILED(hResult)) return false;

    hResult = mCommandList->Close();

    if (FAILED(hResult)) return false;

    return true;
}

bool DuckyUploadContext::Begin()
{
    HRESULT hResult;

    if (mFenceValue != 0 && mFence->GetCompletedValue() < mFenceValue)
    {
        hResult = mFence->SetEventOnCompletion(mFenceValue, mFenceEvent);
        if (FAILED(hResult)) return false;
        if (WaitForSingleObject(mFenceEvent, INFINITE) != WAIT_OBJECT_0) return false;
    }

    hResult = mAllocator.Get()->Reset();
    if (FAILED(hResult)) return false;

    hResult = mCommandList->Reset(mAllocator.Get(), nullptr);
    if (FAILED(hResult)) return false;

    return true;
}

bool DuckyUploadContext::SubmitAndWait()
{
    if (FAILED(mCommandList->Close())) return false;

    ID3D12CommandList* lists[] = { mCommandList.Get()};

    mQueue->ExecuteCommandLists(1, lists);

    ++mFenceValue;

    if (FAILED(mQueue->Signal(mFence.Get(), mFenceValue))) return false;

    if (mFence->GetCompletedValue() < mFenceValue)
    {
        if (FAILED(mFence->SetEventOnCompletion(mFenceValue, mFenceEvent))) return false;
        if (WaitForSingleObject(mFenceEvent, INFINITE) != WAIT_OBJECT_0) return false;
    }

    mPendingUploads.clear();

    return true;
}

bool DuckyUploadContext::UploadBuffer(ID3D12Resource* destination, ID3D12Resource* upload, size_t size, D3D12_RESOURCE_STATES finalState)
{
    if (!destination || !upload || !mCommandList) return false;

    mCommandList->CopyBufferRegion(destination, 0, upload, 0, size);

    auto barrier =  CD3DX12_RESOURCE_BARRIER::Transition(destination,
                                                        D3D12_RESOURCE_STATE_COPY_DEST,
                                                        finalState);

    mCommandList->ResourceBarrier(1, &barrier);
    mPendingUploads.emplace_back(upload);

    return true;
}
