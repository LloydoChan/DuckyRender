struct CullDraw
{
    float3 Center;
    float3 Extents;
    float4 Orientation;

    uint DrawIndex;
    uint IndexCount;
    uint StartIndex;
    int BaseVertex;
};

struct IndirectCommand
{
     uint DrawIndex;
     uint IndexCountPerInstance;
     uint InstanceCount; 
     uint StartIndexLocation;
     int BaseVertexLocation; 
     uint StartInstanceLocation;
};

cbuffer CullConstants : register(b0)
{
    float4 FrustumPlanes[6];
    
    uint InputOffset;
    uint OutputOffset;
    uint DrawCount;
    uint CounterIndex;
};

StructuredBuffer<CullDraw> CullDraws : register(t0);
RWStructuredBuffer<IndirectCommand> Commands : register(u0);
RWStructuredBuffer<uint> DrawCounts : register(u1);

float3 RotateVectorByQuaternion(float3 v, float4 q)
{
    return v + 2.0f * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}

bool TestFrustum(float3 Center, float3 Extents, float4 Orientation)
{
    float3 axisX = RotateVectorByQuaternion(float3(1, 0, 0), Orientation);
    float3 axisY = RotateVectorByQuaternion(float3(0, 1, 0), Orientation);
    float3 axisZ = RotateVectorByQuaternion(float3(0, 0, 1), Orientation);

    bool visible = true;

    [unroll]
    for (uint i = 0; i < 6; ++i)
    {
        float3 normal = FrustumPlanes[i].xyz;

        float distance = dot(Center, normal) + FrustumPlanes[i].w;

        float radius =
            Extents.x * abs(dot(axisX, normal)) +
            Extents.y * abs(dot(axisY, normal)) +
            Extents.z * abs(dot(axisZ, normal));

        if (distance + radius < 0.0f)
        {
            visible = false;
            break;
        }
    }

    return visible;
}

[numthreads(64, 1, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
    uint localIndex = threadID.x;

    if (localIndex >= DrawCount)
        return;

    uint drawIndex = InputOffset + localIndex;
    CullDraw draw = CullDraws[drawIndex];
    if (!TestFrustum(draw.Center, draw.Extents, draw.Orientation)) return;
    
    uint compactIdx;
    InterlockedAdd(DrawCounts[CounterIndex], 1, compactIdx);
    
    uint outputIdx = OutputOffset + compactIdx;

    IndirectCommand command;

    command.DrawIndex = draw.DrawIndex;
    command.IndexCountPerInstance = draw.IndexCount;
    command.InstanceCount = 1;
    command.StartIndexLocation = draw.StartIndex;
    command.BaseVertexLocation = draw.BaseVertex;
    command.StartInstanceLocation = 0;

    Commands[outputIdx] = command;
}