#include "Common.hlsli"

struct SkyboxPixelShaderInput
{
    float4 ProjectedPosition : SV_POSITION;
    float3 ModelPosition : POSITION;
};

struct PixelShaderOutput
{
    float4 PixelColor : SV_TARGET0;
};

PixelShaderOutput main(SkyboxPixelShaderInput input)
{
    PixelShaderOutput output;
    
    output.PixelColor = g_EnvIBLTex.SampleLevel(g_LinearWrapSampler, input.ModelPosition.xyz, g_EnvLodBias);
    
    /*if (textureToDraw == 0)
        output.pixelColor = envIBLTex.SampleLevel(linearWrapSampler, input.posModel.xyz, envLodBias);
    else if (textureToDraw == 1)
        output.pixelColor = specularIBLTex.SampleLevel(linearWrapSampler, input.posModel.xyz, envLodBias);
    else if (textureToDraw == 2)
        output.pixelColor = irradianceIBLTex.SampleLevel(linearWrapSampler, input.posModel.xyz, envLodBias);
    else
        output.pixelColor = float4(135/255, 206/255, 235/255, 1);*/

    output.PixelColor.rgb *= g_StrengthIBL;
    output.PixelColor.a = 1.0f;
    
    return output;
}
