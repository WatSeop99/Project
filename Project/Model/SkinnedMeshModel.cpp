#include "../pch.h"
#include "../Renderer/ConstantDataType.h"
#include "../Model/GeometryGenerator.h"
#include "../Graphics/GraphicsUtil.h"
#include "SkinnedMeshModel.h"

SkinnedMeshModel::SkinnedMeshModel()
{
	ModelType = RenderObjectType_SkinnedType;
}

void SkinnedMeshModel::Initialize(Renderer* pRenderer, const std::vector<MeshInfo>& MESH_INFOS, const AnimationData& ANIM_DATA)
{
	_ASSERT(pRenderer);

	m_pRenderer = pRenderer;

	Model::Initialize(pRenderer, MESH_INFOS);
	InitAnimationData(pRenderer, ANIM_DATA);
	/*{
		Matrix spineNode = CharacterAnimationData.NodeTransforms[1];
		Matrix spineOffset = CharacterAnimationData.OffsetMatrices[1].Invert();
		Matrix spine1Offset = CharacterAnimationData.OffsetMatrices[2].Invert();
		Vector3 pos = Vector3::Transform(Vector3(0.0f), spineOffset);
		Vector3 pos1 = Vector3::Transform(Vector3(0.0f), spine1Offset);

		int a = 0;
	}*/
	initBoundingCapsule();
	initJointSpheres();
	initChain();

	CharacterAnimationData.Position = World.Translation();
	CharacterAnimationData.Direction = Vector3(0.0f, 0.0f, -1.0f); // should be normalized.
}

void SkinnedMeshModel::InitMeshBuffers(Renderer* pRenderer, const MeshInfo& MESH_INFO, Mesh* pNewMesh)
{
	_ASSERT(pRenderer);

	HRESULT hr = S_OK;
	ResourceManager* pResourceManager = pRenderer->GetResourceManager();

	// Create vertex buffer.
	if (MESH_INFO.SkinnedVertices.size() > 0)
	{
		hr = pResourceManager->CreateVertexBuffer(sizeof(SkinnedVertex),
												  (UINT)MESH_INFO.SkinnedVertices.size(),
												  &pNewMesh->Vertex.VertexBufferView,
												  &pNewMesh->Vertex.pBuffer,
												  (void*)MESH_INFO.SkinnedVertices.data());
		BREAK_IF_FAILED(hr);
		pNewMesh->Vertex.Count = (UINT)MESH_INFO.SkinnedVertices.size();
	}
	else
	{
		hr = pResourceManager->CreateVertexBuffer(sizeof(Vertex),
												  (UINT)MESH_INFO.Vertices.size(),
												  &pNewMesh->Vertex.VertexBufferView,
												  &pNewMesh->Vertex.pBuffer,
												  (void*)MESH_INFO.Vertices.data());
		BREAK_IF_FAILED(hr);
		pNewMesh->Vertex.Count = (UINT)MESH_INFO.Vertices.size();
	}

	// Create index buffer.
	hr = pResourceManager->CreateIndexBuffer(sizeof(UINT),
											 (UINT)MESH_INFO.Indices.size(),
											 &pNewMesh->Index.IndexBufferView,
											 &pNewMesh->Index.pBuffer,
											 (void*)MESH_INFO.Indices.data());
	BREAK_IF_FAILED(hr);
	pNewMesh->Index.Count = (UINT)MESH_INFO.Indices.size();
}

void SkinnedMeshModel::InitMeshBuffers(Renderer* pRenderer, const MeshInfo& MESH_INFO, Mesh** ppNewMesh)
{
	_ASSERT(pRenderer);

	HRESULT hr = S_OK;
	ResourceManager* pResourceManager = pRenderer->GetResourceManager();

	// Create vertex buffer.
	if (MESH_INFO.SkinnedVertices.size() > 0)
	{
		hr = pResourceManager->CreateVertexBuffer(sizeof(SkinnedVertex),
												  (UINT)MESH_INFO.SkinnedVertices.size(),
												  &(*ppNewMesh)->Vertex.VertexBufferView,
												  &(*ppNewMesh)->Vertex.pBuffer,
												  (void*)MESH_INFO.SkinnedVertices.data());
		BREAK_IF_FAILED(hr);
		(*ppNewMesh)->Vertex.Count = (UINT)MESH_INFO.SkinnedVertices.size();
	}
	else
	{
		hr = pResourceManager->CreateVertexBuffer(sizeof(Vertex),
												  (UINT)MESH_INFO.Vertices.size(),
												  &(*ppNewMesh)->Vertex.VertexBufferView,
												  &(*ppNewMesh)->Vertex.pBuffer,
												  (void*)MESH_INFO.Vertices.data());
		BREAK_IF_FAILED(hr);
		(*ppNewMesh)->Vertex.Count = (UINT)MESH_INFO.Vertices.size();
	}

	// Create index buffer.
	hr = pResourceManager->CreateIndexBuffer(sizeof(UINT),
											 (UINT)MESH_INFO.Indices.size(),
											 &(*ppNewMesh)->Index.IndexBufferView,
											 &(*ppNewMesh)->Index.pBuffer,
											 (void*)MESH_INFO.Indices.data());
	BREAK_IF_FAILED(hr);
	(*ppNewMesh)->Index.Count = (UINT)MESH_INFO.Indices.size();
}

void SkinnedMeshModel::InitAnimationData(Renderer* pRenderer, const AnimationData& ANIM_DATA)
{
	if (ANIM_DATA.Clips.empty())
	{
		return;
	}

	CharacterAnimationData = ANIM_DATA;

	// 여기서는 AnimationClip이 SkinnedMesh라고 가정.
	// ANIM_DATA.Clips[0].Keys.size() -> 뼈의 수.

	pBoneTransform = pRenderer->GetTextureManager()->CreateNonImageTexture((UINT)ANIM_DATA.Clips[0].Keys.size(), sizeof(Matrix));

	// 단위행렬로 초기화.
	CD3DX12_RANGE writeRange(0, 0);
	BYTE* pBoneTransformMem = nullptr;
	pBoneTransform->pTextureResource->Map(0, &writeRange, (void**)&pBoneTransformMem);

	Matrix* pDest = (Matrix*)pBoneTransformMem;
	for (UINT64 i = 0, size = ANIM_DATA.Clips[0].Keys.size(); i < size; ++i)
	{
		pDest[i] = Matrix();
	}

	pBoneTransform->pTextureResource->Unmap(0, nullptr);
}

void SkinnedMeshModel::UpdateWorld(const Matrix& WORLD)
{
	World = WORLD;
	// InverseWorldTranspose.Translation(Vector3(0.0f));
	InverseWorldTranspose = WORLD.Invert().Transpose();

	for (UINT64 i = 0, size = Meshes.size(); i < size; ++i)
	{
		Mesh* pCurMesh = Meshes[i];
		MeshConstant& meshConstantData = pCurMesh->MeshConstantData;

		meshConstantData.World = World.Transpose();
		meshConstantData.InverseWorldTranspose = InverseWorldTranspose.Transpose();
		meshConstantData.InverseWorld = meshConstantData.InverseWorldTranspose.Transpose();
	}
}

