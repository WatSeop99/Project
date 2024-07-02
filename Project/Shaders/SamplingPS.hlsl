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
    float4 ScreenPosition : SV_POSITION;
    float2 Texcoord : TEXCOORD;
};

float4 main(SamplingPixelShaderInput input) : SV_TARGET
{
    float3 color = clamp(g_Texture.Sample(g_Sampler, input.Texcoord).rgb, 0.0f, 1.0f);
    //float l = (color.x + color.y + color.y) / 3;
    
    //if (l > g_Threshold)
    //{
    //    return float4(color, 1.0f);
    //}
    //else
    //{
    //    return float4(0.0f, 0.0f, 0.0f, 0.0f);
    //}
    
    return float4(color, 1.0f);
}