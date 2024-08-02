#pragma once

#include <directxtk12/SimpleMath.h>

using DirectX::SimpleMath::Vector2;
using DirectX::SimpleMath::Vector3;
using DirectX::SimpleMath::Vector4;
using DirectX::SimpleMath::Matrix;

struct Vertex
{
	Vector3 Position;
	Vector3 Normal;
	Vector2 Texcoord;
	Vector3 Tangent; // BiTangent는 GPU에서 계산.
};
struct SkinnedVertex
{
	Vector3 Position;
	Vector3 Normal;
	Vector2 Texcoord;
	Vector3 Tangent;

	float BlendWeights[8] = { 0.0f, };  // BLENDWEIGHT0 and 1
	UCHAR BoneIndices[8] = { 0, };		// BLENDINDICES0 and 1
};
