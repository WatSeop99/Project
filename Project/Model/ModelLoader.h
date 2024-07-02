#pragma once

#include "AnimationData.h"
#include "MeshInfo.h"

struct aiNode;
struct aiScene;
struct aiMesh;
struct aiMaterial;
enum aiTextureType;

class ModelLoader
{
public:
	ModelLoader() = default;
	~ModelLoader() { MeshInfos.clear(); }

	HRESULT Load(std::wstring& basePath, std::wstring& fileName, bool _bRevertNormal);
	HRESULT LoadAnimation(std::wstring& basePath, std::wstring& fileName);

protected:
	void findDeformingBones(const aiScene* pSCENE);
	const aiNode* findParent(const aiNode* pNODE);

	void processNode(aiNode* pNode, const aiScene* pSCENE, Matrix& transform);
	void processMesh(aiMesh* pMesh, const aiScene* pSCENE, MeshInfo* pMeshInfo);

	void readAnimation(const aiScene* pSCENE);
	HRESULT readTextureFileName(const aiScene* pSCENE, aiMaterial* pMaterial, aiTextureType type, std::wstring* pDst);

	void updateTangents();
	void updateBoneIDs(aiNode* pNode, int* pCounter);

	void calculateTangentBitangent(const Vertex& V1, const Vertex& V2, const Vertex& V3, DirectX::XMFLOAT3* pTangent, DirectX::XMFLOAT3* pBitangent);

public:
	std::string szBasePath;
	std::vector<MeshInfo> MeshInfos;

	AnimationData AnimData;

	bool bIsGLTF = false; // gltf or fbx.
	bool bRevertNormal = false;
};
