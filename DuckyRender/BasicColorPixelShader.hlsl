struct Output
{
    float4 svpos : SV_Position;
    float2 uv : TEXCOORD;
};

float4 BasicPS(Output input) : SV_TARGET
{
	return float4(input.uv, 0.0f, 1.0f);
}