void SkinnedMeshModel::UpdateAnimation(const int CLIP_ID, const int FRAME, const float DELTA_TIME, JointUpdateInfo* pUpdateInfo)
{
	if (!bIsVisible)
	{
		return;
	}

	// 입력에 따른 변환행렬 업데이트.
	// CharacterAnimationData.Update(clipID, frame);

	// IK 계산. 서 있을 때만.
	if (CLIP_ID == 0)
	{
		solveCharacterIK(CLIP_ID, FRAME, DELTA_TIME, pUpdateInfo);
		CharacterAnimationData.Update(CLIP_ID, FRAME);
	}

	// joint 위치 갱신.
	updateChainPosition(CLIP_ID, FRAME);

	// Update bone transform buffer.
	BYTE* pBoneTransformMem = nullptr;
	CD3DX12_RANGE writeRange(0, 0);
	pBoneTransform->pTextureResource->Map(0, &writeRange, (void**)&pBoneTransformMem);

	Matrix* pDest = (Matrix*)pBoneTransformMem;
	// pDest[0] = (CharacterAnimationData.InverseDefaultTransform * CharacterAnimationData.OffsetMatrices[0] * CharacterAnimationData.BoneTransforms[0] * CharacterAnimationData.InverseOffsetMatrices[0] * CharacterAnimationData.DefaultTransform * CharacterAnimationData.AccumulatedRootTransform).Transpose();
	for (UINT64 i = 0, size = CharacterAnimationData.Clips[CLIP_ID].Keys.size(); i < size; ++i)
	{
		pDest[i] = CharacterAnimationData.Get((int)i).Transpose();
	}

	pBoneTransform->pTextureResource->Unmap(0, nullptr);

	updateJointSpheres(CLIP_ID, FRAME);
}

