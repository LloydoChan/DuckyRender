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

static const uint ALPHA_OPAQUE = 0;
static const uint ALPHA_MASK = 1;
static const uint ALPHA_BLEND = 2;

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
    
    uint  alphaMode;
    float alphaCutoff;
    uint  doubleSided;
    
    uint padding1;
    uint padding2;
    
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

float3 SRGBToLinear(float3 c)
{
    float3 low = c / 12.92f;
    float3 high = pow((c + 0.055f) / 1.055f, 2.4f);

    return select(high, low, c <= 0.04045f);
}

float3 LinearToSRGB(float3 c)
{
    c = saturate(c);

    float3 low = c * 12.92f;
    float3 high = 1.055f * pow(c, 1.0f / 2.4f) - 0.055f;

    return select(high, low, c <= 0.0031308f);
}

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
    float4 baseColorSample = BaseColorFactor * float4(input.col.rgb, 1.f);
    
   // if (HasBaseColorTexture == 1)
    //{
       baseColorSample *= tex.Sample(smp, input.uv);
   // }
    
    if (alphaMode == ALPHA_MASK) clip(baseColorSample.a - alphaCutoff);
   
    float3 baseColor = baseColorSample.rgb;
    float3 V = normalize(cameraPosition.xyz - input.worldPos);
    // create tangent space
    
    float faceSign = doubleSided != 0 && !isFrontFace ? -1.0f : 1.0f;

    float3 N = normalize(input.normal) * faceSign;
    float3 T = normalize(input.tangent.xyz) * faceSign;
    float3 B = cross(N, T) * input.tangent.w;
    
     float3 L = normalize(-lightDirection.xyz);
    
     if (HasNormalTexture == 1)
     {
         float3 NormalMapSample = NormalMapTexture.Sample(smp, input.uv).rgb * 2.f - 1.f;
         NormalMapSample.xy *= NormalScale;
         NormalMapSample = normalize(NormalMapSample);
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
     float NdotV = saturate(dot(N, V));
     float VdotH = saturate(dot(V, H));

     float3 F0 = lerp(0.04f, baseColor, metallic);
     float3 F = FresnelSchlick(VdotH, F0);
     float D = DistributionGGX(N, H, roughness);
     float G = GeometrySmith(N, V, L, roughness);

     float3 specular = D * G * F / max(4.0f * NdotV * NDotL, 0.0001f);
     float3 kS = F;
     float3 kD = (1.0f - kS) * (1.0f - metallic);

     float3 diffuse = kD * baseColor / PI;

     float3 radiance = lightColor;

     float3 color = (diffuse + specular) * NDotL * radiance + emissive;
    
    // hack ambient term
    float3 ambient = baseColor * 0.03f * (1.0f - metallic);
    color += ambient;
  
    if (visualisationMode == DEPTH)
        return float4((input.svpos.z / input.svpos.w).xxx, 1.f);
    
    if (visualisationMode == ROUGHNESS)
        return float4(roughness.xxx, 1.f);
    
    if (visualisationMode == NORMAL)
        return float4(N * 0.5f + 0.5f, 1.f);
    
    if (visualisationMode == METAL)
        return float4(metallic.xxx, 1.f);
    
    return float4(color, baseColorSample.a);
}