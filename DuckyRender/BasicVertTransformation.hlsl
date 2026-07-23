struct Output
{
    float4 svpos : SV_Position;
    float2 uv : TEXCOORD;
};

Output main( float4 pos : POSITION, float2 uv : TEXCOORD) 
{
    Output result;
    result.svpos = pos;
    result.uv = uv;
	return result;
}