void SkinnedMeshModel::Render(eRenderPSOType psoSetting)
{
	_ASSERT(m_pRenderer);

	if (!bIsVisible)
	{
		return;
	}

	HRESULT hr = S_OK;
	ResourceManager* pResourceManager = m_pRenderer->GetResourceManager();
	ConstantBufferManager* pConstantBufferManager = m_pRenderer->GetConstantBufferManager();

	ID3D12Device5* pDevice = m_pRenderer->GetD3DDevice();
	ID3D12GraphicsCommandList* pCommandList = m_pRenderer->GetCommandList();
	DynamicDescriptorPool* pDynamicDescriptorPool = m_pRenderer->GetDynamicDescriptorPool();
	ConstantBufferPool* pMeshConstantBufferPool = pConstantBufferManager->GetConstantBufferPool(ConstantBufferType_Mesh);
	ConstantBufferPool* pMaterialConstantBufferPool = pConstantBufferManager->GetConstantBufferPool(ConstantBufferType_Material);
	const UINT CBV_SRV_DESCRIPTOR_SIZE = pResourceManager->CBVSRVUAVDescriptorSize;

	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuDescriptorTable = {};
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuDescriptorTable = {};

	for (UINT64 i = 0, size = Meshes.size(); i < size; ++i)
	{
		Mesh* const pCurMesh = Meshes[i];

		Material* pMeshMaterialTextures = &pCurMesh->Material;
		MeshConstant* pMeshConstantData = &pCurMesh->MeshConstantData;
		MaterialConstant* pMaterialConstantData = &pCurMesh->MaterialConstantData;
		CBInfo* pMeshCB = pMeshConstantBufferPool->AllocCB();
		CBInfo* pMaterialCB = pMaterialConstantBufferPool->AllocCB();

		// Upload constant buffer(mesh, material).
		BYTE* pMeshConstMem = pMeshCB->pSystemMemAddr;
		BYTE* pMaterialConstMem = pMaterialCB->pSystemMemAddr;
		memcpy(pMeshConstMem, &pCurMesh->MeshConstantData, sizeof(pCurMesh->MeshConstantData));
		memcpy(pMaterialConstMem, &pCurMesh->MaterialConstantData, sizeof(pCurMesh->MaterialConstantData));

		switch (psoSetting)
		{
			case RenderPSOType_Skinned:
			case RenderPSOType_ReflectionSkinned:
			{
				hr = pDynamicDescriptorPool->AllocDescriptorTable(&cpuDescriptorTable, &gpuDescriptorTable, 10);
				BREAK_IF_FAILED(hr);

				CD3DX12_CPU_DESCRIPTOR_HANDLE dstHandle(cpuDescriptorTable, 0, CBV_SRV_DESCRIPTOR_SIZE);

				// t7
				pDevice->CopyDescriptorsSimple(1, dstHandle, pBoneTransform->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);

				// b2, b3
				pDevice->CopyDescriptorsSimple(1, dstHandle, pMeshCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);
				pDevice->CopyDescriptorsSimple(1, dstHandle, pMaterialCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);

				// t0 ~ t5
				if (pMeshMaterialTextures->pAlbedo)
				{
					pDevice->CopyDescriptorsSimple(1, dstHandle, pMeshMaterialTextures->pAlbedo->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				}
				else
				{
					pDevice->CopyDescriptorsSimple(1, dstHandle, pResourceManager->NullSRVDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				}
				dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);

				if (pMeshMaterialTextures->pEmissive)
				{
					pDevice->CopyDescriptorsSimple(1, dstHandle, pMeshMaterialTextures->pEmissive->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				}
				else
				{
					pDevice->CopyDescriptorsSimple(1, dstHandle, pResourceManager->NullSRVDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				}
				dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);

				if (pMeshMaterialTextures->pNormal)
				{
					pDevice->CopyDescriptorsSimple(1, dstHandle, pMeshMaterialTextures->pNormal->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				}
				else
				{
					pDevice->CopyDescriptorsSimple(1, dstHandle, pResourceManager->NullSRVDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				}
				dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);

				if (pMeshMaterialTextures->pAmbientOcclusion)
				{
					pDevice->CopyDescriptorsSimple(1, dstHandle, pMeshMaterialTextures->pAmbientOcclusion->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				}
				else
				{
					pDevice->CopyDescriptorsSimple(1, dstHandle, pResourceManager->NullSRVDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				}
				dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);

				if (pMeshMaterialTextures->pMetallic)
				{
					pDevice->CopyDescriptorsSimple(1, dstHandle, pMeshMaterialTextures->pMetallic->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				}
				else
				{
					pDevice->CopyDescriptorsSimple(1, dstHandle, pResourceManager->NullSRVDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				}
				dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);

				if (pMeshMaterialTextures->pRoughness)
				{
					pDevice->CopyDescriptorsSimple(1, dstHandle, pMeshMaterialTextures->pRoughness->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				}
				else
				{
					pDevice->CopyDescriptorsSimple(1, dstHandle, pResourceManager->NullSRVDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				}
				dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);

				// t6
				if (pMeshMaterialTextures->pHeight)
				{
					pDevice->CopyDescriptorsSimple(1, dstHandle, pMeshMaterialTextures->pHeight->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				}
				else
				{
					pDevice->CopyDescriptorsSimple(1, dstHandle, pResourceManager->NullSRVDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				}

				pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable);

			}
			break;

			case RenderPSOType_DepthOnlySkinned:
			case RenderPSOType_DepthOnlyCubeSkinned:
			case RenderPSOType_DepthOnlyCascadeSkinned:
			{
				hr = pDynamicDescriptorPool->AllocDescriptorTable(&cpuDescriptorTable, &gpuDescriptorTable, 3);
				BREAK_IF_FAILED(hr);

				CD3DX12_CPU_DESCRIPTOR_HANDLE dstHandle(cpuDescriptorTable, 0, CBV_SRV_DESCRIPTOR_SIZE);

				// t7
				pDevice->CopyDescriptorsSimple(1, dstHandle, pBoneTransform->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);

				// b2, b3
				pDevice->CopyDescriptorsSimple(1, dstHandle, pMeshCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);
				pDevice->CopyDescriptorsSimple(1, dstHandle, pMaterialCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);

				pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable);
			}
			break;

			default:
				__debugbreak();
				break;
		}

		pCommandList->IASetVertexBuffers(0, 1, &pCurMesh->Vertex.VertexBufferView);
		pCommandList->IASetIndexBuffer(&pCurMesh->Index.IndexBufferView);
		pCommandList->DrawIndexedInstanced(pCurMesh->Index.Count, 1, 0, 0, 0);
	}
}

void SkinnedMeshModel::Render(UINT threadIndex, ID3D12GraphicsCommandList* pCommandList, DynamicDescriptorPool* pDescriptorPool, ConstantBufferManager* pConstantBufferManager, ResourceManager* pManager, int psoSetting)
{
	_ASSERT(pCommandList);
	_ASSERT(pManager);
	_ASSERT(pDescriptorPool);
	_ASSERT(pConstantBufferManager);

	if (!bIsVisible)
	{
		return;
	}

	HRESULT hr = S_OK;

	ID3D12Device5* pDevice = m_pRenderer->GetD3DDevice();
	ConstantBufferPool* pMeshConstantBufferPool = pConstantBufferManager->GetConstantBufferPool(ConstantBufferType_Mesh);
	ConstantBufferPool* pMaterialConstantBufferPool = pConstantBufferManager->GetConstantBufferPool(ConstantBufferType_Material);
	const UINT CBV_SRV_DESCRIPTOR_SIZE = pManager->CBVSRVUAVDescriptorSize;

	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuDescriptorTable = {};
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuDescriptorTable = {};

	for (UINT64 i = 0, size = Meshes.size(); i < size; ++i)
	{
		Mesh* pCurMesh = Meshes[i];

		Material* pMeshMaterialTextures = &pCurMesh->Material;
		MeshConstant* pMeshConstantData = &pCurMesh->MeshConstantData;
		MaterialConstant* pMaterialConstantData = &pCurMesh->MaterialConstantData;
		CBInfo* pMeshCB = pMeshConstantBufferPool->AllocCB();
		CBInfo* pMaterialCB = pMaterialConstantBufferPool->AllocCB();

		// Upload constant buffer(mesh, material).
		BYTE* pMeshConstMem = pMeshCB->pSystemMemAddr;
		BYTE* pMaterialConstMem = pMaterialCB->pSystemMemAddr;
		memcpy(pMeshConstMem, &pCurMesh->MeshConstantData, sizeof(pCurMesh->MeshConstantData));
		memcpy(pMaterialConstMem, &pCurMesh->MaterialConstantData, sizeof(pCurMesh->MaterialConstantData));


		switch (psoSetting)
		{
			case RenderPSOType_Skinned:
			case RenderPSOType_ReflectionSkinned:
			{
				hr = pDescriptorPool->AllocDescriptorTable(&cpuDescriptorTable, &gpuDescriptorTable, 10);
				BREAK_IF_FAILED(hr);

				CD3DX12_CPU_DESCRIPTOR_HANDLE dstHandle(cpuDescriptorTable, 0, CBV_SRV_DESCRIPTOR_SIZE);

				// t7
				pDevice->CopyDescriptorsSimple(1, dstHandle, pBoneTransform->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);

				// b2, b3
				pDevice->CopyDescriptorsSimple(1, dstHandle, pMeshCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);
				pDevice->CopyDescriptorsSimple(1, dstHandle, pMaterialCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);

				// t0 ~ t5
				if (pMeshMaterialTextures->pAlbedo)
				{
					pDevice->CopyDescriptorsSimple(1, dstHandle, pMeshMaterialTextures->pAlbedo->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				}
				else
				{
					pDevice->CopyDescriptorsSimple(1, dstHandle, pManager->NullSRVDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				}
				dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);

				if (pMeshMaterialTextures->pEmissive)
				{
					pDevice->CopyDescriptorsSimple(1, dstHandle, pMeshMaterialTextures->pEmissive->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				}
				else
				{
					pDevice->CopyDescriptorsSimple(1, dstHandle, pManager->NullSRVDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				}
				dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);

				if (pMeshMaterialTextures->pNormal)
				{
					pDevice->CopyDescriptorsSimple(1, dstHandle, pMeshMaterialTextures->pNormal->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				}
				else
				{
					pDevice->CopyDescriptorsSimple(1, dstHandle, pManager->NullSRVDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				}
				dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);

				if (pMeshMaterialTextures->pAmbientOcclusion)
				{
					pDevice->CopyDescriptorsSimple(1, dstHandle, pMeshMaterialTextures->pAmbientOcclusion->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				}
				else
				{
					pDevice->CopyDescriptorsSimple(1, dstHandle, pManager->NullSRVDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				}
				dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);

				if (pMeshMaterialTextures->pMetallic)
				{
					pDevice->CopyDescriptorsSimple(1, dstHandle, pMeshMaterialTextures->pMetallic->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				}
				else
				{
					pDevice->CopyDescriptorsSimple(1, dstHandle, pManager->NullSRVDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				}
				dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);

				if (pMeshMaterialTextures->pRoughness)
				{
					pDevice->CopyDescriptorsSimple(1, dstHandle, pMeshMaterialTextures->pRoughness->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				}
				else
				{
					pDevice->CopyDescriptorsSimple(1, dstHandle, pManager->NullSRVDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				}
				dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);

				// t6
				if (pMeshMaterialTextures->pHeight)
				{
					pDevice->CopyDescriptorsSimple(1, dstHandle, pMeshMaterialTextures->pHeight->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				}
				else
				{
					pDevice->CopyDescriptorsSimple(1, dstHandle, pManager->NullSRVDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				}

				pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable);

			}
			break;

			case RenderPSOType_DepthOnlySkinned:
			case RenderPSOType_DepthOnlyCubeSkinned:
			case RenderPSOType_DepthOnlyCascadeSkinned:
			{
				hr = pDescriptorPool->AllocDescriptorTable(&cpuDescriptorTable, &gpuDescriptorTable, 3);
				BREAK_IF_FAILED(hr);

				CD3DX12_CPU_DESCRIPTOR_HANDLE dstHandle(cpuDescriptorTable, 0, CBV_SRV_DESCRIPTOR_SIZE);

				// t7
				pDevice->CopyDescriptorsSimple(1, dstHandle, pBoneTransform->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);

				// b2, b3
				pDevice->CopyDescriptorsSimple(1, dstHandle, pMeshCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);
				pDevice->CopyDescriptorsSimple(1, dstHandle, pMaterialCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);

				pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable);
			}
			break;

			default:
				__debugbreak();
				break;
		}

		pCommandList->IASetVertexBuffers(0, 1, &pCurMesh->Vertex.VertexBufferView);
		pCommandList->IASetIndexBuffer(&pCurMesh->Index.IndexBufferView);
		pCommandList->DrawIndexedInstanced(pCurMesh->Index.Count, 1, 0, 0, 0);
	}
}

void SkinnedMeshModel::RenderBoundingCapsule(eRenderPSOType psoSetting)
{
	_ASSERT(m_pRenderer);

	HRESULT hr = S_OK;
	ResourceManager* pResourceManager = m_pRenderer->GetResourceManager();
	ConstantBufferManager* pConstantBufferManager = m_pRenderer->GetConstantBufferManager();

	ID3D12Device5* pDevice = m_pRenderer->GetD3DDevice();
	ID3D12GraphicsCommandList* pCommandList = m_pRenderer->GetCommandList();
	ID3D12DescriptorHeap* pCBVSRVHeap = m_pRenderer->GetSRVUAVAllocator()->GetDescriptorHeap();
	DynamicDescriptorPool* pDynamicDescriptorPool = m_pRenderer->GetDynamicDescriptorPool();
	ConstantBufferPool* pMeshConstantBufferPool = pConstantBufferManager->GetConstantBufferPool(ConstantBufferType_Mesh);
	ConstantBufferPool* pMaterialConstantBufferPool = pConstantBufferManager->GetConstantBufferPool(ConstantBufferType_Material);
	const UINT CBV_SRV_DESCRIPTOR_SIZE = pResourceManager->CBVSRVUAVDescriptorSize;


	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuDescriptorTable = {};
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuDescriptorTable = {};

	hr = pDynamicDescriptorPool->AllocDescriptorTable(&cpuDescriptorTable, &gpuDescriptorTable, 3);
	BREAK_IF_FAILED(hr);


	CD3DX12_CPU_DESCRIPTOR_HANDLE dstHandle(cpuDescriptorTable, 0, CBV_SRV_DESCRIPTOR_SIZE);

	MeshConstant* pMeshConstantData = &m_pBoundingBoxMesh->MeshConstantData;
	MaterialConstant* pMaterialConstantData = &m_pBoundingBoxMesh->MaterialConstantData;
	CBInfo* pMeshCB = pMeshConstantBufferPool->AllocCB();
	CBInfo* pMaterialCB = pMaterialConstantBufferPool->AllocCB();

	// Upload constant buffer(mesh, material).
	BYTE* pMeshConstMem = pMeshCB->pSystemMemAddr;
	BYTE* pMaterialConstMem = pMaterialCB->pSystemMemAddr;
	memcpy(pMeshConstMem, &m_pBoundingBoxMesh->MeshConstantData, sizeof(m_pBoundingBoxMesh->MeshConstantData));
	memcpy(pMaterialConstMem, &m_pBoundingBoxMesh->MaterialConstantData, sizeof(m_pBoundingBoxMesh->MaterialConstantData));


	// b2, b3
	pDevice->CopyDescriptorsSimple(1, dstHandle, pMeshCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);
	pDevice->CopyDescriptorsSimple(1, dstHandle, pMaterialCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);

	// t6(null)
	pDevice->CopyDescriptorsSimple(1, dstHandle, pResourceManager->NullSRVDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable);

	pCommandList->IASetVertexBuffers(0, 1, &m_pBoundingCapsuleMesh->Vertex.VertexBufferView);
	pCommandList->IASetIndexBuffer(&m_pBoundingCapsuleMesh->Index.IndexBufferView);
	pCommandList->DrawIndexedInstanced(m_pBoundingCapsuleMesh->Index.Count, 1, 0, 0, 0);
}

void SkinnedMeshModel::RenderJointSphere(eRenderPSOType psoSetting)
{
	_ASSERT(m_pRenderer);

	HRESULT hr = S_OK;
	ResourceManager* pResourceManager = m_pRenderer->GetResourceManager();
	ConstantBufferManager* pConstantBufferManager = m_pRenderer->GetConstantBufferManager();

	ID3D12Device5* pDevice = m_pRenderer->GetD3DDevice();
	ID3D12GraphicsCommandList* pCommandList = m_pRenderer->GetCommandList();
	ID3D12DescriptorHeap* pCBVSRVHeap = m_pRenderer->GetSRVUAVAllocator()->GetDescriptorHeap();
	DynamicDescriptorPool* pDynamicDescriptorPool = m_pRenderer->GetDynamicDescriptorPool();
	ConstantBufferPool* pMeshConstantBufferPool = pConstantBufferManager->GetConstantBufferPool(ConstantBufferType_Mesh);
	ConstantBufferPool* pMaterialConstantBufferPool = pConstantBufferManager->GetConstantBufferPool(ConstantBufferType_Material);
	const UINT CBV_SRV_DESCRIPTOR_SIZE = pResourceManager->CBVSRVUAVDescriptorSize;

	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuDescriptorTable[18];
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuDescriptorTable[18];

	for (int i = 0; i < 18; ++i)
	{
		hr = pDynamicDescriptorPool->AllocDescriptorTable(&cpuDescriptorTable[i], &gpuDescriptorTable[i], 3);
		BREAK_IF_FAILED(hr);
	}

	// render all chain spheres.
	int descriptorTableIndex = 0;
	for (int i = 0; i < 4; ++i)
	{
		Mesh* pPartOfArm = m_ppRightArm[i];
		CD3DX12_CPU_DESCRIPTOR_HANDLE& descriptorHandle = cpuDescriptorTable[descriptorTableIndex];

		MeshConstant* pMeshConstantData = &pPartOfArm->MeshConstantData;
		MaterialConstant* pMaterialConstantData = &pPartOfArm->MaterialConstantData;
		CBInfo* pMeshCB = pMeshConstantBufferPool->AllocCB();
		CBInfo* pMaterialCB = pMaterialConstantBufferPool->AllocCB();

		// Upload constant buffer(mesh, material).
		BYTE* pMeshConstMem = pMeshCB->pSystemMemAddr;
		BYTE* pMaterialConstMem = pMaterialCB->pSystemMemAddr;
		memcpy(pMeshConstMem, &pPartOfArm->MeshConstantData, sizeof(pPartOfArm->MeshConstantData));
		memcpy(pMaterialConstMem, &pPartOfArm->MaterialConstantData, sizeof(pPartOfArm->MaterialConstantData));

		// b2, b3
		pDevice->CopyDescriptorsSimple(1, descriptorHandle, pMeshCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		descriptorHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);
		pDevice->CopyDescriptorsSimple(1, descriptorHandle, pMaterialCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		descriptorHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);

		// t6(null)
		pDevice->CopyDescriptorsSimple(1, descriptorHandle, pResourceManager->NullSRVDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable[descriptorTableIndex]);

		pCommandList->IASetVertexBuffers(0, 1, &pPartOfArm->Vertex.VertexBufferView);
		pCommandList->IASetIndexBuffer(&pPartOfArm->Index.IndexBufferView);
		pCommandList->DrawIndexedInstanced(pPartOfArm->Index.Count, 1, 0, 0, 0);
		++descriptorTableIndex;
	}
	for (int i = 0; i < 4; ++i)
	{
		Mesh* pPartOfArm = m_ppLeftArm[i];
		CD3DX12_CPU_DESCRIPTOR_HANDLE& descriptorHandle = cpuDescriptorTable[descriptorTableIndex];

		MeshConstant* pMeshConstantData = &pPartOfArm->MeshConstantData;
		MaterialConstant* pMaterialConstantData = &pPartOfArm->MaterialConstantData;
		CBInfo* pMeshCB = pMeshConstantBufferPool->AllocCB();
		CBInfo* pMaterialCB = pMaterialConstantBufferPool->AllocCB();

		// Upload constant buffer(mesh, material).
		BYTE* pMeshConstMem = pMeshCB->pSystemMemAddr;
		BYTE* pMaterialConstMem = pMaterialCB->pSystemMemAddr;
		memcpy(pMeshConstMem, &pPartOfArm->MeshConstantData, sizeof(pPartOfArm->MeshConstantData));
		memcpy(pMaterialConstMem, &pPartOfArm->MaterialConstantData, sizeof(pPartOfArm->MaterialConstantData));

		// b2, b3
		pDevice->CopyDescriptorsSimple(1, descriptorHandle, pMeshCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		descriptorHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);
		pDevice->CopyDescriptorsSimple(1, descriptorHandle, pMaterialCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		descriptorHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);

		// t6(null)
		pDevice->CopyDescriptorsSimple(1, descriptorHandle, pResourceManager->NullSRVDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable[descriptorTableIndex]);

		pCommandList->IASetVertexBuffers(0, 1, &pPartOfArm->Vertex.VertexBufferView);
		pCommandList->IASetIndexBuffer(&pPartOfArm->Index.IndexBufferView);
		pCommandList->DrawIndexedInstanced(pPartOfArm->Index.Count, 1, 0, 0, 0);
		++descriptorTableIndex;
	}
	for (int i = 0; i < 4; ++i)
	{
		Mesh* pPartOfLeg = m_ppRightLeg[i];
		CD3DX12_CPU_DESCRIPTOR_HANDLE& descriptorHandle = cpuDescriptorTable[descriptorTableIndex];

		MeshConstant* pMeshConstantData = &pPartOfLeg->MeshConstantData;
		MaterialConstant* pMaterialConstantData = &pPartOfLeg->MaterialConstantData;
		CBInfo* pMeshCB = pMeshConstantBufferPool->AllocCB();
		CBInfo* pMaterialCB = pMaterialConstantBufferPool->AllocCB();

		// Upload constant buffer(mesh, material).
		BYTE* pMeshConstMem = pMeshCB->pSystemMemAddr;
		BYTE* pMaterialConstMem = pMaterialCB->pSystemMemAddr;
		memcpy(pMeshConstMem, &pPartOfLeg->MeshConstantData, sizeof(pPartOfLeg->MeshConstantData));
		memcpy(pMaterialConstMem, &pPartOfLeg->MaterialConstantData, sizeof(pPartOfLeg->MaterialConstantData));

		// b2, b3
		pDevice->CopyDescriptorsSimple(1, descriptorHandle, pMeshCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		descriptorHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);
		pDevice->CopyDescriptorsSimple(1, descriptorHandle, pMaterialCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		descriptorHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);

		// t6(null)
		pDevice->CopyDescriptorsSimple(1, descriptorHandle, pResourceManager->NullSRVDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable[descriptorTableIndex]);

		pCommandList->IASetVertexBuffers(0, 1, &pPartOfLeg->Vertex.VertexBufferView);
		pCommandList->IASetIndexBuffer(&pPartOfLeg->Index.IndexBufferView);
		pCommandList->DrawIndexedInstanced(pPartOfLeg->Index.Count, 1, 0, 0, 0);
		++descriptorTableIndex;
	}
	for (int i = 0; i < 4; ++i)
	{
		Mesh* pPartOfLeg = m_ppLeftLeg[i];
		CD3DX12_CPU_DESCRIPTOR_HANDLE& descriptorHandle = cpuDescriptorTable[descriptorTableIndex];

		MeshConstant* pMeshConstantData = &pPartOfLeg->MeshConstantData;
		MaterialConstant* pMaterialConstantData = &pPartOfLeg->MaterialConstantData;
		CBInfo* pMeshCB = pMeshConstantBufferPool->AllocCB();
		CBInfo* pMaterialCB = pMaterialConstantBufferPool->AllocCB();

		// Upload constant buffer(mesh, material).
		BYTE* pMeshConstMem = pMeshCB->pSystemMemAddr;
		BYTE* pMaterialConstMem = pMaterialCB->pSystemMemAddr;
		memcpy(pMeshConstMem, &pPartOfLeg->MeshConstantData, sizeof(pPartOfLeg->MeshConstantData));
		memcpy(pMaterialConstMem, &pPartOfLeg->MaterialConstantData, sizeof(pPartOfLeg->MaterialConstantData));

		// b2, b3
		pDevice->CopyDescriptorsSimple(1, descriptorHandle, pMeshCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		descriptorHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);
		pDevice->CopyDescriptorsSimple(1, descriptorHandle, pMaterialCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		descriptorHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);

		// t6(null)
		pDevice->CopyDescriptorsSimple(1, descriptorHandle, pResourceManager->NullSRVDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable[descriptorTableIndex]);

		pCommandList->IASetVertexBuffers(0, 1, &pPartOfLeg->Vertex.VertexBufferView);
		pCommandList->IASetIndexBuffer(&pPartOfLeg->Index.IndexBufferView);
		pCommandList->DrawIndexedInstanced(pPartOfLeg->Index.Count, 1, 0, 0, 0);
		++descriptorTableIndex;
	}

	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE& descriptorHandle = cpuDescriptorTable[descriptorTableIndex];

		MeshConstant* pMeshConstantData = &m_pTargetPos1->MeshConstantData;
		MaterialConstant* pMaterialConstantData = &m_pTargetPos1->MaterialConstantData;
		CBInfo* pMeshCB = pMeshConstantBufferPool->AllocCB();
		CBInfo* pMaterialCB = pMaterialConstantBufferPool->AllocCB();

		// Upload constant buffer(mesh, material).
		BYTE* pMeshConstMem = pMeshCB->pSystemMemAddr;
		BYTE* pMaterialConstMem = pMaterialCB->pSystemMemAddr;
		memcpy(pMeshConstMem, &m_pTargetPos1->MeshConstantData, sizeof(m_pTargetPos1->MeshConstantData));
		memcpy(pMaterialConstMem, &m_pTargetPos1->MaterialConstantData, sizeof(m_pTargetPos1->MaterialConstantData));

		// b2, b3
		pDevice->CopyDescriptorsSimple(1, descriptorHandle, pMeshCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		descriptorHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);
		pDevice->CopyDescriptorsSimple(1, descriptorHandle, pMaterialCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		descriptorHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);

		// t6(null)
		pDevice->CopyDescriptorsSimple(1, descriptorHandle, pResourceManager->NullSRVDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable[descriptorTableIndex]);

		pCommandList->IASetVertexBuffers(0, 1, &m_pTargetPos1->Vertex.VertexBufferView);
		pCommandList->IASetIndexBuffer(&m_pTargetPos1->Index.IndexBufferView);
		pCommandList->DrawIndexedInstanced(m_pTargetPos1->Index.Count, 1, 0, 0, 0);
		++descriptorTableIndex;
	}
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE& descriptorHandle = cpuDescriptorTable[descriptorTableIndex];

		MeshConstant* pMeshConstantData = &m_pTargetPos2->MeshConstantData;
		MaterialConstant* pMaterialConstantData = &m_pTargetPos2->MaterialConstantData;
		CBInfo* pMeshCB = pMeshConstantBufferPool->AllocCB();
		CBInfo* pMaterialCB = pMaterialConstantBufferPool->AllocCB();

		// Upload constant buffer(mesh, material).
		BYTE* pMeshConstMem = pMeshCB->pSystemMemAddr;
		BYTE* pMaterialConstMem = pMaterialCB->pSystemMemAddr;
		memcpy(pMeshConstMem, &m_pTargetPos2->MeshConstantData, sizeof(m_pTargetPos2->MeshConstantData));
		memcpy(pMaterialConstMem, &m_pTargetPos2->MaterialConstantData, sizeof(m_pTargetPos2->MaterialConstantData));

		// b2, b3
		pDevice->CopyDescriptorsSimple(1, descriptorHandle, pMeshCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		descriptorHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);
		pDevice->CopyDescriptorsSimple(1, descriptorHandle, pMaterialCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		descriptorHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);

		// t6(null)
		pDevice->CopyDescriptorsSimple(1, descriptorHandle, pResourceManager->NullSRVDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable[descriptorTableIndex]);

		pCommandList->IASetVertexBuffers(0, 1, &m_pTargetPos2->Vertex.VertexBufferView);
		pCommandList->IASetIndexBuffer(&m_pTargetPos2->Index.IndexBufferView);
		pCommandList->DrawIndexedInstanced(m_pTargetPos2->Index.Count, 1, 0, 0, 0);
	}
}

void SkinnedMeshModel::Cleanup()
{
	if (!m_pRenderer)
	{
		return;
	}

	if (pBoneTransform)
	{
		TextureManager* pTextureManager = m_pRenderer->GetTextureManager();
		pTextureManager->DeleteTexture(pBoneTransform);
		pBoneTransform = nullptr;
	}

	for (int i = 0; i < 4; ++i)
	{
		if (m_ppRightArm[i])
		{
			delete m_ppRightArm[i];
			m_ppRightArm[i] = nullptr;
		}
		if (m_ppLeftArm[i])
		{
			delete m_ppLeftArm[i];
			m_ppLeftArm[i] = nullptr;
		}
		if (m_ppRightLeg[i])
		{
			delete m_ppRightLeg[i];
			m_ppRightLeg[i] = nullptr;
		}
		if (m_ppLeftLeg[i])
		{
			delete m_ppLeftLeg[i];
			m_ppLeftLeg[i] = nullptr;
		}
	}
	if (m_pBoundingCapsuleMesh)
	{
		delete m_pBoundingCapsuleMesh;
		m_pBoundingCapsuleMesh = nullptr;
	}
	if (m_pTargetPos1)
	{
		delete m_pTargetPos1;
		m_pTargetPos1 = nullptr;
	}
	if (m_pTargetPos2)
	{
		delete m_pTargetPos2;
		m_pTargetPos2 = nullptr;
	}
}

void SkinnedMeshModel::initBoundingCapsule()
{
	MeshInfo meshData = INIT_MESH_INFO;
	MakeWireCapsule(&meshData, Vector3(0.0f), 0.2f, BoundingSphere.Radius * 1.3f);

	m_pBoundingCapsuleMesh = new Mesh;
	m_pBoundingCapsuleMesh->Initialize();

	MeshConstant& meshConstantData = m_pBoundingCapsuleMesh->MeshConstantData;
	MaterialConstant& materialConstantData = m_pBoundingCapsuleMesh->MaterialConstantData;
	meshConstantData.World = Matrix();

	Model::InitMeshBuffers(m_pRenderer, meshData, m_pBoundingCapsuleMesh);
}

void SkinnedMeshModel::initJointSpheres()
{
	MeshInfo meshData = INIT_MESH_INFO;
	MakeWireSphere(&meshData, Vector3(0.0f), 0.02f);

	// for chain debugging.
	for (int i = 0; i < 4; ++i)
	{
		Mesh** ppRightArmPart = &m_ppRightArm[i];
		Mesh** ppLeftArmPart = &m_ppLeftArm[i];
		Mesh** ppRightLegPart = &m_ppRightLeg[i];
		Mesh** ppLeftLegPart = &m_ppLeftLeg[i];

		*ppRightArmPart = new Mesh;
		*ppLeftArmPart = new Mesh;
		*ppRightLegPart = new Mesh;
		*ppLeftLegPart = new Mesh;

		(*ppRightArmPart)->Initialize();
		(*ppLeftArmPart)->Initialize();
		(*ppRightLegPart)->Initialize();
		(*ppLeftLegPart)->Initialize();

		InitMeshBuffers(m_pRenderer, meshData, ppRightArmPart);
		InitMeshBuffers(m_pRenderer, meshData, ppLeftArmPart);
		InitMeshBuffers(m_pRenderer, meshData, ppRightLegPart);
		InitMeshBuffers(m_pRenderer, meshData, ppLeftLegPart);
	}

	m_pTargetPos1 = new Mesh;
	m_pTargetPos2 = new Mesh;
	m_pTargetPos1->Initialize();
	m_pTargetPos2->Initialize();
	InitMeshBuffers(m_pRenderer, meshData, &m_pTargetPos1);
	InitMeshBuffers(m_pRenderer, meshData, &m_pTargetPos2);
}

void SkinnedMeshModel::initChain()
{
	const float TO_RADIAN = DirectX::XM_PI / 180.0f;
	const char* BONE_NAME[16] =
	{
		// right arm
		"mixamorig:RightArm",
		"mixamorig:RightForeArm",
		"mixamorig:RightHand",
		"mixamorig:RightHandMiddle1",

		// left arm
		"mixamorig:LeftArm",
		"mixamorig:LeftForeArm",
		"mixamorig:LeftHand",
		"mixamorig:LeftHandMiddle1",

		// right leg
		"mixamorig:RightUpLeg",
		"mixamorig:RightLeg",
		"mixamorig:RightFoot",
		"mixamorig:RightToeBase",

		// left leg
		"mixamorig:LeftUpLeg",
		"mixamorig:LeftLeg",
		"mixamorig:LeftFoot",
		"mixamorig:LeftToeBase",
	};
	const Matrix BONE_CORRECTION_TRANSFORM[16] =
	{
		// right arm.
		Matrix::CreateTranslation(Vector3(0.085f, 0.33f, 0.06f)),
		Matrix::CreateTranslation(Vector3(-0.04f, 0.32f, 0.06f)),
		Matrix::CreateTranslation(Vector3(-0.18f, 0.32f, 0.06f)),
		Matrix::CreateTranslation(Vector3(-0.235f, 0.32f, 0.055f)),

		// left arm.
		Matrix::CreateTranslation(Vector3(0.32f, 0.34f, 0.05f)),
		Matrix::CreateTranslation(Vector3(0.45f, 0.32f, 0.05f)),
		Matrix::CreateTranslation(Vector3(0.59f, 0.32f, 0.05f)),
		Matrix::CreateTranslation(Vector3(0.65f, 0.32f, 0.05f)),

		// right leg.
		Matrix::CreateTranslation(Vector3(0.16f, 0.02f, 0.04f)),
		Matrix::CreateTranslation(Vector3(0.15f, -0.17f, 0.04f)),
		Matrix::CreateTranslation(Vector3(0.16f, -0.39f, 0.05f)),
		Matrix::CreateTranslation(Vector3(0.15f, -0.42f, 0.0f)),

		// left leg.
		Matrix::CreateTranslation(Vector3(0.26f, 0.025f, 0.05f)),
		Matrix::CreateTranslation(Vector3(0.26f, -0.165f, 0.05f)),
		Matrix::CreateTranslation(Vector3(0.25f, -0.38f, 0.05f)),
		Matrix::CreateTranslation(Vector3(0.26f, -0.42f, 0.0f)),
	};
	const Vector2 ANGLE_LIMITATION[16][3] =
	{
		// right arm
		{ Vector2(-1.0f, 1.0f), Vector2(0.0f), Vector2(-57.0f * TO_RADIAN, 85.0f * TO_RADIAN) },
		{ Vector2(0.0f), Vector2(0.0f), Vector2(0.0f, 85.0f * TO_RADIAN) },
		{ Vector2(-75.0f * TO_RADIAN, 75.0f * TO_RADIAN), Vector2(0.0f), Vector2(-25.0f * TO_RADIAN, 25.0f * TO_RADIAN) },
		{ Vector2(0.0f), Vector2(0.0f), Vector2(0.0f) },

		// left arm
		{ Vector2(0.0f), Vector2(0.0f), Vector2(-85.0f * TO_RADIAN, 57.0f * TO_RADIAN) },
		{ Vector2(0.0f), Vector2(0.0f), Vector2(-85.0f * TO_RADIAN, 0.0f) },
		{ Vector2(-75.0f * TO_RADIAN, 75.0f * TO_RADIAN), Vector2(0.0f), Vector2(-25.0f * TO_RADIAN, 25.0f * TO_RADIAN) },
		{ Vector2(0.0f), Vector2(0.0f), Vector2(0.0f) },

		// right leg
		{ Vector2(-58.8f * TO_RADIAN, 70.3f * TO_RADIAN), Vector2(0.0f), Vector2(-10.0f * TO_RADIAN, 52.0f * TO_RADIAN) },
		{ Vector2(-89.0f * TO_RADIAN, 0.0f), Vector2(0.0f), Vector2(0.0f) },
		{ Vector2(-15.0f * TO_RADIAN, 10.0f * TO_RADIAN), Vector2(0.0f), Vector2(0.0f) },
		{ Vector2(0.0f), Vector2(0.0f), Vector2(0.0f) },

		// left leg
		{ Vector2(-58.8f * TO_RADIAN, 70.3f * TO_RADIAN), Vector2(0.0f), Vector2(-52.0f * TO_RADIAN, 10.0f * TO_RADIAN) },
		{ Vector2(-89.0f * TO_RADIAN, 0.0f), Vector2(0.0f), Vector2(0.0f) },
		{ Vector2(-15.0f * TO_RADIAN, 10.0f * TO_RADIAN), Vector2(0.0f), Vector2(0.0f) },
		{ Vector2(0.0f), Vector2(0.0f), Vector2(0.0f) },
	};

	RightArm.BodyChain.resize(4);
	RightArm.pAnimationClips = &CharacterAnimationData.Clips;
	RightArm.DefaultTransform = CharacterAnimationData.DefaultTransform;
	RightArm.InverseDefaultTransform = CharacterAnimationData.InverseDefaultTransform;

	LeftArm.BodyChain.resize(4);
	LeftArm.pAnimationClips = &CharacterAnimationData.Clips;
	LeftArm.DefaultTransform = CharacterAnimationData.DefaultTransform;
	LeftArm.InverseDefaultTransform = CharacterAnimationData.InverseDefaultTransform;

	RightLeg.BodyChain.resize(4);
	RightLeg.pAnimationClips = &CharacterAnimationData.Clips;
	RightLeg.DefaultTransform = CharacterAnimationData.DefaultTransform;
	RightLeg.InverseDefaultTransform = CharacterAnimationData.InverseDefaultTransform;

	LeftLeg.BodyChain.resize(4);
	LeftLeg.pAnimationClips = &CharacterAnimationData.Clips;
	LeftLeg.DefaultTransform = CharacterAnimationData.DefaultTransform;
	LeftLeg.InverseDefaultTransform = CharacterAnimationData.InverseDefaultTransform;

	int boneNameIndex = 0;
	// right arm.
	for (int i = 0; i < 4; ++i)
	{
		const UINT BONE_ID = CharacterAnimationData.BoneNameToID[BONE_NAME[boneNameIndex]];
		const UINT BONE_PARENT_ID = CharacterAnimationData.BoneParents[BONE_ID];
		Joint* pJoint = &RightArm.BodyChain[i];

		pJoint->BoneID = BONE_ID;
		pJoint->AngleLimitation[Joint::JointAxis_X] = ANGLE_LIMITATION[boneNameIndex][Joint::JointAxis_X];
		pJoint->AngleLimitation[Joint::JointAxis_Y] = ANGLE_LIMITATION[boneNameIndex][Joint::JointAxis_Y];
		pJoint->AngleLimitation[Joint::JointAxis_Z] = ANGLE_LIMITATION[boneNameIndex][Joint::JointAxis_Z];
		pJoint->pOffset = &CharacterAnimationData.OffsetMatrices[BONE_ID];
		pJoint->pParentMatrix = &CharacterAnimationData.BoneTransforms[BONE_PARENT_ID];
		pJoint->pJointTransform = &CharacterAnimationData.BoneTransforms[BONE_ID];
		pJoint->Correction = BONE_CORRECTION_TRANSFORM[boneNameIndex];
		pJoint->CharacterWorld = World;

		++boneNameIndex;
	}
	// left arm.
	for (int i = 0; i < 4; ++i)
	{
		const UINT BONE_ID = CharacterAnimationData.BoneNameToID[BONE_NAME[boneNameIndex]];
		const UINT BONE_PARENT_ID = CharacterAnimationData.BoneParents[BONE_ID];
		Joint* pJoint = &LeftArm.BodyChain[i];

		pJoint->BoneID = BONE_ID;
		pJoint->AngleLimitation[Joint::JointAxis_X] = ANGLE_LIMITATION[boneNameIndex][Joint::JointAxis_X];
		pJoint->AngleLimitation[Joint::JointAxis_Y] = ANGLE_LIMITATION[boneNameIndex][Joint::JointAxis_Y];
		pJoint->AngleLimitation[Joint::JointAxis_Z] = ANGLE_LIMITATION[boneNameIndex][Joint::JointAxis_Z];
		pJoint->pOffset = &CharacterAnimationData.OffsetMatrices[BONE_ID];
		pJoint->pParentMatrix = &CharacterAnimationData.BoneTransforms[BONE_PARENT_ID];
		pJoint->pJointTransform = &CharacterAnimationData.BoneTransforms[BONE_ID];
		pJoint->Correction = BONE_CORRECTION_TRANSFORM[boneNameIndex];
		pJoint->CharacterWorld = World;

		++boneNameIndex;
	}
	// right leg.
	for (int i = 0; i < 4; ++i)
	{
		const UINT BONE_ID = CharacterAnimationData.BoneNameToID[BONE_NAME[boneNameIndex]];
		const UINT BONE_PARENT_ID = CharacterAnimationData.BoneParents[BONE_ID];
		Joint* pJoint = &RightLeg.BodyChain[i];

		pJoint->BoneID = BONE_ID;
		pJoint->AngleLimitation[Joint::JointAxis_X] = ANGLE_LIMITATION[boneNameIndex][Joint::JointAxis_X];
		pJoint->AngleLimitation[Joint::JointAxis_Y] = ANGLE_LIMITATION[boneNameIndex][Joint::JointAxis_Y];
		pJoint->AngleLimitation[Joint::JointAxis_Z] = ANGLE_LIMITATION[boneNameIndex][Joint::JointAxis_Z];
		pJoint->pOffset = &CharacterAnimationData.OffsetMatrices[BONE_ID];
		pJoint->pParentMatrix = &CharacterAnimationData.BoneTransforms[BONE_PARENT_ID];
		pJoint->pJointTransform = &CharacterAnimationData.BoneTransforms[BONE_ID];
		pJoint->Correction = BONE_CORRECTION_TRANSFORM[boneNameIndex];
		pJoint->CharacterWorld = World;

		++boneNameIndex;
	}
	// left leg.
	for (int i = 0; i < 4; ++i)
	{
		const UINT BONE_ID = CharacterAnimationData.BoneNameToID[BONE_NAME[boneNameIndex]];
		const UINT BONE_PARENT_ID = CharacterAnimationData.BoneParents[BONE_ID];
		Joint* pJoint = &LeftLeg.BodyChain[i];

		pJoint->BoneID = BONE_ID;
		pJoint->AngleLimitation[Joint::JointAxis_X] = ANGLE_LIMITATION[boneNameIndex][Joint::JointAxis_X];
		pJoint->AngleLimitation[Joint::JointAxis_Y] = ANGLE_LIMITATION[boneNameIndex][Joint::JointAxis_Y];
		pJoint->AngleLimitation[Joint::JointAxis_Z] = ANGLE_LIMITATION[boneNameIndex][Joint::JointAxis_Z];
		pJoint->pOffset = &CharacterAnimationData.OffsetMatrices[BONE_ID];
		pJoint->pParentMatrix = &CharacterAnimationData.BoneTransforms[BONE_PARENT_ID];
		pJoint->pJointTransform = &CharacterAnimationData.BoneTransforms[BONE_ID];
		pJoint->Correction = BONE_CORRECTION_TRANSFORM[boneNameIndex];
		pJoint->CharacterWorld = World;

		++boneNameIndex;
	}
}

void SkinnedMeshModel::updateChainPosition(const int CLIP_ID, const int FRAME)
{
	for (int i = 0; i < 4; ++i)
	{
		Joint* pRightArmPart = &RightArm.BodyChain[i];
		Joint* pLeftArmPart = &LeftArm.BodyChain[i];
		Joint* pRightLegPart = &RightLeg.BodyChain[i];
		Joint* pLeftLegPart = &LeftLeg.BodyChain[i];

		pRightArmPart->Position = (CharacterAnimationData.GetGlobalBonePositionMatix(CLIP_ID, FRAME, pRightArmPart->BoneID) * World).Translation();
		pLeftArmPart->Position = (CharacterAnimationData.GetGlobalBonePositionMatix(CLIP_ID, FRAME, pLeftArmPart->BoneID) * World).Translation();
		pRightLegPart->Position = (CharacterAnimationData.GetGlobalBonePositionMatix(CLIP_ID, FRAME, pRightLegPart->BoneID) * World).Translation();
		pLeftLegPart->Position = (CharacterAnimationData.GetGlobalBonePositionMatix(CLIP_ID, FRAME, pLeftLegPart->BoneID) * World).Translation();
	}
}

void SkinnedMeshModel::updateJointSpheres(const int CLIP_ID, const int FRAME)
{
	const Matrix CHARACTER_POS = Matrix::CreateTranslation(CharacterAnimationData.Position);
	const int ROOT_BONE_ID = 0;
	const Matrix ROOT_BONE_TRANSFORM = CharacterAnimationData.GetRootBoneTransformWithoutLocalRot(CLIP_ID, FRAME);

	m_pBoundingBoxMesh->MeshConstantData.World = (ROOT_BONE_TRANSFORM * CHARACTER_POS).Transpose();
	m_pBoundingSphereMesh->MeshConstantData.World = m_pBoundingBoxMesh->MeshConstantData.World;
	m_pBoundingCapsuleMesh->MeshConstantData.World = m_pBoundingBoxMesh->MeshConstantData.World;
	BoundingBox.Center = m_pBoundingBoxMesh->MeshConstantData.World.Transpose().Translation();
	BoundingSphere.Center = BoundingBox.Center;
	/*{
		Matrix rootWorld = CharacterAnimationData.OffsetMatrices[0] * CharacterAnimationData.BoneTransforms[0];
		Vector3 pos = rootWorld.Translation();

		char szDebugString[256];
		sprintf_s(szDebugString, 256, "rootbone center: %f, %f, %f\n", pos.x, pos.y, pos.z);
		OutputDebugStringA(szDebugString);
		sprintf_s(szDebugString, 256, "capsule center: %f, %f, %f\n", BoundingSphere.Center.x, BoundingSphere.Center.y, BoundingSphere.Center.z);
		OutputDebugStringA(szDebugString);
		sprintf_s(szDebugString, 256, "character center: %f, %f, %f\n", CharacterAnimationData.Position.x, CharacterAnimationData.Position.y, CharacterAnimationData.Position.z);
		OutputDebugStringA(szDebugString);
	}*/

	// update debugging sphere for chain.
	for (int i = 0; i < 4; ++i)
	{
		MeshConstant* pRightArmMeshConstantData = &m_ppRightArm[i]->MeshConstantData;
		MeshConstant* pLeftArmMeshConstantData = &m_ppLeftArm[i]->MeshConstantData;
		MeshConstant* pRightLegMeshConstantData = &m_ppRightLeg[i]->MeshConstantData;
		MeshConstant* pLeftLegMeshConstantData = &m_ppLeftLeg[i]->MeshConstantData;

		Joint* pRightArmPart = &RightArm.BodyChain[i];
		Joint* pLeftArmPart = &LeftArm.BodyChain[i];
		Joint* pRightLegPart = &RightLeg.BodyChain[i];
		Joint* pLeftLegPart = &LeftLeg.BodyChain[i];

		pRightArmMeshConstantData->World = (CharacterAnimationData.GetGlobalBonePositionMatix(CLIP_ID, FRAME, pRightArmPart->BoneID) * CHARACTER_POS).Transpose();
		pLeftArmMeshConstantData->World = (CharacterAnimationData.GetGlobalBonePositionMatix(CLIP_ID, FRAME, pLeftArmPart->BoneID) * CHARACTER_POS).Transpose();
		pRightLegMeshConstantData->World = (CharacterAnimationData.GetGlobalBonePositionMatix(CLIP_ID, FRAME, pRightLegPart->BoneID) * CHARACTER_POS).Transpose();
		pLeftLegMeshConstantData->World = (CharacterAnimationData.GetGlobalBonePositionMatix(CLIP_ID, FRAME, pLeftLegPart->BoneID) * CHARACTER_POS).Transpose();
	}

	RightHandMiddle.Center = m_ppRightArm[3]->MeshConstantData.World.Transpose().Translation();
	LeftHandMiddle.Center = m_ppLeftArm[3]->MeshConstantData.World.Transpose().Translation();
	RightToe.Center = m_ppRightLeg[3]->MeshConstantData.World.Transpose().Translation();
	LeftToe.Center = m_ppLeftLeg[3]->MeshConstantData.World.Transpose().Translation();
	{
		char szDebugString[256];
		/*sprintf_s(szDebugString, 256, "rightHand center: %f, %f, %f\nleftHand center: %f, %f, %f\nrightFoot center: %f, %f, %f\nleftFoot center: %f, %f, %f\n\n", 
				  RightHandMiddle.Center.x, RightHandMiddle.Center.y, RightHandMiddle.Center.z,
				  LeftHandMiddle.Center.x, LeftHandMiddle.Center.y, LeftHandMiddle.Center.z,
				  RightToe.Center.x, RightToe.Center.y, RightToe.Center.z,
				  LeftToe.Center.x, LeftToe.Center.y, LeftToe.Center.z);*/

		Matrix mat = CharacterAnimationData.BoneTransforms[RightLeg.BodyChain[2].BoneID];
		mat = CharacterAnimationData.InverseDefaultTransform * mat * CharacterAnimationData.DefaultTransform * CHARACTER_POS;
		Vector3 pos = mat.Translation();
		sprintf_s(szDebugString, 256, "right foot pos: %f, %f, %f\n", pos.x, pos.y, pos.z);
		OutputDebugStringA(szDebugString);
		sprintf_s(szDebugString, 256, "right toe pos: %f, %f, %f\n", RightToe.Center.x, RightToe.Center.y, RightToe.Center.z);
		OutputDebugStringA(szDebugString);

		pos = CharacterAnimationData.Position;
		sprintf_s(szDebugString, 256, "world pos: %f, %f, %f\n\n", pos.x, pos.y, pos.z);
		OutputDebugStringA(szDebugString);
	}
}

void SkinnedMeshModel::solveCharacterIK(const int CLIP_ID, const int FRAME, const float DELTA_TIME, JointUpdateInfo* pUpdateInfo)
{
	for (int jointPart = 0; jointPart < JointPart_TotalJointParts; ++jointPart)
	{
		if (!pUpdateInfo->bUpdatedJointParts[jointPart])
		{
			continue;
		}

		Vector3 targetPos(pUpdateInfo->EndEffectorTargetPoses[jointPart]);

		switch (jointPart)
		{
			case JointPart_RightArm:
				RightArm.SolveIK(targetPos, CLIP_ID, FRAME, DELTA_TIME);
				break;

			case JointPart_LeftArm:
				LeftArm.SolveIK(targetPos, CLIP_ID, FRAME, DELTA_TIME);
				break;

			case JointPart_RightLeg:
				RightLeg.SolveIK(targetPos, CLIP_ID, FRAME, DELTA_TIME);
				m_pTargetPos1->MeshConstantData.World = Matrix::CreateTranslation(targetPos).Transpose();
				/*{
					char szDebugString[256];
					sprintf_s(szDebugString, 256, "targetRightFootPos: %f, %f, %f\n", targetPos.x, targetPos.y, targetPos.z);
					OutputDebugStringA(szDebugString);
				}*/
				break;

			case JointPart_LeftLeg:
				LeftLeg.SolveIK(targetPos, CLIP_ID, FRAME, DELTA_TIME);
				m_pTargetPos2->MeshConstantData.World = Matrix::CreateTranslation(targetPos).Transpose();
				/*{
					char szDebugString[256];
					sprintf_s(szDebugString, 256, "targetLeftFootPos: %f, %f, %f\n", targetPos.x, targetPos.y, targetPos.z);
					OutputDebugStringA(szDebugString);
				}*/
				break;

			default:
				__debugbreak();
				break;
		}
	}
}
