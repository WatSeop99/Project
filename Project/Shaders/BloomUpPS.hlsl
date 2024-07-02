Texture2D g_Texture : register(t0);
SamplerState g_Sampler : register(s1);

cbuffer SamplingPixelConstantData : register(b4)
{
    float g_DX;
    float g_DY;
    float g_Threshold;
    float g_Strength;
    float4 g_Options;
};

struct SamplingPixelShaderInput
{
    float4 ProjectedPosition : SV_POSITION;
    float2 Texcoord : TEXCOORD;
};

float4 main(SamplingPixelShaderInput input) : SV_TARGET
{
    float x = input.Texcoord.x;
    float y = input.Texcoord.y;
    
    float3 a = g_Texture.Sample(g_Sampler, float2(x - g_DX, y + g_DY)).rgb;
    float3 b = g_Texture.Sample(g_Sampler, float2(x, y + g_DY)).rgb;
    float3 c = g_Texture.Sample(g_Sampler, float2(x + g_DX, y + g_DY)).rgb;

    float3 d = g_Texture.Sample(g_Sampler, float2(x - g_DX, y)).rgb;
    float3 e = g_Texture.Sample(g_Sampler, float2(x, y)).rgb;
    float3 f = g_Texture.Sample(g_Sampler, float2(x + g_DX, y)).rgb;

    float3 g = g_Texture.Sample(g_Sampler, float2(x - g_DX, y - g_DY)).rgb;
    float3 h = g_Texture.Sample(g_Sampler, float2(x, y - g_DY)).rgb;
    float3 i = g_Texture.Sample(g_Sampler, float2(x + g_DX, y - g_DY)).rgb;

    float3 color = e * 4.0f;
    color += (b + d + f + h) * 2.0f;
    color += (a + c + g + i);
    color *= 1.0 / 16.0f;
  
    return float4(color, 1.0f);
}
