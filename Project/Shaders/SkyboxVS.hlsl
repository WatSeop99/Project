#include "Common.hlsli"

struct SkyboxPixelShaderInput
{
    float4 ProjectedPosition : SV_POSITION;
    float3 ModelPosition : POSITION;
};

SkyboxPixelShaderInput main(VertexShaderInput input)
{
    SkyboxPixelShaderInput output;
    output.ModelPosition = input.ModelPosition;
    output.ProjectedPosition = mul(float4(input.ModelPosition, 0.0f), g_View); // È¸Àü¸¸
    output.ProjectedPosition = mul(float4(output.ProjectedPosition.xyz, 1.0f), g_Projection);

    return output;
}
