struct Output
{
    float4 svpos : SV_Position;
    float3 worldPos : POSITIONT_WS;
    float3 normal : NORMAL_WS;
    float4 tangent : TANGENT_WS;
    float2 uv : TEXCOORD;
    float4 col : COLOR;
};

cbuffer cbuff0 : register(b0)
{
    matrix viewProj;
    
    float4 cameraPosition;
    float4 lightDirection;
    float4 lightColor;
};

struct GPUInstance
{
    float4x4 World;
    float4x4 Normal;
};

StructuredBuffer<GPUInstance> Instances : register(t3);

cbuffer DrawConstants : register(b2)
{
    uint InstanceIndex;
    uint MaterialIndex;
};

Output main(float3 pos : POSITION, float3 normal : NORMAL, float4 tangent : TANGENT, float2 uv : TEXCOORD, float4 col : COLOR) 
{
    GPUInstance instance = Instances[InstanceIndex];
    Output result;
    
    float4 worldPos = mul(float4(pos, 1.0f), instance.World);
    result.worldPos = worldPos.xyz;
    result.svpos    = mul(worldPos, viewProj);
    
    result.normal = mul(float4(normal, 0.f), instance.Normal).xyz;
    float3 tangentWs = normalize(mul(tangent.xyz, (float3x3) instance.World));
    
    result.tangent = float4(tangentWs, tangent.w);
    
    result.uv = uv;
    
    result.col = col;
    
	return result;
}