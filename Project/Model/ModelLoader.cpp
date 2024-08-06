#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/material.h>

// #include <fbxsdk.h>

#include <iostream>
#include <fstream>
#include <string>
#include <locale>
#include "../pch.h"
#include "../Util/Utility.h"
#include "ModelLoader.h"

HRESULT ModelLoader::Load(std::wstring& basePath, std::wstring& fileName, bool _bRevertNormal)
{
	HRESULT hr = S_OK;
	
	if (GetFileExtension(fileName).compare(L".gltf") == 0)
	{
		bIsGLTF = true;
		bRevertNormal = _bRevertNormal;
	}

	std::string fileNameA(fileName.begin(), fileName.end());
	szBasePath = std::string(basePath.begin(), basePath.end());

	Assimp::Importer importer;
	const aiScene* pSCENE = importer.ReadFile(szBasePath + fileNameA, aiProcess_Triangulate | aiProcess_ConvertToLeftHanded);

	if (pSCENE)
	{
		// 모든 메쉬에 대해, 정점에 영향 주는 뼈들의 목록을 생성.
		findDeformingBones(pSCENE);

		// 트리 구조를 따라, 업데이트 순서대로 뼈들의 인덱스를 결정.
		int totalBoneCount = 0;
		updateBoneIDs(pSCENE->mRootNode, &totalBoneCount);

		// 업데이트 순서대로 뼈 이름 저장. (pBoneIDToNames)
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
		processNode(pSCENE->mRootNode, pSCENE, globalTransform);

		// 애니메이션 정보 읽기.
		if (pSCENE->HasAnimations())
		{
			readAnimation(pSCENE);
		}

		updateTangents();
	}
	else
	{
		const char* pErrorDescription = importer.GetErrorString();
		char szDebugString[256];

		sprintf_s(szDebugString, 256, "Failed to read file: %s\nAssimp error: %s\n", (szBasePath + fileNameA).c_str(), pErrorDescription);
		OutputDebugStringA(szDebugString);

		hr = E_FAIL;
	}

	return hr;
}

HRESULT ModelLoader::LoadAnimation(std::wstring& basePath, std::wstring& fileName)
{
	HRESULT hr = S_OK;

	std::string fileNameA(fileName.begin(), fileName.end());
	szBasePath = std::string(basePath.begin(), basePath.end());

	Assimp::Importer importer;
	const aiScene* pSCENE = importer.ReadFile(szBasePath + fileNameA, aiProcess_Triangulate | aiProcess_ConvertToLeftHanded);

	if (pSCENE && pSCENE->HasAnimations())
	{
		readAnimation(pSCENE);
	}
	else
	{
		const char* pErrorDescription = importer.GetErrorString();
		char szDebugString[256];

		sprintf_s(szDebugString, 256, "Failed to read animation from file: %s\n Assimp error: %s\n", (szBasePath + fileNameA).c_str(), pErrorDescription);
		OutputDebugStringA(szDebugString);

		hr = E_FAIL;
	}

	return hr;
}

void ModelLoader::findDeformingBones(const aiScene* pSCENE)
{
	for (UINT i = 0; i < pSCENE->mNumMeshes; ++i)
	{
		const aiMesh* pMESH = pSCENE->mMeshes[i];
		if (pMESH->HasBones())
		{
			for (UINT j = 0; j < pMESH->mNumBones; ++j)
			{
				const aiBone* pBONE = pMESH->mBones[j];
				AnimData.BoneNameToID[pBONE->mName.C_Str()] = -1;
			}
		}
	}
}

const aiNode* ModelLoader::findParent(const aiNode * pNODE)
{
	if (!pNODE)
	{
		return nullptr;
	}
	if (AnimData.BoneNameToID.count(pNODE->mName.C_Str()) > 0)
	{
		return pNODE;
	}
	return findParent(pNODE->mParent);
}

