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
		AnimData.OffsetMatrices.resize(totalBoneCount);
		AnimData.InverseOffsetMatrices.resize(totalBoneCount);
		AnimData.BoneTransforms.resize(totalBoneCount);
		processNode(pRootNode, pScene);

		// 애니메이션 정보 읽기.
		/*if (pScene->HasAnimations())
		{
			readAnimation(pScene);
		}*/
		const int ANIM_STACK_COUNT = pScene->GetSrcObjectCount<FbxAnimStack>();
		AnimData.Clips.resize(ANIM_STACK_COUNT);
		for (int i = 0; i < ANIM_STACK_COUNT; ++i)
		{
			FbxAnimStack* pAnimStack = pScene->GetSrcObject<FbxAnimStack>(i);
			AnimData.Clips[i].Name = std::string(pAnimStack->GetName());
			AnimData.Clips[i].Keys.resize(totalBoneCount);

			readAnimationData(pAnimStack, pRootNode, pScene, i);
		}

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

	const int CHILD_COUNT = pNODE->GetChildCount();
	for (int i = 0; i < CHILD_COUNT; ++i)
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

Vector3 FBXModelLoader::readNormal(FbxMesh* pMesh, int vertexIndex, int vertexCount)
{
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

	return vertexNormal;
}

Vector2 FBXModelLoader::readTexcoord(FbxMesh* pMesh, int vertexIndex, int UVIndex)
{
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

	return vertexTexcoord;
}

void FBXModelLoader::readMaterial(FbxProperty& materialProperty, MeshInfo* pMeshInfo, eTextureType type)
{
	WCHAR szFileName[256] = { 0, };

	const int LAYERED_TEXTURE_COUNT = materialProperty.GetSrcObjectCount<FbxLayeredTexture>();
	if (LAYERED_TEXTURE_COUNT > 0)
	{
		for (int j = 0; j < LAYERED_TEXTURE_COUNT; ++j)
		{
			FbxLayeredTexture* pLayeredTexture = materialProperty.GetSrcObject<FbxLayeredTexture>(j);
			const int TEXTURE_COUNT = pLayeredTexture->GetSrcObjectCount<FbxTexture>();

			for (int k = 0; k < TEXTURE_COUNT; ++k)
			{
				OutputDebugStringA((char*)pLayeredTexture->GetName());
				mbstowcs(szFileName, pLayeredTexture->GetName(), 256);

				switch (type)
				{
					case TextureType_Albedo:
						pMeshInfo->szAlbedoTextureFileName = std::wstring(szFileName) + L".png";
						break;

					case TextureType_Emissive:
						pMeshInfo->szEmissiveTextureFileName = std::wstring(szFileName) + L".png";
						break;

					case TextureType_Specular:
						pMeshInfo->szMetallicTextureFileName = std::wstring(szFileName) + L".png";
						break;

					case TextureType_Ambient:
						pMeshInfo->szAOTextureFileName = std::wstring(szFileName) + L".png";
						break;

					case TextureType_Normal:
						pMeshInfo->szNormalTextureFileName = std::wstring(szFileName) + L".png";
						break;

					default:
						break;
				}
			}
		}
	}
	else
	{
		const int TEXTURE_COUNT = materialProperty.GetSrcObjectCount<FbxTexture>();
		if (TEXTURE_COUNT <= 0)
		{
			return;
		}

		for (int j = 0; j < TEXTURE_COUNT; ++j)
		{
			FbxTexture* pTexture = materialProperty.GetSrcObject<FbxTexture>(j);
			if (!pTexture)
			{
				continue;
			}

			OutputDebugStringA((char*)pTexture->GetName());
			mbstowcs(szFileName, pTexture->GetName(), 256);

			switch (type)
			{
				case TextureType_Albedo:
					pMeshInfo->szAlbedoTextureFileName = std::wstring(szFileName);
					break;

				case TextureType_Emissive:
					pMeshInfo->szEmissiveTextureFileName = std::wstring(szFileName);
					break;

				case TextureType_Specular:
					pMeshInfo->szMetallicTextureFileName = std::wstring(szFileName);
					break;

				case TextureType_Ambient:
					pMeshInfo->szAOTextureFileName = std::wstring(szFileName);
					break;

				case TextureType_Normal:
					pMeshInfo->szNormalTextureFileName = std::wstring(szFileName);
					break;

				default:
					break;
			}
		}
	}
}

