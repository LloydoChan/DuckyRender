struct Output
{
    float4 svpos : SV_Position;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
};

Texture2D<float4> tex : register(t0);
SamplerState smp : register(s0);

float4 main(Output input) : SV_TARGET
{
    //return float4(input.uv, 1.f, 1.f);
    return float4(tex.Sample(smp, input.uv));
}