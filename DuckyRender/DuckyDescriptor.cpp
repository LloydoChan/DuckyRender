#include "pch.h"
#include "DuckyDescriptor.h"
#include "DuckyTools.h"


ConstantBufferAllocation ConstantBufferAllocator::AllocateConstantBuffer(UINT64 size)
{
    const UINT64 alignedSize = AlignConstantBufferSize(size);

    ConstantBufferAllocation allocation;

    if (mCurrentOffset + alignedSize > mCapacity) return allocation;

    allocation.mCpuAddress = mMappedData + mCurrentOffset;
    allocation.mGpuAddress = mGpuBaseAddress + mCurrentOffset;
    mCurrentOffset += alignedSize;

    return allocation;
}

bool ConstantBufferAllocator::Init(ID3D12Device* device, UINT64 capacityBytes)
{
    if (device == nullptr || capacityBytes == 0) return false;

    mCapacity = AlignConstantBufferSize(capacityBytes);

    D3D12_HEAP_PROPERTIES heapProperties = {};
    heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Alignment = 0;
    resourceDesc.Width = mCapacity;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    HRESULT hResult = device->CreateCommittedResource(&heapProperties,
                                                         D3D12_HEAP_FLAG_NONE,
                                                         &resourceDesc,
                                                         D3D12_RESOURCE_STATE_GENERIC_READ,
                                                         nullptr,
                                                         IID_PPV_ARGS(mResourceBuffer.ReleaseAndGetAddressOf()));

    if (FAILED(hResult)) return false;

    hResult = mResourceBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mMappedData));

    if (FAILED(hResult))
    {
        mResourceBuffer.Reset();
        return false;
    }

    mGpuBaseAddress = mResourceBuffer->GetGPUVirtualAddress();
    mCurrentOffset = 0;

    return true;
}
