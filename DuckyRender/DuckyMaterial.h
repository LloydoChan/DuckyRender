#pragma once

using namespace DirectX;

const int INVALID_HANDLE = -1;

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


    AlphaMode alphaMode = AlphaMode::Opaque;
    float alphaCutoff = 0.5f;
    unsigned int doubleSided = 0;

    unsigned int padding1;
    unsigned int padding2;
};

struct DuckyMaterial
{
    MaterialConstants constants{};

   int mBaseColorTexture         = INVALID_HANDLE;
   int mNormalTexture            = INVALID_HANDLE;
   int mMetallicRoughnessTexture = INVALID_HANDLE;
   int mEmissive                 = INVALID_HANDLE;
};