#pragma once

#include "AnimationData.h"
#include "MeshInfo.h"

HRESULT ReadFromFile(std::vector<MeshInfo>& dst, std::wstring& basePath, std::wstring& fileName, bool bRevertNormals = false);
HRESULT ReadAnimationFromFile(std::vector<MeshInfo>& meshInfos, AnimationData& animData, std::wstring& basePath, std::wstring& fileName, bool bRevertNormals = false);

void Normalize(const Vector3& CENTER, const float LONGEST_LENGTH, std::vector<MeshInfo>& meshes, AnimationData& animData);

void MakeSquare(MeshInfo* pDst, const float SCALE = 1.0f, const Vector2 TEX_SCALE = Vector2(1.0f));
void MakeSquareGrid(MeshInfo* pDst, const int NUM_SLICES, const int NUM_STACKS, const float SCALE = 1.0f, const Vector2 TEX_SCALE = Vector2(1.0f));
void MakeGrass(MeshInfo* pDst);
void MakeBox(MeshInfo* pDst, const float SCALE = 1.0f);
void MakeWireBox(MeshInfo* pDst, const Vector3& CENTER, const Vector3& EXTENTS);
void MakeWireSphere(MeshInfo* pDst, const Vector3& CENTER, const float RADIUS);
void MakeWireCapsule(MeshInfo* pDst, const Vector3& CENTER, const float RADIUS, const float TOTAL_LENGTH);
void MakeCylinder(MeshInfo* pDst, const float BOTTOM_RADIUS, const float TOP_RADIUS, float height, int numSlices);
void MakeSphere(MeshInfo* pDst, const float RADIUS, const int NUM_SLICES, const int NUM_STACKS, const Vector2 TEX_SCALE = Vector2(1.0f));
void MakeTetrahedron(MeshInfo* pDst);
void MakeIcosahedron(MeshInfo* pDst);

void MakeSlope(MeshInfo* pDst, const float ANGLE, const float LENGTH);

void SubdivideToSphere(MeshInfo* pDst, const float RADIUS, MeshInfo& meshData);
