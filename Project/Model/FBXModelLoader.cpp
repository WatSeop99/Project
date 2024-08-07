// #define FBXSDK_SHARED 

#include "../pch.h"
#include "AnimationData.h"
#include "MeshInfo.h"
#include "../Util/Utility.h"
#include "FBXModelLoader.h"

HRESULT FBXModelLoader::Load(std::wstring& basePath, std::wstring& fileName, bool _bRevertNormal)
{
	HRESULT hr = S_OK;

	std::string fileNameA(fileName.begin(), fileName.end());
	szBasePath = std::string(basePath.begin(), basePath.end());

	FbxManager* pFbxSdkManager = FbxManager::Create();
	if (!pFbxSdkManager)
	{
		__debugbreak();
	}

	FbxIOSettings* pIOS = FbxIOSettings::Create(pFbxSdkManager, IOSROOT);
	if (!pIOS)
	{
		__debugbreak();
	}
	pFbxSdkManager->SetIOSettings(pIOS);

	FbxImporter* pImporter = FbxImporter::Create(pFbxSdkManager, "FBXImporter");
	if (!pImporter)
	{
		__debugbreak();
	}
	bool bStatus = pImporter->Initialize((szBasePath + fileNameA).c_str(), -1, pFbxSdkManager->GetIOSettings());
	if (!bStatus)
	{
		FbxString szError = pImporter->GetStatus().GetErrorString();
		char szDebugString[256];
		sprintf_s(szDebugString, 256, "Fbx Importer failed in Initializing with error %s\n", szError.Buffer());
		OutputDebugStringA(szDebugString);
		__debugbreak();
	}

	if (!pImporter->IsFBX())
	{
		__debugbreak();
	}

	FbxScene* pScene = FbxScene::Create(pFbxSdkManager, "FBXScene");
	if (!pScene)
	{
		__debugbreak();
	}

	
	bStatus = pImporter->Import(pScene);
	if (bStatus)
	{
		FbxGeometryConverter converter(pFbxSdkManager);
		converter.Triangulate(pScene, true);

		FbxNode* pRootNode = pScene->GetRootNode();
		int totalBoneCount = 0;
		// 최적화 필요.
		findDeformingBones(pRootNode);
		updateBoneIDs(pRootNode, &totalBoneCount);

		AnimData.BoneIDToNames.resize(totalBoneCount);
		for (auto iter = AnimData.BoneNameToID.begin(), endIter = AnimData.BoneNameToID.end(); iter != endIter; ++iter)
		{
			AnimData.BoneIDToNames[iter->second] = iter->first;
		}

		// 각 뼈마다 부모 인덱스를 저장할 준비.
		AnimData.BoneParents.resize(totalBoneCount, -1);

		Matrix globalTransform; // Initial transformation.
		AnimData.AccumulatedNodeTransforms.resize(totalBoneCount);
		AnimData.NodeTransforms.resize(totalBoneCount);
		processNode(pRootNode, pScene);

		// 애니메이션 정보 읽기.
		/*if (pScene->HasAnimations())
		{
			readAnimation(pScene);
		}*/

		updateTangents();
	}
	else
	{
		FbxString szError = pImporter->GetStatus().GetErrorString();
		char szDebugString[256];
		sprintf_s(szDebugString, 256, "Fbx Importer failed in Initializing with error %s\n", szError.Buffer());
		OutputDebugStringA(szDebugString);

		hr = E_FAIL;
	}

	pScene->Destroy();
	pImporter->Destroy();
	pIOS->Destroy();
	pFbxSdkManager->Destroy();

	return hr;
}

HRESULT FBXModelLoader::LoadAnimation(std::wstring& basePath, std::wstring& fileName)
{
	return E_NOTIMPL;
}

