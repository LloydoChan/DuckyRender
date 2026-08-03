#pragma once


using Microsoft::WRL::ComPtr;

const UINT INVALID_INDEX = 2000;

struct ConstantBufferAllocation;

struct ConstantBufferAllocator
{
    ComPtr<ID3D12Resource> mResourceBuffer;
    unsigned char* mMappedData = nullptr;

    D3D12_GPU_VIRTUAL_ADDRESS mGpuBaseAddress = 0;
    UINT64 mCapacity = 0;
    UINT64 mCurrentOffset = 0;

    ConstantBufferAllocation AllocateConstantBuffer(UINT64 size);

    void Reset() { mCurrentOffset = 0; }
    bool Init(ID3D12Device* device, UINT64 capacityBytes);
};

struct ConstantBufferAllocation
{
    void* mCpuAddress = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS mGpuAddress = 0;
};