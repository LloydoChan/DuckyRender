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

cbuffer cbuff1 : register(b1)
{
    matrix instanceTransform;
    matrix normalTransform;
};

Output main(float3 pos : POSITION, float3 normal : NORMAL, float4 tangent : TANGENT, float2 uv : TEXCOORD, float4 col : COLOR) 
{
    Output result;
    
    float4 worldPos = mul(float4(pos, 1.0f), instanceTransform);
    result.worldPos = worldPos.xyz;
    result.svpos    = mul(worldPos, viewProj);
    
    result.normal    = mul(float4(normal, 0.f), normalTransform).xyz;
    float3 tangentWs = normalize(mul(tangent.xyz, (float3x3) instanceTransform));
    
    result.tangent = float4(tangentWs, tangent.w);
    
    result.uv = uv;
    
    result.col = col;
    
	return result;
}