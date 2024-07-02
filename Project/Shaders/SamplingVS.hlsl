#include "Common.hlsli"

struct SamplingVertexShaderInput
{
    float3 ModelPosition : POSITION;
    float2 Texcoord : TEXCOORD;
};

struct SamplingPixelShaderInput
{
    float4 ScreenPosition : SV_POSITION;
    float2 Texcoord : TEXCOORD;
};

SamplingPixelShaderInput main(SamplingVertexShaderInput input)
{
    SamplingPixelShaderInput output;
    
    output.ScreenPosition = float4(input.ModelPosition, 1.0f);
    output.Texcoord = input.Texcoord;

    return output;
}
