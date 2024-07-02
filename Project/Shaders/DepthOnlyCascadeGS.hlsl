cbuffer ShadowConstants : register(b4)
{
    matrix LightViewProj[6];
};

struct PixelShaderInput
{
    float4 ProjectedPosition : SV_POSITION;
    uint RTIndex : SV_RenderTargetArrayIndex;
};

[maxvertexcount(12)]
void main(triangle float4 InPos[3] : SV_POSITION, inout TriangleStream<PixelShaderInput> OutStream)
{
    for (int cascadeIndex = 0; cascadeIndex < 4; ++cascadeIndex)
    {
        PixelShaderInput output;
        output.RTIndex = cascadeIndex;

        for (int v = 0; v < 3; ++v)
        {
            output.ProjectedPosition = mul(InPos[v], LightViewProj[cascadeIndex]);
            OutStream.Append(output);
        }

        OutStream.RestartStrip();
    }
}
