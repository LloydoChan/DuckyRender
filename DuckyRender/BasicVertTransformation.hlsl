struct Output
{
    float4 svpos : SV_Position;
    float3 worldPos : POSITIONT_WS;
    float3 normal : NORMAL_WS;
    float4 tangent : TANGENT_WS;
    float2 uv : TEXCOORD;
};

cbuffer cbuff0 : register(b0)
{
    matrix viewProj;
    
    float4 cameraPosition;
    float4 lightDirection;
    float4 lightColor;
};

cbuffer cbuff1 : register(b1)
{
    matrix instanceTransform;
};

Output main( float4 pos : POSITION, float2 uv : TEXCOORD, float3 normal : NORMAL, float4 tangent : TANGENT) 
{
    Output result;
    
    float4 worldPos = mul(pos, instanceTransform);
    result.worldPos = worldPos.xyz;
    result.svpos    = mul(worldPos, viewProj);
    
    result.normal =    normalize(mul(normal, (float3x3)instanceTransform));
    float3 tangentWs = normalize(mul(tangent.xyz, (float3x3)instanceTransform));
    
    result.tangent = float4(tangentWs, tangent.w);
    
    result.uv = uv;
    
	return result;
}