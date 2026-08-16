struct Input
{
    float4 svpos    : SV_Position;
    float3 worldPos : POSITIONT_WS;
    float3 normal   : NORMAL_WS;
    float4 tangent  : TANGENT_WS;
    float2 uv       : TEXCOORD;
    float4 col      : COLOR;
};

static const uint DEPTH = 1;
static const uint ROUGHNESS = 2;
static const uint METAL = 3;
static const uint NORMAL = 4;
static const uint UV = 5;
static const uint DIFFUSE = 6;

static const uint ALPHA_OPAQUE = 0;
static const uint ALPHA_MASK = 1;
static const uint ALPHA_BLEND = 2;

struct GPUDrawData
{
    uint InstanceIndex;
    uint MaterialIndex;
};

cbuffer PerFrameConstants : register(b0)
{
    matrix viewProj;

    float4 cameraPosition;
    float4 lightDirection;
    float4 lightColor;

    uint visualisationMode;

    uint InstanceBufferIndex;
    uint MaterialBufferIndex;
    uint DrawBufferIndex;
};

cbuffer DrawConstants : register(b2)
{
    uint DrawIndex;
};

struct InstanceMaterial
{
    float4 BaseColorFactor;
    float3 EmissiveColorFactor;

    float NormalScale;
    float RoughnessFactor;
    float MetallicFactor;

    uint alphaMode;
    float alphaCutoff;
    uint doubleSided;

    uint BaseColorTexture;
    uint NormalTexture;
    uint MetallicRoughnessTexture;
    uint EmissiveTexture;
};

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

float4 main(Input input, 
            uint primitiveID : SV_PrimitiveID,
            bool isFrontFace : SV_IsFrontFace) : SV_TARGET
{
    
    StructuredBuffer<GPUDrawData> draws = ResourceDescriptorHeap[DrawBufferIndex];

    GPUDrawData draw = draws[DrawIndex];
    
    StructuredBuffer<InstanceMaterial> materials = ResourceDescriptorHeap[MaterialBufferIndex];

    InstanceMaterial mat = materials[draw.MaterialIndex];
    
    float4 baseColor = mat.BaseColorFactor * float4(input.col.rgb, 1.f);
    
    Texture2D<float4> base = ResourceDescriptorHeap[mat.BaseColorTexture];
    float4 baseColorSample = base.Sample(smp, input.uv);
    
    baseColor *= baseColorSample;
    
    if (mat.alphaMode == ALPHA_MASK) clip(baseColor.a - mat.alphaCutoff);
   
    float3 V = normalize(cameraPosition.xyz - input.worldPos);
    // create tangent space
    

    float3 N = normalize(input.normal);
    float3 T = normalize(input.tangent.xyz);
    
    // make tangent orthogonal to N
    T = normalize(T - N * dot(N, T));
    
    float3 B = cross(N, T) * input.tangent.w;
    
    float faceSign = mat.doubleSided != 0 && !isFrontFace ? -1.0f : 1.0f;
    
    T *= faceSign;
    N *= faceSign;
    B *= faceSign;
    
    float3 L = normalize(-lightDirection.xyz);
    
    Texture2D<float3> normalMap = ResourceDescriptorHeap[mat.NormalTexture];
    
    float3 NormalMapSample = normalMap.Sample(smp, input.uv).rgb  * 2.f - 1.f;
    NormalMapSample.xy *= mat.NormalScale;
    NormalMapSample = normalize(NormalMapSample);
    float3x3 TangentToWorld = float3x3(T, B, N);
    N = normalize(mul(NormalMapSample, TangentToWorld)).rgb;
    N = normalize(N);
   
     // roughness
    Texture2D<float4> metallicTex = ResourceDescriptorHeap[mat.MetallicRoughnessTexture];
    float4 metallicRoughnessSample = metallicTex.Sample(smp, input.uv);
    float roughness = saturate(mat.RoughnessFactor * metallicRoughnessSample.g);
    float metallic = saturate(mat.MetallicFactor * metallicRoughnessSample.b);
    
     float3 emissive = float3(0.f, 0.f, 0.f);

     Texture2D<float3> emissiveTex = ResourceDescriptorHeap[mat.EmissiveTexture];
     emissive += emissiveTex.Sample(smp, input.uv).rgb * mat.EmissiveColorFactor;
    
     roughness = max(roughness, 0.045f);
    
     float3 H = normalize(V + L);
     float NDotL = saturate(dot(N, L));
     float NdotV = saturate(dot(N, V));
     float VdotH = saturate(dot(V, H));

     float3 F0 = lerp(0.04f, baseColor.rgb, metallic);
     float3 F  = FresnelSchlick(VdotH, F0);
     float D   = DistributionGGX(N, H, roughness);
     float G   = GeometrySmith(N, V, L, roughness);

     float3 specular = D * G * F / max(4.0f * NdotV * NDotL, 0.0001f);
     float3 kS = F;
     float3 kD = (1.0f - kS) * (1.0f - metallic);

    float3 diffuse = kD * baseColor.rgb / PI * NDotL;

     float3 radiance = lightColor;

    float3 color = (diffuse + specular) * radiance + emissive * 0.1f;
    
    // hack ambient term
    float3 ambient = baseColor.rgb * 0.03f * (1.0f - metallic);
    color += ambient;
  
    if (visualisationMode == DEPTH)
        return float4(input.svpos.z.xxx, 1.f);
    
    if (visualisationMode == ROUGHNESS)
        return float4(roughness.xxx, 1.f);
    
    if (visualisationMode == NORMAL)
        return float4(N * 0.5f + 0.5f, 1.f);
    
    if (visualisationMode == METAL)
        return float4(metallic.xxx, 1.f);
    
    if (visualisationMode == UV)
        return float4(input.uv.xy, 0.f, 1.f);
    
    if (visualisationMode == DIFFUSE)
        return float4(baseColor.rgb, 1.f);
    
    return float4(color, baseColor.a);
}