void ModelLoader::processNode(aiNode* pNode, const aiScene* pSCENE, Matrix& transform)
{
	// 현재 노드의 global transform 계산.
	Matrix globalTransform(&pNode->mTransformation.a1);
	globalTransform = globalTransform.Transpose() * transform;

	// 사용되는 부모 뼈를 찾아서 부모의 인덱스 저장.
	const aiNode* pPARENT = findParent(pNode->mParent);
	const char* pNODE_NAME = pNode->mName.C_Str();
	if (pNode->mParent &&
		AnimData.BoneNameToID.count(pNODE_NAME) > 0 &&
		pPARENT)
	{
		const int BONE_ID = AnimData.BoneNameToID[pNODE_NAME];
		AnimData.BoneParents[BONE_ID] = AnimData.BoneNameToID[pPARENT->mName.C_Str()];
		AnimData.AccumulatedNodeTransforms[BONE_ID] = globalTransform;
		AnimData.NodeTransforms[BONE_ID] = Matrix(&pNode->mTransformation.a1).Transpose();
	}

	for (UINT i = 0; i < pNode->mNumMeshes; ++i)
	{
		aiMesh* pMesh = pSCENE->mMeshes[pNode->mMeshes[i]];
		MeshInfo newMeshInfo;

		processMesh(pMesh, pSCENE, &newMeshInfo);
		/*for (UINT64 j = 0, size = newMeshInfo.Vertices.size(); j < size; ++j)
		{
			Vertex& v = newMeshInfo.Vertices[j];
			v.Position = DirectX::SimpleMath::Vector3::Transform(v.Position, globalTransform);
		}*/

		MeshInfos.push_back(newMeshInfo);
	}

	for (UINT i = 0; i < pNode->mNumChildren; ++i)
	{
		processNode(pNode->mChildren[i], pSCENE, globalTransform);
	}
}

