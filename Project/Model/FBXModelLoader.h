#pragma once

#include <string>
#include <vector>


//class FbxScene;
//class FbxNode;
//class FbxMesh;
class AnimationData;
struct MeshInfo;

class FBXModelLoader
{
public:
	FBXModelLoader() = default;
	~FBXModelLoader() = default;

	HRESULT Load(std::wstring& basePath, std::wstring& fileName, bool _bRevertNormal);
	HRESULT LoadAnimation(std::wstring& basePath, std::wstring& fileName);

protected:
	void findDeformingBones(const FbxNode* pNODE);

	void updateTangents();
	void updateBoneIDs(const FbxNode* pNODE, int* pCounter);

	void processNode(FbxNode* pNode, const FbxScene* pSCENE);
	void processMesh(FbxMesh* pMesh, const FbxScene* pSCENE, MeshInfo* pMeshInfo);

	void calculateTangentBitangent(const Vertex& V1, const Vertex& V2, const Vertex& V3, DirectX::XMFLOAT3* pTangent, DirectX::XMFLOAT3* pBitangent);

public:
	std::string szBasePath;
	std::vector<MeshInfo> MeshInfos;

	AnimationData AnimData;

	bool bRevertNormal = false;
};
