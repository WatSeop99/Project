cbuffer ShadowConstants : register(b4)
{
    matrix LightViewProj[6];
};

struct PixelShaderInput
{
    float4 ProjectedPosition : SV_POSITION;
    uint RTIndex : SV_RenderTargetArrayIndex;
};

[maxvertexcount(18)]
void main(triangle float4 InPos[3] : SV_POSITION, inout TriangleStream<PixelShaderInput> OutStream)
{
    for (int face = 0; face < 6; ++face)
    {
        PixelShaderInput output;
        output.RTIndex = face;

        for (int v = 0; v < 3; ++v)
        {
            output.ProjectedPosition = mul(InPos[v], LightViewProj[face]);
            OutStream.Append(output);
        }

        OutStream.RestartStrip();
    }
}