void ModelLoader::processMesh(aiMesh* pMesh, const aiScene* pSCENE, MeshInfo* pMeshInfo)
{
	std::vector<Vertex>& vertices = pMeshInfo->Vertices;
	std::vector<uint32_t>& indices = pMeshInfo->Indices;
	std::vector<SkinnedVertex>& skinnedVertices = pMeshInfo->SkinnedVertices;

	// Walk through each of the mesh's vertices.
	vertices.resize(pMesh->mNumVertices);
	for (UINT i = 0; i < pMesh->mNumVertices; ++i)
	{
		Vertex& vertex = vertices[i];

		vertex.Position.x = pMesh->mVertices[i].x;
		vertex.Position.y = pMesh->mVertices[i].y;
		vertex.Position.z = pMesh->mVertices[i].z;

		if (pMesh->mNormals)
		{
			vertex.Normal.x = pMesh->mNormals[i].x;
			if (bIsGLTF)
			{
				vertex.Normal.y = pMesh->mNormals[i].z;
				vertex.Normal.z = -pMesh->mNormals[i].y;
			}
			else
			{
				vertex.Normal.y = pMesh->mNormals[i].y;
				vertex.Normal.z = pMesh->mNormals[i].z;
			}

			if (bRevertNormal)
			{
				vertex.Normal *= -1.0f;
			}

			vertex.Normal.Normalize();
		}

		if (pMesh->mTextureCoords[0])
		{
			vertex.Texcoord.x = (float)(pMesh->mTextureCoords[0][i].x);
			vertex.Texcoord.y = (float)(pMesh->mTextureCoords[0][i].y);
		}
	}

	indices.reserve(pMesh->mNumFaces);
	for (UINT i = 0; i < pMesh->mNumFaces; ++i)
	{
		aiFace face = pMesh->mFaces[i];
		for (UINT j = 0; j < face.mNumIndices; ++j)
		{
			indices.push_back(face.mIndices[j]);
		}
	}

	if (pMesh->HasBones())
	{
		const UINT64 VERT_SIZE = vertices.size();
		std::vector<std::vector<float>> boneWeights(VERT_SIZE);
		std::vector<std::vector<UINT8>> boneIndices(VERT_SIZE);
		
		const UINT64 TOTAL_BONE = AnimData.BoneNameToID.size();
		AnimData.OffsetMatrices.resize(TOTAL_BONE);
		AnimData.InverseOffsetMatrices.resize(TOTAL_BONE);
		AnimData.BoneTransforms.resize(TOTAL_BONE);

		int count = 0;
		for (UINT i = 0; i < pMesh->mNumBones; ++i)
		{
			const aiBone* pBONE = pMesh->mBones[i];
			const UINT BONE_ID = AnimData.BoneNameToID[pBONE->mName.C_Str()];

			AnimData.OffsetMatrices[BONE_ID] = Matrix(&pBONE->mOffsetMatrix.a1).Transpose();
			AnimData.InverseOffsetMatrices[BONE_ID] = AnimData.OffsetMatrices[BONE_ID].Invert();

			// 이 뼈가 영향을 주는 정점 개수.
			for (UINT j = 0; j < pBONE->mNumWeights; ++j)
			{
				aiVertexWeight weight = pBONE->mWeights[j];
				_ASSERT(weight.mVertexId < boneIndices.size());

				boneIndices[weight.mVertexId].push_back(BONE_ID);
				boneWeights[weight.mVertexId].push_back(weight.mWeight);
			}
		}

		int maxBones = 0;
		for (UINT64 i = 0, boneWeightSize = boneWeights.size(); i < boneWeightSize; ++i)
		{
			maxBones = Max(maxBones, (int)(boneWeights[i].size()));
		}

		{
			char debugString[256];
			sprintf_s(debugString, "Max number of influencing bones per vertex = %d\n", maxBones);
			OutputDebugStringA(debugString);
		}

		skinnedVertices.resize(VERT_SIZE);
		for (UINT64 i = 0; i < VERT_SIZE; ++i)
		{
			skinnedVertices[i].Position = vertices[i].Position;
			skinnedVertices[i].Normal = vertices[i].Normal;
			skinnedVertices[i].Texcoord = vertices[i].Texcoord;

			for (UINT64 j = 0, curBoneWeightsSize = boneWeights[i].size(); j < curBoneWeightsSize; ++j)
			{
				skinnedVertices[i].BlendWeights[j] = boneWeights[i][j];
				skinnedVertices[i].BoneIndices[j] = boneIndices[i][j];
			}
		}
	}

	if (pMesh->mMaterialIndex >= 0)
	{
		aiMaterial* pMaterial = pSCENE->mMaterials[pMesh->mMaterialIndex];

		readTextureFileName(pSCENE, pMaterial, aiTextureType_BASE_COLOR, &(pMeshInfo->szAlbedoTextureFileName));
		if (pMeshInfo->szAlbedoTextureFileName.empty())
		{
			readTextureFileName(pSCENE, pMaterial, aiTextureType_DIFFUSE, &(pMeshInfo->szAlbedoTextureFileName));
		}
		readTextureFileName(pSCENE, pMaterial, aiTextureType_EMISSIVE, &(pMeshInfo->szEmissiveTextureFileName));
		readTextureFileName(pSCENE, pMaterial, aiTextureType_HEIGHT, &(pMeshInfo->szHeightTextureFileName));
		readTextureFileName(pSCENE, pMaterial, aiTextureType_NORMALS, &(pMeshInfo->szNormalTextureFileName));
		readTextureFileName(pSCENE, pMaterial, aiTextureType_METALNESS, &(pMeshInfo->szMetallicTextureFileName));
		readTextureFileName(pSCENE, pMaterial, aiTextureType_DIFFUSE_ROUGHNESS, &(pMeshInfo->szRoughnessTextureFileName));
		readTextureFileName(pSCENE, pMaterial, aiTextureType_AMBIENT_OCCLUSION, &(pMeshInfo->szAOTextureFileName));
		if (pMeshInfo->szAOTextureFileName.empty())
		{
			readTextureFileName(pSCENE, pMaterial, aiTextureType_LIGHTMAP, &(pMeshInfo->szAOTextureFileName));
		}
		readTextureFileName(pSCENE, pMaterial, aiTextureType_OPACITY, &(pMeshInfo->szOpacityTextureFileName)); // 불투명도를 표현하는 텍스쳐.

		if (!pMeshInfo->szOpacityTextureFileName.empty())
		{
			WCHAR szDebugString[256];
			swprintf_s(szDebugString, 256, L"%s\nOpacity %s\n", pMeshInfo->szAlbedoTextureFileName.c_str(), pMeshInfo->szOpacityTextureFileName.c_str());
			OutputDebugStringW(szDebugString);
		}
	}
}

