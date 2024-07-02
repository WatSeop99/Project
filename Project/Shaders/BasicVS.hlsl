#include "Common.hlsli"

Texture2D g_HeightTex : register(t6);

PixelShaderInput main(VertexShaderInput input)
{
    // 뷰 좌표계는 NDC이기 때문에 월드 좌표를 이용해서 조명 계산.
    PixelShaderInput output;
    
#ifdef SKINNED
    float weights[8] = 
    {
        input.BoneWeights0.x,
        input.BoneWeights0.y,
        input.BoneWeights0.z,
        input.BoneWeights0.w,
        input.BoneWeights1.x,
        input.BoneWeights1.y,
        input.BoneWeights1.z,
        input.BoneWeights1.w,
    };
    uint indices[8] = 
    {
        input.BoneIndices0.x,
        input.BoneIndices0.y,
        input.BoneIndices0.z,
        input.BoneIndices0.w,
        input.BoneIndices1.x,
        input.BoneIndices1.y,
        input.BoneIndices1.z,
        input.BoneIndices1.w,
    };

    float3 modelPos = float3(0.0f, 0.0f, 0.0f);
    float3 modelNormal = float3(0.0f, 0.0f, 0.0f);
    float3 modelTangent = float3(0.0f, 0.0f, 0.0f);
    
    // Uniform Scaling 가정.
    // (float3x3)boneTransforms 캐스팅으로 Translation 제외.
    for (int i = 0; i < 8; ++i)
    {
        // weight의 합은 1임.
        modelPos += weights[i] * mul(float4(input.ModelPosition, 1.0f), g_BoneTransforms[indices[i]]).xyz;
        modelNormal += weights[i] * mul(input.ModelNormal, (float3x3)g_BoneTransforms[indices[i]]);
        modelTangent += weights[i] * mul(input.ModelTangent, (float3x3)g_BoneTransforms[indices[i]]);
    }

    input.ModelPosition = modelPos;
    input.ModelNormal = modelNormal;
    input.ModelTangent = modelTangent;
#endif
    
    output.ModelPosition = input.ModelPosition;
    output.WorldNormal = mul(float4(input.ModelNormal, 0.0f), g_WorldInverseTranspose).xyz;
    output.WorldNormal = normalize(output.WorldNormal);
    output.WorldPosition = mul(float4(input.ModelPosition, 1.0f), g_World).xyz;
    
    if (bUseHeightMap)
    {
        float height = g_HeightTex.SampleLevel(g_LinearClampSampler, input.Texcoord, 0.0f).r;
        height = height * 2.0f - 1.0f;
        output.WorldPosition += output.WorldNormal * height * g_HeightScale;
    }
    
    output.ProjectedPosition = mul(float4(output.WorldPosition, 1.0f), g_ViewProjection);
    output.Texcoord = input.Texcoord;
    output.WorldTangent = mul(float4(input.ModelTangent, 0.0f), g_World).xyz;

    return output;
}
