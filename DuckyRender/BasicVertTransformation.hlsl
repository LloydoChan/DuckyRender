struct Output
{
    float4 svpos : SV_Position;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
};

cbuffer cbuff0 : register(b0)
{
    matrix viewProj;
};

cbuffer cbuff1 : register(b1)
{
    matrix instanceTransform;
};

Output main( float4 pos : POSITION, float2 uv : TEXCOORD, float3 normal : NORMAL) 
{
    Output result;
    float4 worldPosition = mul(pos, instanceTransform);
    result.svpos = mul(worldPosition, viewProj);
    result.uv = uv;
    result.normal = normal;
	return result;
}