void ModelLoader::readAnimation(const aiScene* pSCENE)
{
	AnimData.Clips.resize(pSCENE->mNumAnimations);

	for (UINT i = 0; i < pSCENE->mNumAnimations; ++i)
	{
		AnimationClip& clip = AnimData.Clips[i];
		const aiAnimation* pANIM = pSCENE->mAnimations[i];
		const UINT64 TOTAL_BONES = AnimData.BoneNameToID.size();

		clip.Duration = pANIM->mDuration;
		clip.TicksPerSec = pANIM->mTicksPerSecond;
		clip.Keys.resize(TOTAL_BONES);
		clip.NumChannels = pANIM->mNumChannels;

		for (UINT c = 0; c < pANIM->mNumChannels; ++c)
		{
			// channel은 각 뼈들의 움직임이 channel로 제공됨을 의미.
			const aiNodeAnim* pNODE_ANIM = pANIM->mChannels[c];
			const int BONE_ID = AnimData.BoneNameToID[pNODE_ANIM->mNodeName.C_Str()];
			clip.Keys[BONE_ID].resize(pNODE_ANIM->mNumPositionKeys);

			for (UINT k = 0; k < pNODE_ANIM->mNumPositionKeys; ++k)
			{
				const aiVector3D& POS = pNODE_ANIM->mPositionKeys[k].mValue;
				const aiQuaternion& ROTATION = pNODE_ANIM->mRotationKeys[k].mValue;
				const aiVector3D& SCALE = pNODE_ANIM->mScalingKeys[k].mValue;

				AnimationClip::Key& key = clip.Keys[BONE_ID][k];
				key.Position = Vector3(POS.x, POS.y, POS.z);
				key.Rotation = Quaternion(ROTATION.x, ROTATION.y, ROTATION.z, ROTATION.w);
				key.Scale = Vector3(SCALE.x, SCALE.y, SCALE.z);
			}
		}

		// key data가 없는 곳을 default로 채움.
		for (UINT64 boneID = 0; boneID < TOTAL_BONES; ++boneID)
		{
			std::vector<AnimationClip::Key>& keys = clip.Keys[boneID];
			const UINT64 KEY_SIZE = keys.size();
			if (KEY_SIZE == 0 || keys[0].Position == Vector3(0.0f))
			{
				// keys.push_back(AnimationClip::Key());

				Matrix& nodeTransform = AnimData.NodeTransforms[boneID];

				AnimationClip::Key key;
				key.Position = nodeTransform.Translation();
				key.Rotation = Quaternion::CreateFromRotationMatrix(nodeTransform);
				keys.push_back(key);
			}
		}
	}
}

HRESULT ModelLoader::readTextureFileName(const aiScene* pSCENE, aiMaterial* pMaterial, aiTextureType type, std::wstring* pDst)
{
	HRESULT hr = S_OK;

	if (pMaterial->GetTextureCount(type) > 0)
	{
		aiString filePath;
		pMaterial->GetTexture(type, 0, &filePath);

		std::string fullPath = szBasePath + RemoveBasePath(filePath.C_Str());
		struct _stat64 sourceFileStat;

		// 실제로 파일이 존재하는지 확인.
		if (_stat64(fullPath.c_str(), &sourceFileStat) == -1)
		{
			// 파일이 없을 경우 혹시 fbx 자체에 Embedded인지 확인.
			const aiTexture* pTEXTURE = pSCENE->GetEmbeddedTexture(filePath.C_Str());
			if (pTEXTURE)
			{
				// Embedded texture가 존재하고 png일 경우 저장.(png가 아닐 수도..)
				if (std::string(pTEXTURE->achFormatHint).find("png") != std::string::npos)
				{
					std::ofstream fileSystem(fullPath.c_str(), std::ios::binary | std::ios::out);
					fileSystem.write((char*)pTEXTURE->pcData, pTEXTURE->mWidth);
					fileSystem.close();
				}
			}
			else
			{
				char szDebugString[256];
				sprintf_s(szDebugString, 256, "%s doesn't exists. Return empty filename.\n", fullPath.c_str());
				OutputDebugStringA(szDebugString);
			}
		}
		else
		{
			*pDst = std::wstring(fullPath.begin(), fullPath.end());
		}
	}
	else
	{
		hr = E_FAIL;
	}

	return hr;
}

void ModelLoader::updateTangents()
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

			if (curSkinnedVertices.empty() == false) // vertices와 skinned vertices가 같은 크기를 가지고 있다고 가정.
			{
				curSkinnedVertices[curIndices[j * 3]].Tangent = tangent;
				curSkinnedVertices[curIndices[j * 3 + 1]].Tangent = tangent;
				curSkinnedVertices[curIndices[j * 3 + 2]].Tangent = tangent;
			}
		}
	}
}

void ModelLoader::updateBoneIDs(aiNode* pNode, int* pCounter)
{
	static int s_ID = 0;
	if (pNode)
	{
		const char* pNODE_NAME = pNode->mName.C_Str();
		if (AnimData.BoneNameToID.count(pNODE_NAME))
		{
			AnimData.BoneNameToID[pNODE_NAME] = *pCounter;
			*pCounter += 1;
		}
		for (UINT i = 0; i < pNode->mNumChildren; ++i)
		{
			updateBoneIDs(pNode->mChildren[i], pCounter);
		}
	}
}

void ModelLoader::calculateTangentBitangent(const Vertex& V1, const Vertex& V2, const Vertex& V3, DirectX::XMFLOAT3* pTangent, DirectX::XMFLOAT3* pBitangent)
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
