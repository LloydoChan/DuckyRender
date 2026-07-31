struct Input
{
    float4 svpos    : SV_Position;
    float3 worldPos : POSITIONT_WS;
    float3 normal   : NORMAL_WS;
    float4 tangent  : TANGENT_WS;
    float2 uv       : TEXCOORD;
    float4 col : COLOR;
};

static const uint DEPTH = 1;
static const uint ROUGHNESS = 2;
static const uint METAL = 3;
static const uint NORMAL = 4;

cbuffer PerFrameConstants : register(b0)
{
    matrix viewProj;
    
    float4 cameraPosition;
    float4 lightDirection;
    float4 lightColor;
    
    uint visualisationMode;
};

cbuffer InstanceMaterial : register(b2)
{
    float4 BaseColorFactor;

    float RoughnessFactor;
    float MetallicFactor;
    float NormalScale;
    
    uint  HasBaseColorTexture;
    uint  HasNormalTexture;
    uint  HasMetallicRoughnessTexture;
    uint  HasEmissiveTexture;
};

Texture2D<float4> tex : register(t0);
Texture2D<float4> NormalMapTexture : register(t1);
Texture2D<float4> MetallicRoughnessTexture : register(t2);
Texture2D<float4> EmissiveTexture : register(t3);

SamplerState smp : register(s0);

static const float PI = 3.14159265359f;

float3 PrimitiveIDToColour(uint primitiveID)
{
    uint hash = primitiveID;

    hash ^= hash >> 16;
    hash *= 0x7feb352d;
    hash ^= hash >> 15;
    hash *= 0x846ca68b;
    hash ^= hash >> 16;

    float red = float((hash >> 0) & 255) / 255.0f;
    float green = float((hash >> 8) & 255) / 255.0f;
    float blue = float((hash >> 16) & 255) / 255.0f;

    return float3(red, green, blue);
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0f - F0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

float DistributionGGX(float3 N, float3 H, float roughness)
{
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float NdotH = saturate(dot(N, H));

    float NdotH2 = NdotH * NdotH;

    float denominator = NdotH2 * (alpha2 - 1.0f) + 1.0f;

    denominator = PI * denominator *denominator;

    return alpha2 / max(denominator, 0.000001f);
}

float GeometrySchlickGGX(float NdotX, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;

    return NdotX / max(NdotX * (1.0f - k) + k, 0.000001f);
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = saturate(dot(N, V));
    float NdotL = saturate(dot(N, L));

    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

float4 main(Input input, uint primitiveID : SV_PrimitiveID) : SV_TARGET
{
    float3 V = normalize(cameraPosition.xyz - input.worldPos);
    // create tangent space
    float3 N = normalize(input.normal);
    float3 T = normalize(input.tangent.xyz);
    
    T = normalize(T - N * dot(N, T));
    
    float3 B = cross(N, T) * input.tangent.w;
    
    float3 L = normalize(-lightDirection.xyz);
    
    if (HasNormalTexture == 1)
    {
        float3 NormalMapSample = NormalMapTexture.Sample(smp, input.uv).rgb * 2.f - 1.f;
        float3x3 TangentToWorld = float3x3(T, B, N);
        N = normalize(mul(NormalMapSample, TangentToWorld)).rgb;
    }
    
    float roughness = RoughnessFactor;
    float metallic = MetallicFactor;
    
    if (HasMetallicRoughnessTexture == 1)
    {
         // roughness
        float4 metallicRoughnessSample = MetallicRoughnessTexture.Sample(smp, input.uv);
        roughness = saturate(RoughnessFactor * metallicRoughnessSample.g);
        metallic = saturate(MetallicFactor * metallicRoughnessSample.b);
    }
    
    float3 emissive = float3(0.f, 0.f, 0.f);

    if (HasEmissiveTexture == 1)
    {
        emissive = EmissiveTexture.Sample(smp, input.uv).rgb; 
    }
    
    roughness = max(roughness, 0.045f);
    
    float3 H = normalize(V + L);
    float NDotL = saturate(dot(N, L));
    float4 baseColorSample = BaseColorFactor * input.col;
    
    if (HasBaseColorTexture == 1)
    {
        baseColorSample = BaseColorFactor * tex.Sample(smp, input.uv);
    }
    float3 baseColor = baseColorSample.rgb;
    
    float3 litColor = baseColor * lightColor.rgb * NDotL;
    
    float NdotV = saturate(dot(N, V));

    float VdotH = saturate(dot(V, H));

    float3 F0 = lerp(0.04f, baseColor, metallic);
    float3 F  = FresnelSchlick(VdotH, F0);
    float  D  = DistributionGGX(N, H, roughness);
    float  G  = GeometrySmith(N, V, L,roughness);

    float3 specular = D * G * F / max(4.0f * NdotV * NDotL, 0.0001f);
    float3 kS = F;
    float3 kD = (1.0f - kS) * (1.0f - metallic);

    float3 diffuse = kD * baseColor / PI;

    float3 radiance = lightColor;

    float3 color = (diffuse + specular) * radiance * NDotL + emissive;
    
    // hack ambient term
    //color += baseColor * float3(0.05f, 0.05f, 0.05f);
   
    // gamma
    color = color / (color + 1.0f);
    color = pow(saturate(color), 1.0f / 2.2f);

    float depthVal = input.svpos.z;
 
   
    if (visualisationMode == DEPTH)
        return float4(input.svpos.z.xxx, 1.f);
    
    if (visualisationMode == ROUGHNESS)
        return float4(roughness.xxx, 1.f);
    
    if (visualisationMode == NORMAL)
        return float4(N, 1.f);
    
    if (visualisationMode == METAL)
        return float4(metallic.xxx, 1.f);
    
   // float3 hashColor = PrimitiveIDToColour(primitiveID);
    
    
    return float4(color, baseColorSample.a);
}