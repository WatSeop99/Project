#include "Common.hlsli"

struct PixelShaderOutput
{
	float4 PixelColor : SV_TARGET0;
};

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	output.PixelColor.rgba = float4(1.0f, 0.0f, 0.0f, 1.0f);
    
	return output;
}