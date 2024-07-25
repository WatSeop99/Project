#include "../pch.h"
#include "../Renderer/ConstantDataType.h"
#include "../Model/GeometryGenerator.h"
#include "../Graphics/GraphicsUtil.h"
#include "SkinnedMeshModel.h"

SkinnedMeshModel::SkinnedMeshModel(Renderer* pRenderer, const std::vector<MeshInfo>& MESHES, const AnimationData& ANIM_DATA)
{
	ModelType = RenderObjectType_SkinnedType;
	Initialize(pRenderer, MESHES, ANIM_DATA);
}

void SkinnedMeshModel::Initialize(Renderer* pRenderer, const std::vector<MeshInfo>& MESH_INFOS, const AnimationData& ANIM_DATA)
{
	Model::Initialize(pRenderer, MESH_INFOS);
	InitAnimationData(pRenderer, ANIM_DATA);
	initBoundingCapsule(pRenderer);
	initJointSpheres(pRenderer);
	initChain();

	MoveInfo.Position = World.Translation();
	MoveInfo.Direction = Vector3(0.0f, 0.0f, -1.0f); // should be normalized.
	MoveInfo.Rotation = Quaternion();
	MoveInfo.Velocity = 1.0f;
}

void SkinnedMeshModel::InitMeshBuffers(Renderer* pRenderer, const MeshInfo& MESH_INFO, Mesh* pNewMesh)
{
	_ASSERT(pRenderer);

	HRESULT hr = S_OK;
	ResourceManager* pManager = pRenderer->GetResourceManager();

	// vertex buffer.
	if (MESH_INFO.SkinnedVertices.size() > 0)
	{
		hr = pManager->CreateVertexBuffer(sizeof(SkinnedVertex),
										  (UINT)MESH_INFO.SkinnedVertices.size(),
										  &pNewMesh->Vertex.VertexBufferView,
										  &pNewMesh->Vertex.pBuffer,
										  (void*)MESH_INFO.SkinnedVertices.data());
		BREAK_IF_FAILED(hr);
		pNewMesh->Vertex.Count = (UINT)MESH_INFO.SkinnedVertices.size();
		pNewMesh->bSkinnedMesh = true;
	}
	else
	{
		hr = pManager->CreateVertexBuffer(sizeof(Vertex),
										  (UINT)MESH_INFO.Vertices.size(),
										  &pNewMesh->Vertex.VertexBufferView,
										  &pNewMesh->Vertex.pBuffer,
										  (void*)MESH_INFO.Vertices.data());
		BREAK_IF_FAILED(hr);
		pNewMesh->Vertex.Count = (UINT)MESH_INFO.Vertices.size();
	}

	// index buffer.
	hr = pManager->CreateIndexBuffer(sizeof(UINT),
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
	ResourceManager* pManager = pRenderer->GetResourceManager();

	// vertex buffer.
	if (MESH_INFO.SkinnedVertices.size() > 0)
	{
		hr = pManager->CreateVertexBuffer(sizeof(SkinnedVertex),
										  (UINT)MESH_INFO.SkinnedVertices.size(),
										  &(*ppNewMesh)->Vertex.VertexBufferView,
										  &(*ppNewMesh)->Vertex.pBuffer,
										  (void*)MESH_INFO.SkinnedVertices.data());
		BREAK_IF_FAILED(hr);
		(*ppNewMesh)->Vertex.Count = (UINT)MESH_INFO.SkinnedVertices.size();
		(*ppNewMesh)->bSkinnedMesh = true;
	}
	else
	{
		hr = pManager->CreateVertexBuffer(sizeof(Vertex),
										  (UINT)MESH_INFO.Vertices.size(),
										  &(*ppNewMesh)->Vertex.VertexBufferView,
										  &(*ppNewMesh)->Vertex.pBuffer,
										  (void*)MESH_INFO.Vertices.data());
		BREAK_IF_FAILED(hr);
		(*ppNewMesh)->Vertex.Count = (UINT)MESH_INFO.Vertices.size();
	}

	// index buffer.
	hr = pManager->CreateIndexBuffer(sizeof(UINT),
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

	BoneTransforms.Initialize(pRenderer, (UINT)ANIM_DATA.Clips[0].Keys.size(), sizeof(Matrix));

	// 단위행렬로 초기화.
	Matrix* pBoneTransformConstData = (Matrix*)BoneTransforms.pData;
	for (UINT64 i = 0, size = ANIM_DATA.Clips[0].Keys.size(); i < size; ++i)
	{
		pBoneTransformConstData[i] = Matrix();
	}
	BoneTransforms.Upload();
}

void SkinnedMeshModel::UpdateConstantBuffers()
{
	/*if (!bIsVisible)
	{
		return;
	}

	Model::UpdateConstantBuffers();
	m_pBoundingCapsuleMesh->MeshConstant.Upload();
	for (int i = 0; i < 4; ++i)
	{
		m_ppRightArm[i]->MeshConstant.Upload();
		m_ppLeftArm[i]->MeshConstant.Upload();
		m_ppRightLeg[i]->MeshConstant.Upload();
		m_ppLeftLeg[i]->MeshConstant.Upload();
	}*/
}

void SkinnedMeshModel::UpdateAnimation(int clipID, int frame, const float DELTA_TIME)
{
	if (!bIsVisible)
	{
		return;
	}

	// 입력에 따른 변환행렬 업데이트.
	CharacterAnimationData.Update(clipID, frame, MoveInfo);

	// 버퍼 업데이트.
	Matrix* pBoneTransformConstData = (Matrix*)BoneTransforms.pData;
	for (UINT64 i = 0, size = BoneTransforms.ElementCount; i < size; ++i)
	{
		pBoneTransformConstData[i] = CharacterAnimationData.Get((int)i).Transpose();
	}
	BoneTransforms.Upload();

	updateJointSpheres(clipID, frame);
}

void SkinnedMeshModel::UpdateCharacterIK(Vector3& target, int chainPart, int clipID, int frame, const float DELTA_TIME)
{
	switch (chainPart)
	{
		// right arm.
		case 0:
			RightArm.SolveIK(target, clipID, frame, DELTA_TIME);
			break;

			// left arm.
		case 1:
			LeftArm.SolveIK(target, clipID, frame, DELTA_TIME);
			break;

			// right leg.
		case 2:
			RightLeg.SolveIK(target, clipID, frame, DELTA_TIME);
			break;

			// left leg.
		case 3:
			LeftLeg.SolveIK(target, clipID, frame, DELTA_TIME);
			break;

		default:
			__debugbreak();
			break;
	}
}

void SkinnedMeshModel::Render(Renderer* pRenderer, eRenderPSOType psoSetting)
{
	_ASSERT(pRenderer);

	if (!bIsVisible)
	{
		return;
	}

	HRESULT hr = S_OK;
	ResourceManager* pManager = pRenderer->GetResourceManager();

	ID3D12Device5* pDevice = pManager->m_pDevice;
	ID3D12GraphicsCommandList* pCommandList = pManager->GetCommandList();
	DynamicDescriptorPool* pDynamicDescriptorPool = pManager->m_pDynamicDescriptorPool;
	ConstantBufferPool* pMeshConstantBufferPool = pManager->m_pConstnatBufferManager->GetConstantBufferPool(ConstantBufferType_Mesh);
	ConstantBufferPool* pMaterialConstantBufferPool = pManager->m_pConstnatBufferManager->GetConstantBufferPool(ConstantBufferType_Material);
	const UINT CBV_SRV_UAV_DESCRIPTOR_SIZE = pManager->m_CBVSRVUAVDescriptorSize;

	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuDescriptorTable;
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuDescriptorTable;

	for (UINT64 i = 0, size = Meshes.size(); i < size; ++i)
	{
		Mesh* const pCurMesh = Meshes[i];

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

				CD3DX12_CPU_DESCRIPTOR_HANDLE dstHandle(cpuDescriptorTable, 0, CBV_SRV_UAV_DESCRIPTOR_SIZE);

				// t7
				pDevice->CopyDescriptorsSimple(1, dstHandle, BoneTransforms.GetSRVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dstHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);

				// b2, b3
				/*pDevice->CopyDescriptorsSimple(2, dstHandle, pCurMesh->MeshConstant.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dstHandle.Offset(2, CBV_SRV_UAV_DESCRIPTOR_SIZE);*/
				pDevice->CopyDescriptorsSimple(1, dstHandle, pMeshCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dstHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
				pDevice->CopyDescriptorsSimple(1, dstHandle, pMaterialCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dstHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);

				// t0 ~ t5
				pDevice->CopyDescriptorsSimple(6, dstHandle, pCurMesh->Material.Albedo.GetSRVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dstHandle.Offset(6, CBV_SRV_UAV_DESCRIPTOR_SIZE);

				// t6
				pDevice->CopyDescriptorsSimple(1, dstHandle, pCurMesh->Material.Height.GetSRVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable);

			}
			break;

			case RenderPSOType_DepthOnlySkinned:
			case RenderPSOType_DepthOnlyCubeSkinned:
			case RenderPSOType_DepthOnlyCascadeSkinned:
			{
				hr = pDynamicDescriptorPool->AllocDescriptorTable(&cpuDescriptorTable, &gpuDescriptorTable, 3);
				BREAK_IF_FAILED(hr);

				CD3DX12_CPU_DESCRIPTOR_HANDLE dstHandle(cpuDescriptorTable, 0, CBV_SRV_UAV_DESCRIPTOR_SIZE);

				// t7
				pDevice->CopyDescriptorsSimple(1, dstHandle, BoneTransforms.GetSRVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dstHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);

				// b2, b3
				// pDevice->CopyDescriptorsSimple(2, dstHandle, pCurMesh->MeshConstant.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				pDevice->CopyDescriptorsSimple(1, dstHandle, pMeshCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dstHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
				pDevice->CopyDescriptorsSimple(1, dstHandle, pMaterialCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dstHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);

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

void SkinnedMeshModel::Render(UINT threadIndex, ID3D12GraphicsCommandList* pCommandList, DynamicDescriptorPool* pDescriptorPool, ResourceManager* pManager, int psoSetting)
{
	_ASSERT(pCommandList);
	_ASSERT(pManager);
	_ASSERT(pDescriptorPool);

	if (!bIsVisible)
	{
		return;
	}

	HRESULT hr = S_OK;

	ID3D12Device5* pDevice = pManager->m_pDevice;
	ConstantBufferPool* pMeshConstantBufferPool = pManager->m_pConstnatBufferManager->GetConstantBufferPool(ConstantBufferType_Mesh);
	ConstantBufferPool* pMaterialConstantBufferPool = pManager->m_pConstnatBufferManager->GetConstantBufferPool(ConstantBufferType_Material);
	const UINT CBV_SRV_UAV_DESCRIPTOR_SIZE = pManager->m_CBVSRVUAVDescriptorSize;

	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuDescriptorTable;
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuDescriptorTable;

	for (UINT64 i = 0, size = Meshes.size(); i < size; ++i)
	{
		Mesh* pCurMesh = Meshes[i];

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

				CD3DX12_CPU_DESCRIPTOR_HANDLE dstHandle(cpuDescriptorTable, 0, CBV_SRV_UAV_DESCRIPTOR_SIZE);

				// t7
				pDevice->CopyDescriptorsSimple(1, dstHandle, BoneTransforms.GetSRVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dstHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);

				// b2, b3
				/*pDevice->CopyDescriptorsSimple(2, dstHandle, pCurMesh->MeshConstant.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dstHandle.Offset(2, CBV_SRV_UAV_DESCRIPTOR_SIZE);*/
				pDevice->CopyDescriptorsSimple(1, dstHandle, pMeshCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dstHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
				pDevice->CopyDescriptorsSimple(1, dstHandle, pMaterialCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dstHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);

				// t0 ~ t5
				pDevice->CopyDescriptorsSimple(6, dstHandle, pCurMesh->Material.Albedo.GetSRVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dstHandle.Offset(6, CBV_SRV_UAV_DESCRIPTOR_SIZE);

				// t6
				pDevice->CopyDescriptorsSimple(1, dstHandle, pCurMesh->Material.Height.GetSRVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable);

			}
			break;

			case RenderPSOType_DepthOnlySkinned:
			case RenderPSOType_DepthOnlyCubeSkinned:
			case RenderPSOType_DepthOnlyCascadeSkinned:
			{
				hr = pDescriptorPool->AllocDescriptorTable(&cpuDescriptorTable, &gpuDescriptorTable, 3);
				BREAK_IF_FAILED(hr);

				CD3DX12_CPU_DESCRIPTOR_HANDLE dstHandle(cpuDescriptorTable, 0, CBV_SRV_UAV_DESCRIPTOR_SIZE);

				// t7
				pDevice->CopyDescriptorsSimple(1, dstHandle, BoneTransforms.GetSRVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dstHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);

				// b2, b3
				// pDevice->CopyDescriptorsSimple(2, dstHandle, pCurMesh->MeshConstant.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				pDevice->CopyDescriptorsSimple(1, dstHandle, pMeshCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dstHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
				pDevice->CopyDescriptorsSimple(1, dstHandle, pMaterialCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dstHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);

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

void SkinnedMeshModel::RenderBoundingCapsule(Renderer* pRenderer, eRenderPSOType psoSetting)
{
	_ASSERT(pRenderer);

	HRESULT hr = S_OK;
	ResourceManager* pManager = pRenderer->GetResourceManager();

	ID3D12Device5* pDevice = pManager->m_pDevice;
	ID3D12GraphicsCommandList* pCommandList = pManager->GetCommandList();
	ID3D12DescriptorHeap* pCBVSRVHeap = pManager->m_pCBVSRVUAVHeap;
	DynamicDescriptorPool* pDynamicDescriptorPool = pManager->m_pDynamicDescriptorPool;
	ConstantBufferPool* pMeshConstantBufferPool = pManager->m_pConstnatBufferManager->GetConstantBufferPool(ConstantBufferType_Mesh);
	ConstantBufferPool* pMaterialConstantBufferPool = pManager->m_pConstnatBufferManager->GetConstantBufferPool(ConstantBufferType_Material);
	const UINT CBV_SRV_DESCRIPTOR_SIZE = pManager->m_CBVSRVUAVDescriptorSize;


	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuDescriptorTable = {};
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuDescriptorTable = {};

	hr = pDynamicDescriptorPool->AllocDescriptorTable(&cpuDescriptorTable, &gpuDescriptorTable, 3);
	BREAK_IF_FAILED(hr);


	CD3DX12_CPU_DESCRIPTOR_HANDLE dstHandle(cpuDescriptorTable, 0, CBV_SRV_DESCRIPTOR_SIZE);
	CD3DX12_CPU_DESCRIPTOR_HANDLE nullHandle(pCBVSRVHeap->GetCPUDescriptorHandleForHeapStart(), 14, CBV_SRV_DESCRIPTOR_SIZE);

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
	/*pDevice->CopyDescriptorsSimple(1, dstHandle, m_pBoundingCapsuleMesh->MeshConstant.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);
	pDevice->CopyDescriptorsSimple(1, dstHandle, m_pBoundingCapsuleMesh->MaterialConstant.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);*/
	pDevice->CopyDescriptorsSimple(1, dstHandle, pMeshCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);
	pDevice->CopyDescriptorsSimple(1, dstHandle, pMaterialCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);

	// t6(null)
	pDevice->CopyDescriptorsSimple(1, dstHandle, nullHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable);

	pCommandList->IASetVertexBuffers(0, 1, &m_pBoundingCapsuleMesh->Vertex.VertexBufferView);
	pCommandList->IASetIndexBuffer(&m_pBoundingCapsuleMesh->Index.IndexBufferView);
	pCommandList->DrawIndexedInstanced(m_pBoundingCapsuleMesh->Index.Count, 1, 0, 0, 0);
}

void SkinnedMeshModel::RenderJointSphere(Renderer* pRenderer, eRenderPSOType psoSetting)
{
	_ASSERT(pRenderer);

	HRESULT hr = S_OK;
	ResourceManager* pManager = pRenderer->GetResourceManager();

	ID3D12Device5* pDevice = pManager->m_pDevice;
	ID3D12GraphicsCommandList* pCommandList = pManager->GetCommandList();
	ID3D12DescriptorHeap* pCBVSRVHeap = pManager->m_pCBVSRVUAVHeap;
	DynamicDescriptorPool* pDynamicDescriptorPool = pManager->m_pDynamicDescriptorPool;
	ConstantBufferPool* pMeshConstantBufferPool = pManager->m_pConstnatBufferManager->GetConstantBufferPool(ConstantBufferType_Mesh);
	ConstantBufferPool* pMaterialConstantBufferPool = pManager->m_pConstnatBufferManager->GetConstantBufferPool(ConstantBufferType_Material);
	const UINT CBV_SRV_DESCRIPTOR_SIZE = pManager->m_CBVSRVUAVDescriptorSize;

	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuDescriptorTable[16];
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuDescriptorTable[16];
	CD3DX12_CPU_DESCRIPTOR_HANDLE nullHandle(pCBVSRVHeap->GetCPUDescriptorHandleForHeapStart(), 14, CBV_SRV_DESCRIPTOR_SIZE);

	for (int i = 0; i < 16; ++i)
	{
		hr = pDynamicDescriptorPool->AllocDescriptorTable(&cpuDescriptorTable[i], &gpuDescriptorTable[i], 3);
		BREAK_IF_FAILED(hr);
	}

	// render all chain spheres.
	{
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
			/*pDevice->CopyDescriptorsSimple(1, cpuDescriptorTable[descriptorTableIndex], m_ppRightArm[i]->MeshConstant.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			cpuDescriptorTable[descriptorTableIndex].Offset(1, CBV_SRV_DESCRIPTOR_SIZE);
			pDevice->CopyDescriptorsSimple(1, cpuDescriptorTable[descriptorTableIndex], m_ppRightArm[i]->MaterialConstant.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			cpuDescriptorTable[descriptorTableIndex].Offset(1, CBV_SRV_DESCRIPTOR_SIZE);*/
			pDevice->CopyDescriptorsSimple(1, descriptorHandle, pMeshCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			descriptorHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);
			pDevice->CopyDescriptorsSimple(1, descriptorHandle, pMaterialCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			descriptorHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);

			// t6(null)
			pDevice->CopyDescriptorsSimple(1, descriptorHandle, nullHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

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
			/*pDevice->CopyDescriptorsSimple(1, cpuDescriptorTable[descriptorTableIndex], m_ppLeftArm[i]->MeshConstant.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			cpuDescriptorTable[descriptorTableIndex].Offset(1, CBV_SRV_DESCRIPTOR_SIZE);
			pDevice->CopyDescriptorsSimple(1, cpuDescriptorTable[descriptorTableIndex], m_ppLeftArm[i]->MaterialConstant.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			cpuDescriptorTable[descriptorTableIndex].Offset(1, CBV_SRV_DESCRIPTOR_SIZE);*/
			pDevice->CopyDescriptorsSimple(1, descriptorHandle, pMeshCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			descriptorHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);
			pDevice->CopyDescriptorsSimple(1, descriptorHandle, pMaterialCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			descriptorHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);

			// t6(null)
			pDevice->CopyDescriptorsSimple(1, descriptorHandle, nullHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

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
			/*pDevice->CopyDescriptorsSimple(1, cpuDescriptorTable[descriptorTableIndex], m_ppRightLeg[i]->MeshConstant.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			cpuDescriptorTable[descriptorTableIndex].Offset(1, CBV_SRV_DESCRIPTOR_SIZE);
			pDevice->CopyDescriptorsSimple(1, cpuDescriptorTable[descriptorTableIndex], m_ppRightLeg[i]->MaterialConstant.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			cpuDescriptorTable[descriptorTableIndex].Offset(1, CBV_SRV_DESCRIPTOR_SIZE);*/
			pDevice->CopyDescriptorsSimple(1, descriptorHandle, pMeshCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			descriptorHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);
			pDevice->CopyDescriptorsSimple(1, descriptorHandle, pMaterialCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			descriptorHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);

			// t6(null)
			pDevice->CopyDescriptorsSimple(1, descriptorHandle, nullHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

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
			/*pDevice->CopyDescriptorsSimple(1, cpuDescriptorTable[descriptorTableIndex], m_ppLeftLeg[i]->MeshConstant.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			cpuDescriptorTable[descriptorTableIndex].Offset(1, CBV_SRV_DESCRIPTOR_SIZE);
			pDevice->CopyDescriptorsSimple(1, cpuDescriptorTable[descriptorTableIndex], m_ppLeftLeg[i]->MaterialConstant.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			cpuDescriptorTable[descriptorTableIndex].Offset(1, CBV_SRV_DESCRIPTOR_SIZE);*/
			pDevice->CopyDescriptorsSimple(1, descriptorHandle, pMeshCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			descriptorHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);
			pDevice->CopyDescriptorsSimple(1, descriptorHandle, pMaterialCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			descriptorHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);

			// t6(null)
			pDevice->CopyDescriptorsSimple(1, descriptorHandle, nullHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable[descriptorTableIndex]);

			pCommandList->IASetVertexBuffers(0, 1, &pPartOfLeg->Vertex.VertexBufferView);
			pCommandList->IASetIndexBuffer(&pPartOfLeg->Index.IndexBufferView);
			pCommandList->DrawIndexedInstanced(pPartOfLeg->Index.Count, 1, 0, 0, 0);
			++descriptorTableIndex;
		}
	}
}

void SkinnedMeshModel::Cleanup()
{
	BoneTransforms.Clear();

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
}

void SkinnedMeshModel::SetDescriptorHeap(Renderer* pRenderer)
{
	_ASSERT(pRenderer);

	ResourceManager* pManager = pRenderer->GetResourceManager();
	ID3D12Device5* pDevice = pManager->m_pDevice;
	ID3D12DescriptorHeap* pCBVSRVHeap = pManager->m_pCBVSRVUAVHeap;

	const UINT CBV_SRV_UAV_DESCRIPTOR_SIZE = pManager->m_CBVSRVUAVDescriptorSize;
	CD3DX12_CPU_DESCRIPTOR_HANDLE cbvSrvLastHandle(pCBVSRVHeap->GetCPUDescriptorHandleForHeapStart(), pManager->m_CBVSRVUAVHeapSize, CBV_SRV_UAV_DESCRIPTOR_SIZE);

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	// BoneTransform buffer.
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC structuredSRVDesc;
		structuredSRVDesc.Format = DXGI_FORMAT_UNKNOWN;
		structuredSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		structuredSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		structuredSRVDesc.Buffer.FirstElement = 0;
		structuredSRVDesc.Buffer.NumElements = (UINT)(BoneTransforms.ElementCount);
		structuredSRVDesc.Buffer.StructureByteStride = (UINT)(BoneTransforms.ElementSize);
		structuredSRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

		pDevice->CreateShaderResourceView(BoneTransforms.GetResource(), &structuredSRVDesc, cbvSrvLastHandle);
		BoneTransforms.SetSRVHandle(cbvSrvLastHandle);
		cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(pManager->m_CBVSRVUAVHeapSize);
	}

	// meshes.
	for (UINT64 i = 0, size = Meshes.size(); i < size; ++i)
	{
		Mesh* pCurMesh = Meshes[i];
		// ConstantBuffer& meshConstant = pCurMesh->MeshConstant;
		// ConstantBuffer& materialConstant = pCurMesh->MaterialConstant;
		Material* pMaterialBuffer = &pCurMesh->Material;

		/*cbvDesc.BufferLocation = meshConstant.GetGPUMemAddr();
		cbvDesc.SizeInBytes = (UINT)(meshConstant.GetBufferSize());
		pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvLastHandle);
		meshConstant.SetCBVHandle(cbvSrvLastHandle);
		cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(pManager->m_CBVSRVUAVHeapSize);

		cbvDesc.BufferLocation = materialConstant.GetGPUMemAddr();
		cbvDesc.SizeInBytes = (UINT)(materialConstant.GetBufferSize());
		pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvLastHandle);
		materialConstant.SetCBVHandle(cbvSrvLastHandle);
		cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(pManager->m_CBVSRVUAVHeapSize);*/

		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		pDevice->CreateShaderResourceView(pMaterialBuffer->Albedo.GetResource(), &srvDesc, cbvSrvLastHandle);
		pMaterialBuffer->Albedo.SetSRVHandle(cbvSrvLastHandle);
		cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(pManager->m_CBVSRVUAVHeapSize);

		pDevice->CreateShaderResourceView(pMaterialBuffer->Emissive.GetResource(), &srvDesc, cbvSrvLastHandle);
		pMaterialBuffer->Emissive.SetSRVHandle(cbvSrvLastHandle);
		cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(pManager->m_CBVSRVUAVHeapSize);

		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		pDevice->CreateShaderResourceView(pMaterialBuffer->Normal.GetResource(), &srvDesc, cbvSrvLastHandle);
		pMaterialBuffer->Normal.SetSRVHandle(cbvSrvLastHandle);
		cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(pManager->m_CBVSRVUAVHeapSize);

		pDevice->CreateShaderResourceView(pMaterialBuffer->Height.GetResource(), &srvDesc, cbvSrvLastHandle);
		pMaterialBuffer->Height.SetSRVHandle(cbvSrvLastHandle);
		cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(pManager->m_CBVSRVUAVHeapSize);

		pDevice->CreateShaderResourceView(pMaterialBuffer->AmbientOcclusion.GetResource(), &srvDesc, cbvSrvLastHandle);
		pMaterialBuffer->AmbientOcclusion.SetSRVHandle(cbvSrvLastHandle);
		cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(pManager->m_CBVSRVUAVHeapSize);

		pDevice->CreateShaderResourceView(pMaterialBuffer->Metallic.GetResource(), &srvDesc, cbvSrvLastHandle);
		pMaterialBuffer->Metallic.SetSRVHandle(cbvSrvLastHandle);
		cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(pManager->m_CBVSRVUAVHeapSize);

		pDevice->CreateShaderResourceView(pMaterialBuffer->Roughness.GetResource(), &srvDesc, cbvSrvLastHandle);
		pMaterialBuffer->Roughness.SetSRVHandle(cbvSrvLastHandle);
		cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(pManager->m_CBVSRVUAVHeapSize);
	}

	// bounding box.
	/*cbvDesc.BufferLocation = m_pBoundingBoxMesh->MeshConstant.GetGPUMemAddr();
	cbvDesc.SizeInBytes = (UINT)m_pBoundingBoxMesh->MeshConstant.GetBufferSize();
	pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvLastHandle);
	m_pBoundingBoxMesh->MeshConstant.SetCBVHandle(cbvSrvLastHandle);
	cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
	++(pManager->m_CBVSRVUAVHeapSize);

	cbvDesc.BufferLocation = m_pBoundingBoxMesh->MaterialConstant.GetGPUMemAddr();
	cbvDesc.SizeInBytes = (UINT)m_pBoundingBoxMesh->MaterialConstant.GetBufferSize();
	pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvLastHandle);
	m_pBoundingBoxMesh->MaterialConstant.SetCBVHandle(cbvSrvLastHandle);
	cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
	++(pManager->m_CBVSRVUAVHeapSize);*/

	// bounding sphere.
	/*cbvDesc.BufferLocation = m_pBoundingSphereMesh->MeshConstant.GetGPUMemAddr();
	cbvDesc.SizeInBytes = (UINT)m_pBoundingSphereMesh->MeshConstant.GetBufferSize();
	pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvLastHandle);
	m_pBoundingSphereMesh->MeshConstant.SetCBVHandle(cbvSrvLastHandle);
	cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
	++(pManager->m_CBVSRVUAVHeapSize);

	cbvDesc.BufferLocation = m_pBoundingSphereMesh->MaterialConstant.GetGPUMemAddr();
	cbvDesc.SizeInBytes = (UINT)m_pBoundingSphereMesh->MaterialConstant.GetBufferSize();
	pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvLastHandle);
	m_pBoundingSphereMesh->MaterialConstant.SetCBVHandle(cbvSrvLastHandle);
	cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
	++(pManager->m_CBVSRVUAVHeapSize);*/

	// bounding capsule.
	/*cbvDesc.BufferLocation = m_pBoundingCapsuleMesh->MeshConstant.GetGPUMemAddr();
	cbvDesc.SizeInBytes = (UINT)m_pBoundingCapsuleMesh->MeshConstant.GetBufferSize();
	pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvLastHandle);
	m_pBoundingCapsuleMesh->MeshConstant.SetCBVHandle(cbvSrvLastHandle);
	cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
	++(pManager->m_CBVSRVUAVHeapSize);

	cbvDesc.BufferLocation = m_pBoundingCapsuleMesh->MaterialConstant.GetGPUMemAddr();
	cbvDesc.SizeInBytes = (UINT)m_pBoundingCapsuleMesh->MaterialConstant.GetBufferSize();
	pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvLastHandle);
	m_pBoundingCapsuleMesh->MaterialConstant.SetCBVHandle(cbvSrvLastHandle);
	cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
	++(pManager->m_CBVSRVUAVHeapSize);*/

	// all chains.
	/*for (int i = 0; i < 4; ++i)
	{
		Mesh** ppRightArmPart = &m_ppRightArm[i];
		Mesh** ppLeftArmPart = &m_ppLeftArm[i];
		Mesh** ppRightLegPart = &m_ppRightLeg[i];
		Mesh** ppLeftLegPart = &m_ppLeftLeg[i];

		cbvDesc.BufferLocation = (*ppRightArmPart)->MeshConstant.GetGPUMemAddr();
		cbvDesc.SizeInBytes = (UINT)(*ppRightArmPart)->MeshConstant.GetBufferSize();
		pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvLastHandle);
		(*ppRightArmPart)->MeshConstant.SetCBVHandle(cbvSrvLastHandle);
		cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(pManager->m_CBVSRVUAVHeapSize);

		cbvDesc.BufferLocation = (*ppRightArmPart)->MaterialConstant.GetGPUMemAddr();
		cbvDesc.SizeInBytes = (UINT)(*ppRightArmPart)->MaterialConstant.GetBufferSize();
		pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvLastHandle);
		(*ppRightArmPart)->MaterialConstant.SetCBVHandle(cbvSrvLastHandle);
		cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(pManager->m_CBVSRVUAVHeapSize);

		cbvDesc.BufferLocation = (*ppLeftArmPart)->MeshConstant.GetGPUMemAddr();
		cbvDesc.SizeInBytes = (UINT)(*ppLeftArmPart)->MeshConstant.GetBufferSize();
		pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvLastHandle);
		(*ppLeftArmPart)->MeshConstant.SetCBVHandle(cbvSrvLastHandle);
		cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(pManager->m_CBVSRVUAVHeapSize);

		cbvDesc.BufferLocation = (*ppLeftArmPart)->MaterialConstant.GetGPUMemAddr();
		cbvDesc.SizeInBytes = (UINT)(*ppLeftArmPart)->MaterialConstant.GetBufferSize();
		pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvLastHandle);
		(*ppLeftArmPart)->MaterialConstant.SetCBVHandle(cbvSrvLastHandle);
		cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(pManager->m_CBVSRVUAVHeapSize);

		cbvDesc.BufferLocation = (*ppRightLegPart)->MeshConstant.GetGPUMemAddr();
		cbvDesc.SizeInBytes = (UINT)(*ppRightLegPart)->MeshConstant.GetBufferSize();
		pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvLastHandle);
		(*ppRightLegPart)->MeshConstant.SetCBVHandle(cbvSrvLastHandle);
		cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(pManager->m_CBVSRVUAVHeapSize);

		cbvDesc.BufferLocation = (*ppRightLegPart)->MaterialConstant.GetGPUMemAddr();
		cbvDesc.SizeInBytes = (UINT)(*ppRightLegPart)->MaterialConstant.GetBufferSize();
		pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvLastHandle);
		(*ppRightLegPart)->MaterialConstant.SetCBVHandle(cbvSrvLastHandle);
		cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(pManager->m_CBVSRVUAVHeapSize);

		cbvDesc.BufferLocation = (*ppLeftLegPart)->MeshConstant.GetGPUMemAddr();
		cbvDesc.SizeInBytes = (UINT)(*ppLeftLegPart)->MeshConstant.GetBufferSize();
		pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvLastHandle);
		(*ppLeftLegPart)->MeshConstant.SetCBVHandle(cbvSrvLastHandle);
		cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(pManager->m_CBVSRVUAVHeapSize);

		cbvDesc.BufferLocation = (*ppLeftLegPart)->MaterialConstant.GetGPUMemAddr();
		cbvDesc.SizeInBytes = (UINT)(*ppLeftLegPart)->MaterialConstant.GetBufferSize();
		pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvLastHandle);
		(*ppLeftLegPart)->MaterialConstant.SetCBVHandle(cbvSrvLastHandle);
		cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(pManager->m_CBVSRVUAVHeapSize);
	}*/
}

void SkinnedMeshModel::initBoundingCapsule(Renderer* pRenderer)
{
	MeshInfo meshData = INIT_MESH_INFO;
	/*MeshConstant* pMeshConst = nullptr;
	MaterialConstant* pMaterialConst = nullptr;

	MakeWireCapsule(&meshData, BoundingSphere.Center, 0.2f, BoundingSphere.Radius * 1.3f);
	m_pBoundingCapsuleMesh = new Mesh;
	m_pBoundingCapsuleMesh->MeshConstant.Initialize(pRenderer, sizeof(MeshConstant));
	m_pBoundingCapsuleMesh->MaterialConstant.Initialize(pRenderer, sizeof(MaterialConstant));
	pMeshConst = (MeshConstant*)m_pBoundingCapsuleMesh->MeshConstant.pData;
	pMaterialConst = (MaterialConstant*)m_pBoundingCapsuleMesh->MaterialConstant.pData;

	pMeshConst->World = Matrix();*/

	MakeWireCapsule(&meshData, BoundingSphere.Center, 0.2f, BoundingSphere.Radius * 1.3f);
	m_pBoundingCapsuleMesh = new Mesh;

	MeshConstant& meshConstantData = m_pBoundingCapsuleMesh->MeshConstantData;
	MaterialConstant& materialConstantData = m_pBoundingCapsuleMesh->MaterialConstantData;
	meshConstantData.World = Matrix();

	Model::InitMeshBuffers(pRenderer, meshData, m_pBoundingCapsuleMesh);
}

void SkinnedMeshModel::initJointSpheres(Renderer* pRenderer)
{
	MeshInfo meshData;
	MeshConstant* pMeshConst = nullptr;
	MaterialConstant* pMaterialConst = nullptr;

	// for chain debugging.
	meshData = INIT_MESH_INFO;
	MakeWireSphere(&meshData, BoundingSphere.Center, 0.03f);
	RightHandMiddle.Radius = 0.03f + 1e-3f;
	LeftHandMiddle.Radius = 0.03f + 1e-3f;
	RightToe.Radius = 0.03f + 1e-3f;
	LeftToe.Radius = 0.03f + 1e-3f;

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

		/*(*ppRightArmPart)->MeshConstant.Initialize(pRenderer, sizeof(MeshConstant));
		(*ppLeftArmPart)->MeshConstant.Initialize(pRenderer, sizeof(MeshConstant));
		(*ppRightLegPart)->MeshConstant.Initialize(pRenderer, sizeof(MeshConstant));
		(*ppLeftLegPart)->MeshConstant.Initialize(pRenderer, sizeof(MeshConstant));

		(*ppRightArmPart)->MaterialConstant.Initialize(pRenderer, sizeof(MaterialConstant));
		(*ppLeftArmPart)->MaterialConstant.Initialize(pRenderer, sizeof(MaterialConstant));
		(*ppRightLegPart)->MaterialConstant.Initialize(pRenderer, sizeof(MaterialConstant));
		(*ppLeftLegPart)->MaterialConstant.Initialize(pRenderer, sizeof(MaterialConstant));

		pMeshConst = (MeshConstant*)(*ppRightArmPart)->MeshConstant.pData;
		pMeshConst->World = Matrix();
		pMeshConst = (MeshConstant*)(*ppLeftArmPart)->MeshConstant.pData;
		pMeshConst->World = Matrix();
		pMeshConst = (MeshConstant*)(*ppRightLegPart)->MeshConstant.pData;
		pMeshConst->World = Matrix();
		pMeshConst = (MeshConstant*)(*ppLeftLegPart)->MeshConstant.pData;
		pMeshConst->World = Matrix();*/

		(*ppRightArmPart)->MeshConstantData.World = Matrix();
		(*ppLeftArmPart)->MeshConstantData.World = Matrix();
		(*ppRightLegPart)->MeshConstantData.World = Matrix();
		(*ppLeftLegPart)->MeshConstantData.World = Matrix();
		
		// need to be for pp ver.
		InitMeshBuffers(pRenderer, meshData, ppRightArmPart);
		InitMeshBuffers(pRenderer, meshData, ppLeftArmPart);
		InitMeshBuffers(pRenderer, meshData, ppRightLegPart);
		InitMeshBuffers(pRenderer, meshData, ppLeftLegPart);
	}
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
		// right arm
		Matrix::CreateTranslation(Vector3(0.09f, 0.52f, 0.048f)),
		Matrix::CreateTranslation(Vector3(-0.18f, 0.51f, 0.048f)),
		Matrix::CreateTranslation(Vector3(-0.32f, 0.52f, 0.048f)),
		Matrix::CreateTranslation(Vector3(-0.44f, 0.52f, 0.048f)),

		// left arm
		Matrix::CreateTranslation(Vector3(0.32f, 0.5125f, 0.048f)),
		Matrix::CreateTranslation(Vector3(0.61f, 0.5f, 0.048f)),
		Matrix::CreateTranslation(Vector3(0.74f, 0.5f, 0.048f)),
		Matrix::CreateTranslation(Vector3(0.87f, 0.5f, 0.05f)),

		// right leg
		Matrix::CreateTranslation(Vector3(0.165f, 0.05f, 0.04f)),
		Matrix::CreateTranslation(Vector3(0.155f, -0.18f, 0.05f)),
		Matrix::CreateTranslation(Vector3(0.16f, -0.38f, 0.05f)),
		Matrix::CreateTranslation(Vector3(0.14f, -0.43f, -0.09f)),

		// left leg
		Matrix::CreateTranslation(Vector3(0.26f, 0.05f, 0.04f)),
		Matrix::CreateTranslation(Vector3(0.26f, -0.18f, 0.05f)),
		Matrix::CreateTranslation(Vector3(0.25f, -0.38f, 0.05f)),
		Matrix::CreateTranslation(Vector3(0.26f, -0.43f, -0.09f)),
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
		// pJoint->Position = ((MeshConstant*)m_ppRightArm[i]->MeshConstant.pData)->World.Transpose().Translation();
		pJoint->Position = m_ppRightArm[i]->MeshConstantData.World.Transpose().Translation();
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
		// pJoint->Position = ((MeshConstant*)m_ppLeftArm[i]->MeshConstant.pData)->World.Transpose().Translation();
		pJoint->Position = m_ppLeftArm[i]->MeshConstantData.World.Transpose().Translation();
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
		// pJoint->Position = ((MeshConstant*)m_ppRightLeg[i]->MeshConstant.pData)->World.Transpose().Translation();
		pJoint->Position = m_ppRightLeg[i]->MeshConstantData.World.Transpose().Translation();
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
		// pJoint->Position = ((MeshConstant*)m_ppLeftLeg[i]->MeshConstant.pData)->World.Transpose().Translation();
		pJoint->Position = m_ppLeftLeg[i]->MeshConstantData.World.Transpose().Translation();
		pJoint->pOffset = &CharacterAnimationData.OffsetMatrices[BONE_ID];
		pJoint->pParentMatrix = &CharacterAnimationData.BoneTransforms[BONE_PARENT_ID];
		pJoint->pJointTransform = &CharacterAnimationData.BoneTransforms[BONE_ID];
		pJoint->Correction = BONE_CORRECTION_TRANSFORM[boneNameIndex];
		pJoint->CharacterWorld = World;

		++boneNameIndex;
	}
}

void SkinnedMeshModel::updateJointSpheres(int clipID, int frame)
{
	// root bone transform을 통해 bounding box 업데이트.
	// 캐릭터는 world 상 고정된 좌표에서 bone transform을 통해 애니메이션하고 있으므로,
	// world를 바꿔주게 되면 캐릭터 자체가 시야에서 없어져버림.
	// 현재, Model에서는 bounding box와 bounding sphere를 world에 맞춰 이동시키는데,
	// 캐릭터에서는 이를 방지하기 위해 bounding object만 따로 변환시킴.

	const int ROOT_BONE_ID = CharacterAnimationData.BoneNameToID["mixamorig:Hips"];
	const Matrix ROOT_BONE_TRANSFORM = CharacterAnimationData.GetRootBoneTransformWithoutLocalRot(clipID, frame);
	const Matrix CORRECTION_CENTER = Matrix::CreateTranslation(Vector3(0.2f, 0.05f, 0.0f));

	/*MeshConstant* pBoxMeshConst = (MeshConstant*)m_pBoundingBoxMesh->MeshConstant.pData;
	MeshConstant* pSphereMeshConst = (MeshConstant*)m_pBoundingSphereMesh->MeshConstant.pData;
	MeshConstant* pCapsuleMeshConst = (MeshConstant*)m_pBoundingCapsuleMesh->MeshConstant.pData;

	pBoxMeshConst->World = (CORRECTION_CENTER * ROOT_BONE_TRANSFORM * World).Transpose();
	// pBoxMeshConst->World = (ROOT_BONE_TRANSFORM * World).Transpose();
	pSphereMeshConst->World = pBoxMeshConst->World;
	pCapsuleMeshConst->World = pBoxMeshConst->World;
	BoundingBox.Center = pBoxMeshConst->World.Transpose().Translation();
	BoundingSphere.Center = BoundingBox.Center;*/

	m_pBoundingBoxMesh->MeshConstantData.World = (CORRECTION_CENTER * ROOT_BONE_TRANSFORM * World).Transpose();
	m_pBoundingSphereMesh->MeshConstantData.World = m_pBoundingBoxMesh->MeshConstantData.World;
	m_pBoundingCapsuleMesh->MeshConstantData.World = m_pBoundingBoxMesh->MeshConstantData.World;
	BoundingBox.Center = m_pBoundingBoxMesh->MeshConstantData.World.Transpose().Translation();
	BoundingSphere.Center = BoundingBox.Center;

	// update debugging sphere for chain.
	{
		const char* BONE_NAME[16] =
		{
			"mixamorig:RightArm",
			"mixamorig:RightForeArm",
			"mixamorig:RightHand",
			"mixamorig:RightHandMiddle1",

			"mixamorig:LeftArm",
			"mixamorig:LeftForeArm",
			"mixamorig:LeftHand",
			"mixamorig:LeftHandMiddle1",

			"mixamorig:RightUpLeg",
			"mixamorig:RightLeg",
			"mixamorig:RightFoot",
			"mixamorig:RightToeBase",

			"mixamorig:LeftUpLeg",
			"mixamorig:LeftLeg",
			"mixamorig:LeftFoot",
			"mixamorig:LeftToeBase",
		};
		const Matrix BONE_CORRECTION_TRANSFORM[16] =
		{
			/*Matrix::CreateTranslation(Vector3(0.09f, 0.52f, 0.048f)),
			Matrix::CreateTranslation(Vector3(-0.18f, 0.51f, 0.048f)),
			Matrix::CreateTranslation(Vector3(-0.32f, 0.52f, 0.048f)),
			Matrix::CreateTranslation(Vector3(-0.44f, 0.52f, 0.048f)),

			Matrix::CreateTranslation(Vector3(0.32f, 0.5125f, 0.048f)),
			Matrix::CreateTranslation(Vector3(0.61f, 0.5f, 0.048f)),
			Matrix::CreateTranslation(Vector3(0.74f, 0.5f, 0.048f)),
			Matrix::CreateTranslation(Vector3(0.87f, 0.5f, 0.05f)),

			Matrix::CreateTranslation(Vector3(0.165f, 0.05f, 0.04f)),
			Matrix::CreateTranslation(Vector3(0.155f, -0.18f, 0.05f)),
			Matrix::CreateTranslation(Vector3(0.16f, -0.38f, 0.05f)),
			Matrix::CreateTranslation(Vector3(0.14f, -0.43f, -0.09f)),

			Matrix::CreateTranslation(Vector3(0.26f, 0.05f, 0.04f)),
			Matrix::CreateTranslation(Vector3(0.26f, -0.18f, 0.05f)),
			Matrix::CreateTranslation(Vector3(0.25f, -0.38f, 0.05f)),
			Matrix::CreateTranslation(Vector3(0.26f, -0.43f, -0.09f)),*/

			Matrix::CreateTranslation(Vector3(0.085f, 0.33f, 0.06f)),
			Matrix::CreateTranslation(Vector3(-0.04f, 0.32f, 0.06f)),
			Matrix::CreateTranslation(Vector3(-0.18f, 0.32f, 0.06f)),
			Matrix::CreateTranslation(Vector3(-0.235f, 0.32f, 0.055f)),

			Matrix::CreateTranslation(Vector3(0.32f, 0.34f, 0.05f)),
			Matrix::CreateTranslation(Vector3(0.45f, 0.32f, 0.05f)),
			Matrix::CreateTranslation(Vector3(0.59f, 0.32f, 0.05f)),
			Matrix::CreateTranslation(Vector3(0.65f, 0.32f, 0.05f)),

			Matrix::CreateTranslation(Vector3(0.16f, 0.02f, 0.04f)),
			Matrix::CreateTranslation(Vector3(0.15f, -0.17f, 0.04f)),
			Matrix::CreateTranslation(Vector3(0.16f, -0.39f, 0.05f)),
			Matrix::CreateTranslation(Vector3(0.15f, -0.42f, 0.0f)),

			Matrix::CreateTranslation(Vector3(0.26f, 0.025f, 0.05f)),
			Matrix::CreateTranslation(Vector3(0.26f, -0.165f, 0.05f)),
			Matrix::CreateTranslation(Vector3(0.25f, -0.38f, 0.05f)),
			Matrix::CreateTranslation(Vector3(0.26f, -0.42f, 0.0f)),
		};
		int boneIDs[16] = {};
		Matrix transformMatrics[16] = {};
		MeshConstant* ppMeshConstants[16] =
		{
			/*(MeshConstant*)m_ppRightArm[0]->MeshConstant.pData,
			(MeshConstant*)m_ppRightArm[1]->MeshConstant.pData,
			(MeshConstant*)m_ppRightArm[2]->MeshConstant.pData,
			(MeshConstant*)m_ppRightArm[3]->MeshConstant.pData,
			(MeshConstant*)m_ppLeftArm[0]->MeshConstant.pData,
			(MeshConstant*)m_ppLeftArm[1]->MeshConstant.pData,
			(MeshConstant*)m_ppLeftArm[2]->MeshConstant.pData,
			(MeshConstant*)m_ppLeftArm[3]->MeshConstant.pData,
			(MeshConstant*)m_ppRightLeg[0]->MeshConstant.pData,
			(MeshConstant*)m_ppRightLeg[1]->MeshConstant.pData,
			(MeshConstant*)m_ppRightLeg[2]->MeshConstant.pData,
			(MeshConstant*)m_ppRightLeg[3]->MeshConstant.pData,
			(MeshConstant*)m_ppLeftLeg[0]->MeshConstant.pData,
			(MeshConstant*)m_ppLeftLeg[1]->MeshConstant.pData,
			(MeshConstant*)m_ppLeftLeg[2]->MeshConstant.pData,
			(MeshConstant*)m_ppLeftLeg[3]->MeshConstant.pData,*/
			&m_ppRightArm[0]->MeshConstantData,
			&m_ppRightArm[1]->MeshConstantData,
			&m_ppRightArm[2]->MeshConstantData,
			&m_ppRightArm[3]->MeshConstantData,
			&m_ppLeftArm[0]->MeshConstantData,
			&m_ppLeftArm[1]->MeshConstantData,
			&m_ppLeftArm[2]->MeshConstantData,
			&m_ppLeftArm[3]->MeshConstantData,
			&m_ppRightLeg[0]->MeshConstantData,
			&m_ppRightLeg[1]->MeshConstantData,
			&m_ppRightLeg[2]->MeshConstantData,
			&m_ppRightLeg[3]->MeshConstantData,
			&m_ppLeftLeg[0]->MeshConstantData,
			&m_ppLeftLeg[1]->MeshConstantData,
			&m_ppLeftLeg[2]->MeshConstantData,
			&m_ppLeftLeg[3]->MeshConstantData,
		};

		for (int i = 0; i < 16; ++i)
		{
			boneIDs[i] = CharacterAnimationData.BoneNameToID[BONE_NAME[i]];
			transformMatrics[i] = CharacterAnimationData.Get(boneIDs[i]);

			ppMeshConstants[i]->World = (BONE_CORRECTION_TRANSFORM[i] * transformMatrics[i] * World).Transpose();
			// ppMeshConstants[i]->World = (transformMatrics[i] * World).Transpose();

			if (i >= 0 && i < 4)
			{
				Joint* pJoint = &RightArm.BodyChain[i % 4];
				pJoint->Position = ppMeshConstants[i]->World.Transpose().Translation();
			}
			else if (i >= 4 && i < 8)
			{
				Joint* pJoint = &LeftArm.BodyChain[i % 4];
				pJoint->Position = ppMeshConstants[i]->World.Transpose().Translation();
			}
			else if (i >= 8 && i < 12)
			{
				Joint* pJoint = &RightLeg.BodyChain[i % 4];
				pJoint->Position = ppMeshConstants[i]->World.Transpose().Translation();
			}
			else if (i >= 12 && i < 16)
			{
				Joint* pJoint = &LeftLeg.BodyChain[i % 4];
				pJoint->Position = ppMeshConstants[i]->World.Transpose().Translation();
			}
		}

		RightHandMiddle.Center = ppMeshConstants[3]->World.Transpose().Translation();
		LeftHandMiddle.Center = ppMeshConstants[7]->World.Transpose().Translation();
		RightToe.Center = ppMeshConstants[11]->World.Transpose().Translation();
		LeftToe.Center = ppMeshConstants[15]->World.Transpose().Translation();
	}
}