void FBXModelLoader::findDeformingBones(const FbxNode* pNODE)
{
	const FbxNodeAttribute* pNodeAttribute = pNODE->GetNodeAttribute();
	if (pNodeAttribute && pNodeAttribute->GetAttributeType() == FbxNodeAttribute::eSkeleton)
	{
		AnimData.BoneNameToID[pNODE->GetName()] = -1;
	}

	int childCount = pNODE->GetChildCount();
	for (int i = 0; i < childCount; ++i)
	{
		findDeformingBones(pNODE->GetChild(i));
	}
}

void FBXModelLoader::updateTangents()
{
	for (UINT64 i = 0, size = MeshInfos.size(); i < size; ++i)
	{
		MeshInfo& curMeshInfo = MeshInfos[i];
		std::vector<Vertex>& curVertices = curMeshInfo.Vertices;
		std::vector<SkinnedVertex>& curSkinnedVertices = curMeshInfo.SkinnedVertices;
		std::vector<uint32_t>& curIndices = curMeshInfo.Indices;
		UINT64 numFaces = curIndices.size() / 3;

		DirectX::XMFLOAT3 tangent;
		DirectX::XMFLOAT3 bitangent;

		for (UINT64 j = 0; j < numFaces; ++j)
		{
			calculateTangentBitangent(curVertices[curIndices[j * 3]], curVertices[curIndices[j * 3 + 1]], curVertices[curIndices[j * 3 + 2]], &tangent, &bitangent);

			curVertices[curIndices[j * 3]].Tangent = tangent;
			curVertices[curIndices[j * 3 + 1]].Tangent = tangent;
			curVertices[curIndices[j * 3 + 2]].Tangent = tangent;

			if (!curSkinnedVertices.empty()) // vertices와 skinned vertices가 같은 크기를 가지고 있다고 가정.
			{
				curSkinnedVertices[curIndices[j * 3]].Tangent = tangent;
				curSkinnedVertices[curIndices[j * 3 + 1]].Tangent = tangent;
				curSkinnedVertices[curIndices[j * 3 + 2]].Tangent = tangent;
			}
		}
	}
}

void FBXModelLoader::updateBoneIDs(const FbxNode* pNODE, int* pCounter)
{
	if (!pNODE)
	{
		return;
	}

	const char* pNODE_NAME = pNODE->GetName();
	if (AnimData.BoneNameToID.count(pNODE_NAME))
	{
		AnimData.BoneNameToID[pNODE_NAME] = *pCounter;
		++(*pCounter);
	}

	int childCount = pNODE->GetChildCount();
	for (int i = 0; i < childCount; ++i)
	{
		updateBoneIDs(pNODE->GetChild(i), pCounter);
	}
}

void FBXModelLoader::processNode(FbxNode* pNode, const FbxScene* pSCENE)
{
	if (!pNode)
	{
		return;
	}

	FbxNode* pPARENT = pNode->GetParent();
	FbxNodeAttribute* pNodeAttribute = pNode->GetNodeAttribute();
	
	if (pPARENT && pNodeAttribute->GetAttributeType() == FbxNodeAttribute::eSkeleton)
	{
		const int BONE_ID = AnimData.BoneNameToID[pNode->GetName()];
		AnimData.BoneParents[BONE_ID] = AnimData.BoneNameToID[pPARENT->GetName()];
	}

	if (pNodeAttribute && pNodeAttribute->GetAttributeType() == FbxNodeAttribute::eMesh)
	{
		FbxMesh* pMesh = pNode->GetMesh();
		MeshInfo meshInfo = INIT_MESH_INFO;
		processMesh(pMesh, pSCENE, &meshInfo);
		MeshInfos.push_back(meshInfo);
	}

	int childNodeCount = pNode->GetChildCount();
	for (int i = 0; i < childNodeCount; ++i)
	{
		processNode(pNode->GetChild(i), pSCENE);
	}
}

