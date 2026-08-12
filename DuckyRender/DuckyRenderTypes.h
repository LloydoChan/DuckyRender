#pragma once

struct GPUDrawData
{
    uint32_t mInstanceIndex;
    uint32_t mMaterialIndex;
};

struct GPUInstance
{
    DirectX::XMFLOAT4X4 mWorld;
    DirectX::XMFLOAT4X4 mNormal;
};

struct IndirectCommand
{
    uint32_t mDrawIndex;
    D3D12_DRAW_INDEXED_ARGUMENTS mDraw;
};