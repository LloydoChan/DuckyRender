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

#ifdef OPAQUE
#undef OPAQUE
#endif

enum class PipelineType : uint32_t
{
	OPAQUE,
	ALPHA,
	MASKED,
	OPAQUE_DBL,
	ALPHA_DBL,
	MASKED_DBL,
	DEBUG,
	COUNT
};