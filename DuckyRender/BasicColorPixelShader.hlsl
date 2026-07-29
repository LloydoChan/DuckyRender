struct Input
{
    float4 svpos : SV_Position;
    float3 worldPos : POSITIONT_WS;
    float3 normal : NORMAL_WS;
    float3 tangent : TANGENT_WS;
    float2 uv : TEXCOORD;
};

cbuffer PerFrameConstants : register(b0)
{
    matrix viewProj;
    
    float4 cameraPosition;
    float4 lightDirection;
    float4 lightColor;
};

cbuffer InstanceMaterial : register(b2)
{
    float4 BaseColorFactor;

    float RoughnessFactor;
    float MetallicFactor;
    float NormalScale;
    uint  HasBaseColorTexture;

    uint    HasNormalTexture;
    uint    HasMetallicRoughnessTexture;
    float2 padding;
};

Texture2D<float4> tex : register(t0);
SamplerState smp : register(s0);

float4 main(Input input) : SV_TARGET
{
    return float4(tex.Sample(smp, input.uv));
}