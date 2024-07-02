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
    
    float3 a = g_Texture.Sample(g_Sampler, float2(x - 2.0f * g_DX, y + 2 * g_DY)).rgb;
    float3 b = g_Texture.Sample(g_Sampler, float2(x, y + 2.0f * g_DY)).rgb;
    float3 c = g_Texture.Sample(g_Sampler, float2(x + 2.0f * g_DX, y + 2.0f * g_DY)).rgb;

    float3 d = g_Texture.Sample(g_Sampler, float2(x - 2.0f * g_DX, y)).rgb;
    float3 e = g_Texture.Sample(g_Sampler, float2(x, y)).rgb;
    float3 f = g_Texture.Sample(g_Sampler, float2(x + 2.0f * g_DX, y)).rgb;

    float3 g = g_Texture.Sample(g_Sampler, float2(x - 2.0f * g_DX, y - 2.0f * g_DY)).rgb;
    float3 h = g_Texture.Sample(g_Sampler, float2(x, y - 2.0f * g_DY)).rgb;
    float3 i = g_Texture.Sample(g_Sampler, float2(x + 2.0f * g_DX, y - 2.0f * g_DY)).rgb;

    float3 j = g_Texture.Sample(g_Sampler, float2(x - g_DX, y + g_DY)).rgb;
    float3 k = g_Texture.Sample(g_Sampler, float2(x + g_DX, y + g_DY)).rgb;
    float3 l = g_Texture.Sample(g_Sampler, float2(x - g_DX, y - g_DY)).rgb;
    float3 m = g_Texture.Sample(g_Sampler, float2(x + g_DX, y - g_DY)).rgb;

    float3 color = e * 0.125f;
    color += (a + c + g + i) * 0.03125f;
    color += (b + d + f + h) * 0.0625f;
    color += (j + k + l + m) * 0.125f;
  
    return float4(color, 1.0f);
}