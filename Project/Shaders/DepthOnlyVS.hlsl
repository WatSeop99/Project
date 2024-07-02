#include "Common.hlsli"

float4 main(VertexShaderInput input) : SV_POSITION
{
#ifdef SKINNED
    float weights[8];
    weights[0] = input.BoneWeights0.x;
    weights[1] = input.BoneWeights0.y;
    weights[2] = input.BoneWeights0.z;
    weights[3] = input.BoneWeights0.w;
    weights[4] = input.BoneWeights1.x;
    weights[5] = input.BoneWeights1.y;
    weights[6] = input.BoneWeights1.z;
    weights[7] = input.BoneWeights1.w;

    uint indices[8];
    indices[0] = input.BoneIndices0.x;
    indices[1] = input.BoneIndices0.y;
    indices[2] = input.BoneIndices0.z;
    indices[3] = input.BoneIndices0.w;
    indices[4] = input.BoneIndices1.x;
    indices[5] = input.BoneIndices1.y;
    indices[6] = input.BoneIndices1.z;
    indices[7] = input.BoneIndices1.w;

    float3 modelPos = float3(0.0f, 0.0f, 0.0f);

    // Uniform Scaling 가정 (worldIT 불필요)
    // (float3x3) 캐스팅으로 Translation 제외
    for (int i = 0; i < 8; ++i)
    {
        // position만 계산.
        modelPos += weights[i] * mul(float4(input.ModelPosition, 1.0f), g_BoneTransforms[indices[i]]).xyz;
    }

    input.ModelPosition = modelPos;

#endif

    float4 pos = mul(float4(input.ModelPosition, 1.0f), g_World);
    return mul(pos, g_ViewProjection);
}
