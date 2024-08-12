#pragma once

#include <string>
#include <vector>

class AnimationData;
struct MeshInfo;

class FBXModelLoader
{
public:
	enum eTextureType
	{
		TextureType_Albedo = 0,
		TextureType_Emissive,
		TextureType_Specular,
		TextureType_Ambient,
		TextureType_Normal,
		TextureType_TotalCount,
	};

public:
	FBXModelLoader() = default;
	~FBXModelLoader() = default;

	HRESULT Load(std::wstring& basePath, std::wstring& fileName, bool _bRevertNormal);
	HRESULT LoadAnimation(std::wstring& basePath, std::wstring& fileName);

protected:
	void findDeformingBones(const FbxNode* pNODE);

	void updateTangents();
	void updateBoneIDs(const FbxNode* pNODE, int* pCounter);

	Vector3 readNormal(FbxMesh* pMesh, int vertexIndex, int vertexCount);
	Vector2 readTexcoord(FbxMesh* pMesh, int vertexIndex, int UVIndex);
	void readMaterial(FbxProperty& materialProperty, MeshInfo* pMeshInfo, eTextureType type);
	void readAnimationData(FbxAnimStack* pAnimStack, FbxNode* pNode, const FbxScene* pSCENE, int animStackIndex);
	void readBoneOffset(FbxCluster* pCluster, const FbxAMatrix& NODE_TRANSFORM, const int BONE_ID);
	void readKeyframe(const int ANIM_INDEX, FbxNode* pNode, FbxCluster* pCluster, FbxScene* pScene, const FbxAMatrix& NODE_TRANSFORM, const int BONE_ID);

	void processNode(FbxNode* pNode, FbxScene* pScene);
	void processMesh(FbxMesh* pMesh, FbxScene* pScene, MeshInfo* pMeshInfo);

	void calculateTangentBitangent(const Vertex& V1, const Vertex& V2, const Vertex& V3, DirectX::XMFLOAT3* pTangent, DirectX::XMFLOAT3* pBitangent);

	FbxAMatrix getTransform(FbxNode* pNode);

public:
	std::string szBasePath;
	std::vector<MeshInfo> MeshInfos;
	FbxArray<FbxString*> AnimNames;

	AnimationData AnimData;

	bool bRevertNormal = false;
};
