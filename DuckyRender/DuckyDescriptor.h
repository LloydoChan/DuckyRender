#pragma once

#include <d3d12.h>

const UINT INVALID_INDEX = 2000;

struct DescriptorAllocation
{
    UINT32 index = INVALID_INDEX;
    D3D12_CPU_DESCRIPTOR_HANDLE cpu{};
    D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
};

class DescriptorAllocator
{
public:
    DescriptorAllocation Allocate();
    void FreeDeferred(
        DescriptorAllocation allocation,
        UINT64 fenceValue);
};