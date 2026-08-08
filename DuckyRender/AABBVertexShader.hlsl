struct GPUOBB
{
    float3 Center;
    float3 Extents;

    float4 Orientation; // quaternion
};

cbuffer cbuff0 : register(b0)
{
    matrix viewProjection;
};

StructuredBuffer<GPUOBB> OBBs : register(t0);

float3 RotateByQuaternion(float3 v, float4 q)
{
    return v + 2.0f * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}

float4 main(
    float3 pos : POSITION,
    uint instanceID : SV_InstanceID
) : SV_POSITION
{
    GPUOBB box = OBBs[instanceID];

    float3 localPos = pos * box.Extents;

    float3 rotatedPos = RotateByQuaternion(localPos, box.Orientation);

    float3 worldPos = box.Center + rotatedPos;

    return mul( float4(worldPos, 1.0f), viewProjection);
}