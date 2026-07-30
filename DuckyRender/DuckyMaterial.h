#pragma once
#include <DirectXMath.h>
#include <cstddef>
#include <limits>

using namespace DirectX;

const size_t INVALID_HANDLE = (std::numeric_limits<size_t>::max)();

enum class AlphaMode : uint32_t
{
    Opaque,
    Mask,
    Blend
};

struct MaterialConstants
{
    XMFLOAT4 mBaseColorFactor =
    {
        1.0f, 1.0f, 1.0f, 1.0f
    };

    float mRoughnessFactor = 1.0f;
    float mMetallicFactor = 1.0f;
    float mNormalScale = 1.0f;

    unsigned int mHasBaseColorTexture = 0;
    unsigned int mHasNormalTexture = 0;
    unsigned int mHasMetallicRoughnessTexture = 0;
    unsigned int mHasEmissiveTexture = 0;

    AlphaMode alphaMode;
    float alphaCutoff;
    uint32_t doubleSided;
};

struct DuckyMaterial
{
    MaterialConstants constants{};

    size_t mBaseColorTexture         = INVALID_HANDLE;
    size_t mNormalTexture            = INVALID_HANDLE;
    size_t mMetallicRoughnessTexture = INVALID_HANDLE;
    size_t mEmissive                 = INVALID_HANDLE;
};