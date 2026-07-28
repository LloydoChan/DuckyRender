#pragma once
#include <DirectXMath.h>
#include <cstddef>
#include <limits>

using namespace DirectX;

constexpr size_t InvalidTextureHandle =
(std::numeric_limits<size_t>::max)();

struct MaterialConstants
{
    XMFLOAT4 mBaseColorFactor =
    {
        1.0f, 1.0f, 1.0f, 1.0f
    };

    float mRoughnessFactor = 1.0f;
    float mMetallicFactor = 1.0f;

    unsigned int mBaseColorTexture = 0;
    unsigned int mNormalTexture = 0;
    unsigned int mMetallicRoughnessTexture = 0;

    float mNormalScale = 1.0f;

    XMFLOAT2 padding{};
};

struct DuckyMaterial
{
    MaterialConstants constants{};

    size_t mBaseColorTexture = InvalidTextureHandle;
    size_t mNormalTexture    = InvalidTextureHandle;
    size_t mMetallicRoughnessTexture = InvalidTextureHandle;
};