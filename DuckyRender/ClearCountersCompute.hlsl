RWStructuredBuffer<uint> DrawCounts : register(u0);

[numthreads(8, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (id.x < 6)
        DrawCounts[id.x] = 0;
}