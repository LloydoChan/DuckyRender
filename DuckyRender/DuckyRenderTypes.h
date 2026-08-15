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

struct GPUCullConstants
{
    XMFLOAT4 mFrustumPlanes[6];
    uint32_t mDrawCount;
};

struct GPUCullDraw
{
    XMFLOAT3    mCenter;
    XMFLOAT3    mExtents;
    XMFLOAT4    mOrientation;
    uint32_t    mDrawIndex;
    uint32_t    mIndexCount;
    uint32_t    mStartIndex;
    int32_t     mBaseVertex;
};

struct DrawRange
{
    uint32_t Offset = 0;
    uint32_t Count = 0;
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