void FBXModelLoader::processMesh(FbxMesh* pMesh, const FbxScene* pSCENE, MeshInfo* pMeshInfo)
{
	if (!pMesh)
	{
		return;
	}

	std::vector<Vertex>& vertices = pMeshInfo->Vertices;
	std::vector<UINT>& indices = pMeshInfo->Indices;
	std::vector<SkinnedVertex>& skinnedVertices = pMeshInfo->SkinnedVertices;


	const int VERTEX_COUNT = pMesh->GetControlPointsCount();
	FbxVector4* pControlPoints = pMesh->GetControlPoints();
	Vector3* pPositions = new Vector3[VERTEX_COUNT];

	for (int i = 0; i < VERTEX_COUNT; ++i)
	{
		FbxVector4 vertex = pMesh->GetControlPointAt(i);
		pPositions[i] = {(float)vertex.mData[0], (float)vertex.mData[1], (float)vertex.mData[2]};
	}

	const int POLYGON_COUNT = pMesh->GetPolygonCount();
	UINT vertexCount = 0;
	for (int i = 0; i < POLYGON_COUNT; ++i)
	{
		const int VERTEX_COUNT_IN_POLYGON = pMesh->GetPolygonSize(i);
		if (VERTEX_COUNT_IN_POLYGON != 3)
		{
			__debugbreak();
		}

		for (int j = 0; j < VERTEX_COUNT_IN_POLYGON; ++j)
		{
			int vertexIndex = pMesh->GetPolygonVertex(i, j);
			
			Vector3* pPosition = &pPositions[vertexIndex];

			// Normal.
			const int NORMAL_ELEMENT_COUNT = pMesh->GetElementNormalCount();
			Vector3 vertexNormal;
			for (int k = 0; k < NORMAL_ELEMENT_COUNT; ++k)
			{
				FbxGeometryElementNormal* pVertexNormal = pMesh->GetElementNormal(k);
				switch (pVertexNormal->GetMappingMode())
				{
					case FbxGeometryElement::eByControlPoint:
					{
						switch (pVertexNormal->GetReferenceMode())
						{
							case FbxGeometryElement::eDirect:
							{
								FbxVector4 normal = pVertexNormal->GetDirectArray().GetAt(vertexIndex);
								vertexNormal = { (float)normal.mData[0], (float)normal.mData[1], (float)normal.mData[2] };
							}
							break;

							case FbxGeometryElement::eIndexToDirect:
							{
								int index = pVertexNormal->GetIndexArray().GetAt(vertexIndex);
								FbxVector4 normal = pVertexNormal->GetDirectArray().GetAt(index);
								vertexNormal = { (float)normal.mData[0], (float)normal.mData[1], (float)normal.mData[2] };
							}
							break;

							default:
								__debugbreak();
								break;
						}
					}
					break;

					case FbxGeometryElement::eByPolygonVertex:
					{
						switch (pVertexNormal->GetReferenceMode())
						{
							case FbxGeometryElement::eDirect:
							{
								FbxVector4 normal = pVertexNormal->GetDirectArray().GetAt(vertexCount);
								vertexNormal = { (float)normal.mData[0], (float)normal.mData[1], (float)normal.mData[2] };
							}
							break;

							case FbxGeometryElement::eIndexToDirect:
							{
								int index = pVertexNormal->GetIndexArray().GetAt(vertexCount);
								FbxVector4 normal = pVertexNormal->GetDirectArray().GetAt(index);
								vertexNormal = { (float)normal.mData[0], (float)normal.mData[1], (float)normal.mData[2] };
							}
							break;

							default:
								__debugbreak();
								break;
						}
					}
					break;

					default:
						__debugbreak();
						break;
				}
			}

			// Texcoord.
			int UVIndex = pMesh->GetTextureUVIndex(i, j);
			const int TEXCOORD_ELEMENT_COUNT = pMesh->GetElementUVCount();
			Vector2 vertexTexcoord;
			for (int k = 0; k < TEXCOORD_ELEMENT_COUNT; ++k)
			{
				FbxGeometryElementUV* pVertexUV = pMesh->GetElementUV(k);
				switch (pVertexUV->GetMappingMode())
				{
					case FbxGeometryElement::eByControlPoint:
					{
						switch (pVertexUV->GetReferenceMode())
						{
							case FbxGeometryElement::eDirect:
							{
								FbxVector2 texcoord = pVertexUV->GetDirectArray().GetAt(vertexIndex);
								vertexTexcoord = { (float)texcoord.mData[0], (float)texcoord.mData[1] };
							}
							break;

							case FbxGeometryElement::eIndexToDirect:
							{
								int index = pVertexUV->GetIndexArray().GetAt(vertexIndex);
								FbxVector2 texcoord = pVertexUV->GetDirectArray().GetAt(index);
								vertexTexcoord = { (float)texcoord.mData[0], (float)texcoord.mData[1] };
							}
							break;

							default:
								__debugbreak();
								break;
						}
					}
					break;

					case FbxGeometryElement::eByPolygonVertex:
					{
						switch (pVertexUV->GetReferenceMode())
						{
							case FbxGeometryElement::eDirect:
							{
								FbxVector2 texcoord = pVertexUV->GetDirectArray().GetAt(UVIndex);
								vertexTexcoord = { (float)texcoord.mData[0], (float)texcoord.mData[1] };
							}
							break;

							case FbxGeometryElement::eIndexToDirect:
							{
								int index = pVertexUV->GetIndexArray().GetAt(UVIndex);
								FbxVector2 texcoord = pVertexUV->GetDirectArray().GetAt(index);
								vertexTexcoord = { (float)texcoord.mData[0], (float)texcoord.mData[1] };
							}
							break;

							default:
								__debugbreak();
								break;
						}
					}
					break;

					default:
						__debugbreak();
						break;
				}
			}

			// Insert vertex.
			Vertex v;
			v.Position = *pPosition;
			v.Normal = vertexNormal;
			v.Texcoord = vertexTexcoord;

			vertices.push_back(v);
			indices.push_back(vertexCount);
			++vertexCount;
		}
	}


	std::vector<std::vector<float>> boneWeights;
	std::vector<std::vector<UINT8>> boneIndices;
	const int DEFORMER_COUNT = pMesh->GetDeformerCount();
	if (DEFORMER_COUNT > 0)
	{
		const UINT64 VERTEX_SIZE = vertices.size();
		skinnedVertices.resize(VERTEX_SIZE);
		boneWeights.resize(VERTEX_SIZE);
		boneIndices.resize(VERTEX_SIZE);
	}
	for (int i = 0; i < DEFORMER_COUNT; ++i)
	{
		FbxSkin* pSkin = (FbxSkin*)pMesh->GetDeformer(i, FbxDeformer::eSkin);
		if (!pSkin)
		{
			continue;
		}

		const int CLUSTER_COUNT = pSkin->GetClusterCount();
		for (int j = 0; j < CLUSTER_COUNT; ++j)
		{
			FbxCluster* pCluster = pSkin->GetCluster(j);
			int* pVertexIndices = pCluster->GetControlPointIndices();
			double* pBoneWeights = pCluster->GetControlPointWeights();
			const int VERTEX_COUNT = pCluster->GetControlPointIndicesCount();
			
			const char* szBoneName = pCluster->GetLink()->GetName();
			const int BONE_ID = AnimData.BoneNameToID[szBoneName];

			for (int k = 0; k < VERTEX_COUNT; ++k)
			{
				boneIndices[pVertexIndices[k]].push_back(BONE_ID);
				boneWeights[pVertexIndices[k]].push_back((float)pBoneWeights[k]);
			}
		}
	}
	for (UINT64 i = 0, VERTEX_SIZE = skinnedVertices.size(); i < VERTEX_SIZE; ++i)
	{
		skinnedVertices[i].Position = vertices[i].Position;
		skinnedVertices[i].Normal = vertices[i].Normal;
		skinnedVertices[i].Texcoord = vertices[i].Texcoord;
		skinnedVertices[i].Tangent = vertices[i].Tangent;

		for (UINT64 j = 0, curBoneWeightsSize = boneWeights[i].size(); j < curBoneWeightsSize; ++j)
		{
			skinnedVertices[i].BlendWeights[j] = boneWeights[i][j];
			skinnedVertices[i].BoneIndices[j] = boneIndices[i][j];
		}
	}


	bool bIsAllSame = true;
	const int MATERIAL_COUNT = pMesh->GetElementMaterialCount();
	for (int i = 0; i < MATERIAL_COUNT; ++i)
	{
		FbxGeometryElementMaterial* pMaterialElement = pMesh->GetElementMaterial(i);
		if (pMaterialElement->GetMappingMode() == FbxGeometryElement::eByPolygon)
		{
			bIsAllSame = false;
			break;
		}
	}

	if (bIsAllSame)
	{
		for (int i = 0; i < MATERIAL_COUNT; ++i)
		{
			FbxGeometryElementMaterial* pMaterialElement = pMesh->GetElementMaterial(i);
			if (pMaterialElement->GetMappingMode() == FbxGeometryElement::eAllSame)
			{
				FbxSurfaceMaterial* lMaterial = pMesh->GetNode()->GetMaterial(pMaterialElement->GetIndexArray().GetAt(0));
				int materialID = pMaterialElement->GetIndexArray().GetAt(0);
				if (materialID >= 0)
				{
					FbxSurfaceMaterial::sReflection;
				}
			}
		}
	}
	else
	{

	}

	delete[] pPositions;
}

