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
        1.f, 1.f, 1.f, 1.f
    };

    XMFLOAT3 mEmissiveColorFactor =
    {
        0.f, 0.f, 0.f
    };

    float mNormalScale = 1.0f;
    float mRoughnessFactor = 1.0f;
    float mMetallicFactor = 1.0f;

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

struct GPUMaterial
{
    XMFLOAT4 BaseColorFactor;
    XMFLOAT3 EmissiveFactor;

    float NormalScale;
    float RoughnessFactor;
    float MetallicFactor;

    uint32_t alphaMode;
    float alphaCutoff;
    uint32_t doubleSided;

    uint32_t BaseColorTexture;
    uint32_t NormalTexture;
    uint32_t MetallicRoughnessTexture;
    uint32_t EmissiveTexture;
};