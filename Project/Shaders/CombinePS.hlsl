Texture2D g_Texture0 : register(t0);
Texture2D g_Texture1 : register(t1);
Texture2D g_PrevFrame : register(t2);
SamplerState g_Sampler : register(s1);

cbuffer ImageFilterConstData : register(b4)
{
    float g_DX;
    float g_DY;
    float g_Threshold;
    float g_Strength;
    float g_Exposure; // option1 in c++
    float g_Gamma;    // option2 in c++
    float g_Blur;     // option3 in c++
    float g_Option4;
};

struct SamplingPixelShaderInput
{
    float4 ProjectedPosition : SV_POSITION;
    float2 Texcoord : TEXCOORD;
};

float3 FilmicToneMapping(float3 color)
{
    color = max(float3(0.0f, 0.0f, 0.0f), color);
    color = (color * (6.2f * color + 0.5f)) / (color * (6.2f * color + 1.7f) + 0.06f);
    return color;
}

float3 LinearToneMapping(float3 color)
{
    float3 invGamma = float3(1.0f, 1.0f, 1.0f) / g_Gamma;

    color = clamp(g_Exposure * color, 0.0f, 1.0f);
    color = pow(color, invGamma);
    return color;
}

float3 Uncharted2ToneMapping(float3 color)
{
    float A = 0.15f;
    float B = 0.50f;
    float C = 0.10f;
    float D = 0.20f;
    float E = 0.02f;
    float F = 0.30f;
    float W = 11.2f;
    
    color *= g_Exposure;
    color = ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E / F;
    float white = ((W * (A * W + C * B) + D * E) / (W * (A * W + B) + D * F)) - E / F;
    color /= white;
    color = pow(color, float3(1.0f, 1.0f, 1.0f) / g_Gamma);
    return color;
}

float3 LumaBasedReinhardToneMapping(float3 color)
{
    float3 invGamma = float3(1.0f, 1.0f, 1.0f) / g_Gamma;
    float luma = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
    float toneMappedLuma = luma / (1.0f + luma);
    color *= toneMappedLuma / luma;
    color = pow(color, invGamma);
    return color;
}

float4 main(SamplingPixelShaderInput input) : SV_TARGET
{
    float3 color0 = g_Texture0.Sample(g_Sampler, input.Texcoord).rgb;
    float3 color1 = g_Texture1.Sample(g_Sampler, input.Texcoord).rgb;
    float3 combined = (1.0f - g_Strength) * color0 + g_Strength * color1;

    // Tone Mapping.
    combined = LinearToneMapping(combined);
    combined = lerp(combined, g_PrevFrame.Sample(g_Sampler, input.Texcoord).rgb, g_Blur);
    
    return float4(combined, 1.0f);
}
