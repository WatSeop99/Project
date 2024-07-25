#pragma once

#include <directxtk12/SimpleMath.h>
#include <minwindef.h>
// #include "../pch.h"

using DirectX::SimpleMath::Vector3;
using DirectX::SimpleMath::Matrix;

#define ALIGN(size) __declspec(align(size))

#define MAX_LIGHTS 3
#define LIGHT_OFF 0x00
#define LIGHT_DIRECTIONAL 0x01
#define LIGHT_POINT 0x02
#define LIGHT_SPOT 0x04
#define LIGHT_SHADOW 0x10

ALIGN(16) struct MeshConstant
{
	Matrix World = Matrix();
	Matrix WorldInverseTranspose = Matrix();
	Matrix WorldInverse = Matrix();
	BOOL bUseHeightMap = FALSE;
	float HeightScale = 0.0f;
	float WindTrunk = 0.0f;
	float WindLeaves = 0.0f;
};
ALIGN(16) struct MaterialConstant
{
	Vector3 AlbedoFactor = Vector3(1.0f);
	float RoughnessFactor = 1.0f;
	float MetallicFactor = 1.0f;
	Vector3 EmissionFactor = Vector3(0.0f);

	BOOL bUseAlbedoMap = FALSE;
	BOOL bUseNormalMap = FALSE;
	BOOL bUseAOMap = FALSE;
	BOOL bInvertNormalMapY = FALSE;
	BOOL bUseMetallicMap = FALSE;
	BOOL bUseRoughnessMap = FALSE;
	BOOL bUseEmissiveMap = FALSE;
	float dummy = 0.0f;
};
ALIGN(16) struct LightProperty
{
	Vector3 Radiance = Vector3(5.0f); // strength.
	float FallOffStart = 0.0f;
	Vector3 Direction = Vector3(0.0f, 0.0f, 1.0f);
	float FallOffEnd = 20.0f;
	Vector3 Position = Vector3(0.0f, 0.0f, -2.0f);
	float SpotPower = 6.0f;

	UINT LightType = LIGHT_OFF;
	float Radius = 0.035f; // 반지름.

	float HaloRadius = 0.0f;
	float HaloStrength = 0.0f;

	Matrix ViewProjections[6]; // spot은 1개만. point는 전부 사용. directional은 4개 사용.
	Matrix Projections[4];
	Matrix InverseProjections[4];
};
ALIGN(16) struct LightConstant
{
	LightProperty Lights[MAX_LIGHTS];
};
ALIGN(16) struct ShadowConstant
{
	Matrix ViewProjects[6];
};
ALIGN(16) struct GlobalConstant
{
	Matrix View = Matrix();
	Matrix Projection = Matrix();
	Matrix InverseProjection = Matrix();
	Matrix ViewProjection = Matrix();
	Matrix InverseViewProjection = Matrix(); // Proj -> World
	Matrix InverseView = Matrix();

	Vector3 EyeWorld = Vector3(0.0f);
	float StrengthIBL = 0.0f;

	int TextureToDraw = 0;	 // 0: Env, 1: Specular, 2: Irradiance, 그외: 검은색.
	float EnvLODBias = 0.0f; // 환경맵 LodBias.
	float LODBias = 2.0f;    // 다른 물체들 LodBias.
	float GlobalTime = 0.0f;

	int dummy[4] = { 0, };
};
ALIGN(16) struct ImageFilterConstant
{
	float DX;
	float DY;
	float Threshold;
	float Strength;
	float Option1; // exposure in CombinePS.hlsl
	float Option2; // gamma in CombinePS.hlsl
	float Option3; // blur in CombinePS.hlsl
	float Option4;
};