void FBXModelLoader::readAnimationData(FbxAnimStack* pAnimStack, FbxNode* pNode, const FbxScene* pSCENE, int animStackIndex)
{
	FbxNodeAttribute* pNodeAttribute = pNode->GetNodeAttribute();
	if (!pNodeAttribute || pNodeAttribute->GetAttributeType() != FbxNodeAttribute::eSkeleton)
	{
		goto LB_CHILD;
	}

	{
		const int BONE_ID = AnimData.BoneNameToID[pNode->GetName()];

		FbxTakeInfo* pTakeInfo = pSCENE->GetTakeInfo(pAnimStack->GetName());
		FbxTime start = pTakeInfo->mLocalTimeSpan.GetStart();
		FbxTime end = pTakeInfo->mLocalTimeSpan.GetStop();

		FbxTime::EMode timeMode = pSCENE->GetGlobalSettings().GetTimeMode();

		FbxLongLong totalFrame = end.GetFrameCount(timeMode) - start.GetFrameCount(timeMode) + 1;
		AnimData.Clips[animStackIndex].Keys[BONE_ID].resize(totalFrame);
		
		FbxVector4 m0 = { 1.0f, 0.0f, 0.0f, 0.0f };
		FbxVector4 m1 = { 0.0f, 0.0f, 1.0f, 0.0f };
		FbxVector4 m2 = { 0.0f, 1.0f, 0.0f, 0.0f };
		FbxVector4 m3 = { 0.0f, 0.0f, 0.0f, 1.0f };
		FbxAMatrix reflect;
		reflect[0] = m0;
		reflect[1] = m1;
		reflect[2] = m2;
		reflect[3] = m3;

		for (FbxLongLong frame = start.GetFrameCount(timeMode), endFrame = end.GetFrameCount(timeMode); frame < endFrame; ++frame)
		{
			FbxTime curTime;
			curTime.SetFrame(frame, timeMode);

			FbxAMatrix keyTransform = pNode->EvaluateGlobalTransform(curTime);
			// keyTransform = reflect * keyTransform * reflect;

			FbxVector4 pos = keyTransform.GetT();
			FbxQuaternion rot = keyTransform.GetQ();
			FbxVector4 scale = keyTransform.GetS();

			AnimationClip::Key& key = AnimData.Clips[animStackIndex].Keys[BONE_ID][frame];
			key.Position = { (float)pos.mData[0], (float)pos.mData[1],(float)pos.mData[2] };
			key.Rotation = { (float)rot.mData[0], (float)rot.mData[1],(float)rot.mData[2], (float)rot.mData[3] };
			key.Scale = { (float)scale.mData[0], (float)scale.mData[1],(float)scale.mData[2] };
		}
	}

LB_CHILD:
	// child.
	const int CHILD_COUNT = pNode->GetChildCount();
	for (int i = 0; i < CHILD_COUNT; ++i)
	{
		readAnimationData(pAnimStack, pNode->GetChild(i), pSCENE, animStackIndex);
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

	vertices.resize(VERTEX_COUNT);
	for (int i = 0; i < VERTEX_COUNT; ++i)
	{
		FbxVector4 vertex = pMesh->GetControlPointAt(i);
		vertices[i].Position = { (float)vertex.mData[0], (float)vertex.mData[1], (float)vertex.mData[2] };
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
			int UVIndex = pMesh->GetTextureUVIndex(i, j);
			
			Vector3 vertexNormal = readNormal(pMesh, vertexIndex, vertexCount);
			Vector2 vertexTexcoord = readTexcoord(pMesh, vertexIndex, UVIndex);

			// Insert vertex.
			/*Vertex v;
			v.Position = *pPosition;
			v.Normal = vertexNormal;
			v.Texcoord = vertexTexcoord;*/

			/*vertices.push_back(v);
			indices.push_back(vertexCount);*/

			Vertex& vertex = vertices[vertexIndex];
			vertex.Normal = vertexNormal;
			vertex.Texcoord = vertexTexcoord;

			indices.push_back(vertexIndex);

			++vertexCount;
		}
	}


	FbxNode* pNode = pMesh->GetNode();
	if (!pNode)
	{
		return;
	}

	const int MATERIAL_COUNT = pNode->GetMaterialCount();
	for (int i = 0; i < MATERIAL_COUNT; ++i)
	{
		FbxSurfaceMaterial* pMaterial = pNode->GetMaterial(i);
		FbxProperty propertyForMaterial;
		
		propertyForMaterial = pMaterial->FindProperty(FbxSurfaceMaterial::sDiffuse);
		if (propertyForMaterial.IsValid())
		{
			readMaterial(propertyForMaterial, pMeshInfo, TextureType_Albedo);
		}

		propertyForMaterial = pMaterial->FindProperty(FbxSurfaceMaterial::sEmissive);
		if (propertyForMaterial.IsValid())
		{
			readMaterial(propertyForMaterial, pMeshInfo, TextureType_Emissive);
		}

		propertyForMaterial = pMaterial->FindProperty(FbxSurfaceMaterial::sSpecular);
		if (propertyForMaterial.IsValid())
		{
			readMaterial(propertyForMaterial, pMeshInfo, TextureType_Specular);
		}

		propertyForMaterial = pMaterial->FindProperty(FbxSurfaceMaterial::sAmbient);
		if (propertyForMaterial.IsValid())
		{
			readMaterial(propertyForMaterial, pMeshInfo, TextureType_Ambient);
		}

		propertyForMaterial = pMaterial->FindProperty(FbxSurfaceMaterial::sNormalMap);
		if (propertyForMaterial.IsValid())
		{
			readMaterial(propertyForMaterial, pMeshInfo, TextureType_Normal);
		}
	}


	std::vector<std::vector<float>> boneWeights;
	std::vector<std::vector<UINT8>> boneIndices;
	const UINT64 VERTEX_SIZE = vertices.size();

	const int SKIN_COUNT = pMesh->GetDeformerCount(FbxDeformer::eSkin);
	if (SKIN_COUNT > 0)
	{
		skinnedVertices.resize(VERTEX_SIZE);
		boneWeights.resize(VERTEX_SIZE);
		boneIndices.resize(VERTEX_SIZE);
	}
	for (int i = 0; i < SKIN_COUNT; ++i)
	{
		FbxSkin* pSkin = (FbxSkin*)pMesh->GetDeformer(i, FbxDeformer::eSkin);
		FbxSkin::EType type = pSkin->GetSkinningType();

		if (type != FbxSkin::eRigid && type != FbxSkin::eLinear)
		{
			continue;
		}

		const int CLUSTER_COUNT = pSkin->GetClusterCount();
		for (int j = 0; j < CLUSTER_COUNT; ++j)
		{
			FbxCluster* pCluster = pSkin->GetCluster(j);
			const char* szBoneName = pCluster->GetLink()->GetName();
			const int BONE_ID = AnimData.BoneNameToID[szBoneName];

			int* pVertexIndices = pCluster->GetControlPointIndices();
			double* pBoneWeights = pCluster->GetControlPointWeights();

			const int INDEX_COUNT = pCluster->GetControlPointIndicesCount();
			for (int k = 0; k < INDEX_COUNT; ++k)
			{
				boneIndices[pVertexIndices[k]].push_back(BONE_ID);
				boneWeights[pVertexIndices[k]].push_back((float)pBoneWeights[k]);
			}
			
			FbxVector4 m0 = { 1.0f, 0.0f, 0.0f, 0.0f };
			FbxVector4 m1 = { 0.0f, 0.0f, 1.0f, 0.0f };
			FbxVector4 m2 = { 0.0f, 1.0f, 0.0f, 0.0f };
			FbxVector4 m3 = { 0.0f, 0.0f, 0.0f, 1.0f };
			FbxAMatrix reflect;
			reflect[0] = m0;
			reflect[1] = m1;
			reflect[2] = m2;
			reflect[3] = m3;

			FbxAMatrix transform;
			FbxAMatrix linkTransform;
			FbxAMatrix boneOffsetMatrix;
			pCluster->GetTransformMatrix(transform);
			pCluster->GetTransformLinkMatrix(linkTransform);
			boneOffsetMatrix = linkTransform.Inverse() * transform;
			// boneOffsetMatrix = reflect * boneOffsetMatrix * reflect;

			for (int a = 0; a < 4; ++a)
			{
				for (int b = 0; b < 4; ++b)
				{
					AnimData.OffsetMatrices[BONE_ID](a, b) = (float)boneOffsetMatrix.Get(a, b);
				}
			}
			AnimData.InverseOffsetMatrices[BONE_ID] = AnimData.OffsetMatrices[BONE_ID].Invert();
		}
	}
	for (UINT64 i = 0; i < VERTEX_SIZE; ++i)
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