void FBXModelLoader::calculateTangentBitangent(const Vertex& V1, const Vertex& V2, const Vertex& V3, DirectX::XMFLOAT3* pTangent, DirectX::XMFLOAT3* pBitangent)
{
	DirectX::XMFLOAT3 vector1;
	DirectX::XMFLOAT3 vector2;
	DirectX::XMFLOAT2 tuVector;
	DirectX::XMFLOAT2 tvVector;
	DirectX::XMStoreFloat3(&vector1, V2.Position - V1.Position);
	DirectX::XMStoreFloat3(&vector2, V3.Position - V1.Position);
	DirectX::XMStoreFloat2(&tuVector, V2.Texcoord - V1.Texcoord);
	DirectX::XMStoreFloat2(&tvVector, V2.Texcoord - V3.Texcoord);

	float den = 1.0f / (tuVector.x * tvVector.y - tuVector.y * tvVector.x);

	pTangent->x = (tvVector.y * vector1.x - tvVector.x * vector2.x) * den;
	pTangent->y = (tvVector.y * vector1.y - tvVector.x * vector2.y) * den;
	pTangent->z = (tvVector.y * vector1.z - tvVector.x * vector2.z) * den;

	pBitangent->x = (tuVector.x * vector2.x - tuVector.y * vector1.x) * den;
	pBitangent->y = (tuVector.x * vector2.y - tuVector.y * vector1.y) * den;
	pBitangent->z = (tuVector.x * vector2.z - tuVector.y * vector1.z) * den;

	float length = sqrt((pTangent->x * pTangent->x) + (pTangent->y * pTangent->y) + (pTangent->z * pTangent->z));
	pTangent->x = pTangent->x / length;
	pTangent->y = pTangent->y / length;
	pTangent->z = pTangent->z / length;

	length = sqrt((pBitangent->x * pBitangent->x) + (pBitangent->y * pBitangent->y) + (pBitangent->z * pBitangent->z));
	pBitangent->x = pBitangent->x / length;
	pBitangent->y = pBitangent->y / length;
	pBitangent->z = pBitangent->z / length;
}
