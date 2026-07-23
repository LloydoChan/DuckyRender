struct Output
{
    float4 svpos : SV_Position;
    float2 uv : TEXCOORD;
};

cbuffer cbuff0 : register(b0)
{
    matrix mat;
};

Output main( float4 pos : POSITION, float2 uv : TEXCOORD) 
{
    Output result;
    result.svpos = mul(pos, mat);
    result.uv = uv;
	return result;
}