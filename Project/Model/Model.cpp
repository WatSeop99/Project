#include "../pch.h"
#include "../Renderer/ConstantDataType.h"
#include "../Graphics/GraphicsUtil.h"
#include "GeometryGenerator.h"
#include "../Util/Utility.h"
#include "Model.h"

Model::Model(Renderer* pRenderer, std::wstring& basePath, std::wstring& fileName)
{
	Initialize(pRenderer, basePath, fileName);
}

Model::Model(Renderer* pRenderer, const std::vector<MeshInfo>& MESH_INFOS)
{
	Initialize(pRenderer, MESH_INFOS);
}

void Model::Initialize(Renderer* pRenderer, std::wstring& basePath, std::wstring& fileName)
{
	std::vector<MeshInfo> meshInfos;
	ReadFromFile(meshInfos, basePath, fileName);
	Initialize(pRenderer, meshInfos);
}

void Model::Initialize(Renderer* pRenderer, const std::vector<MeshInfo>& MESH_INFOS)
{
	_ASSERT(pRenderer);

	HRESULT hr = S_OK;
	struct _stat64 sourceFileStat;

	ResourceManager* pManager = pRenderer->GetResourceManager();
	ID3D12Device5* pDevice = pManager->m_pDevice;
	ID3D12GraphicsCommandList* pCommandList = pManager->GetCommandList();

	Meshes.reserve(MESH_INFOS.size());

	for (UINT64 i = 0, meshSize = MESH_INFOS.size(); i < meshSize; ++i)
	{
		const MeshInfo& MESH_DATA = MESH_INFOS[i];
		Mesh* pNewMesh = new Mesh;
		/*MeshConstant* pMeshConst = nullptr;
		MaterialConstant* pMaterialConst = nullptr;

		pNewMesh->MeshConstant.Initialize(pRenderer, sizeof(MeshConstant));
		pNewMesh->MaterialConstant.Initialize(pRenderer, sizeof(MaterialConstant));
		pMeshConst = (MeshConstant*)pNewMesh->MeshConstant.pData;
		pMaterialConst = (MaterialConstant*)pNewMesh->MaterialConstant.pData;

		pMeshConst->World = Matrix();*/
		MeshConstant& meshConstantData = pNewMesh->MeshConstantData;
		MaterialConstant& materialConstantData = pNewMesh->MaterialConstantData;

		InitMeshBuffers(pRenderer, MESH_DATA, pNewMesh);

		if (!MESH_DATA.szAlbedoTextureFileName.empty())
		{
			std::string albedoTextureA(MESH_DATA.szAlbedoTextureFileName.begin(), MESH_DATA.szAlbedoTextureFileName.end());

			if (_stat64(albedoTextureA.c_str(), &sourceFileStat) != -1)
			{
				pNewMesh->Material.Albedo.Initialize(pRenderer, MESH_DATA.szAlbedoTextureFileName.c_str(), true);
				// pMaterialConst->bUseAlbedoMap = TRUE;
				materialConstantData.bUseAlbedoMap = TRUE;
			}
			else
			{
				OutputDebugStringW(MESH_DATA.szAlbedoTextureFileName.c_str());
				OutputDebugStringA(" does not exists. Skip texture reading.\n");
			}
		}
		if (!MESH_DATA.szEmissiveTextureFileName.empty())
		{
			std::string emissiveTextureA(MESH_DATA.szEmissiveTextureFileName.begin(), MESH_DATA.szEmissiveTextureFileName.end());

			if (_stat64(emissiveTextureA.c_str(), &sourceFileStat) != -1)
			{
				pNewMesh->Material.Emissive.Initialize(pRenderer, MESH_DATA.szEmissiveTextureFileName.c_str(), true);
				// pMaterialConst->bUseEmissiveMap = TRUE;
				materialConstantData.bUseEmissiveMap = TRUE;
			}
			else
			{
				OutputDebugStringW(MESH_DATA.szEmissiveTextureFileName.c_str());
				OutputDebugStringA(" does not exists. Skip texture reading.\n");
			}
		}
		if (!MESH_DATA.szNormalTextureFileName.empty())
		{
			std::string normalTextureA(MESH_DATA.szNormalTextureFileName.begin(), MESH_DATA.szNormalTextureFileName.end());

			if (_stat64(normalTextureA.c_str(), &sourceFileStat) != -1)
			{
				pNewMesh->Material.Normal.Initialize(pRenderer, MESH_DATA.szNormalTextureFileName.c_str(), false);
				// pMaterialConst->bUseNormalMap = TRUE;
				materialConstantData.bUseNormalMap = TRUE;
			}
			else
			{
				OutputDebugStringW(MESH_DATA.szNormalTextureFileName.c_str());
				OutputDebugStringA(" does not exists. Skip texture reading.\n");
			}
		}
		if (!MESH_DATA.szHeightTextureFileName.empty())
		{
			std::string heightTextureA(MESH_DATA.szHeightTextureFileName.begin(), MESH_DATA.szHeightTextureFileName.end());

			if (_stat64(heightTextureA.c_str(), &sourceFileStat) != -1)
			{
				pNewMesh->Material.Height.Initialize(pRenderer, MESH_DATA.szHeightTextureFileName.c_str(), false);
				// pMeshConst->bUseHeightMap = TRUE;
				meshConstantData.bUseHeightMap = TRUE;
			}
			else
			{
				OutputDebugStringW(MESH_DATA.szHeightTextureFileName.c_str());
				OutputDebugStringA(" does not exists. Skip texture reading.\n");
			}
		}
		if (!MESH_DATA.szAOTextureFileName.empty())
		{
			std::string aoTextureA(MESH_DATA.szAOTextureFileName.begin(), MESH_DATA.szAOTextureFileName.end());

			if (_stat64(aoTextureA.c_str(), &sourceFileStat) != -1)
			{
				pNewMesh->Material.AmbientOcclusion.Initialize(pRenderer, MESH_DATA.szAOTextureFileName.c_str(), false);
				// pMaterialConst->bUseAOMap = TRUE;
				materialConstantData.bUseAOMap = TRUE;
			}
			else
			{
				OutputDebugStringW(MESH_DATA.szAOTextureFileName.c_str());
				OutputDebugStringA(" does not exists. Skip texture reading.\n");
			}
		}
		if (!MESH_DATA.szMetallicTextureFileName.empty())
		{
			std::string metallicTextureA(MESH_DATA.szMetallicTextureFileName.begin(), MESH_DATA.szMetallicTextureFileName.end());

			if (_stat64(metallicTextureA.c_str(), &sourceFileStat) != -1)
			{
				pNewMesh->Material.Metallic.Initialize(pRenderer, MESH_DATA.szMetallicTextureFileName.c_str(), false);
				// pMaterialConst->bUseMetallicMap = TRUE;
				materialConstantData.bUseMetallicMap = TRUE;
			}
			else
			{
				OutputDebugStringW(MESH_DATA.szMetallicTextureFileName.c_str());
				OutputDebugStringA(" does not exists. Skip texture reading.\n");
			}
		}
		if (!MESH_DATA.szRoughnessTextureFileName.empty())
		{
			std::string roughnessTextureA(MESH_DATA.szRoughnessTextureFileName.begin(), MESH_DATA.szRoughnessTextureFileName.end());

			if (_stat64(roughnessTextureA.c_str(), &sourceFileStat) != -1)
			{
				pNewMesh->Material.Roughness.Initialize(pRenderer, MESH_DATA.szRoughnessTextureFileName.c_str(), false);
				// pMaterialConst->bUseRoughnessMap = TRUE;
				materialConstantData.bUseRoughnessMap = TRUE;
			}
			else
			{
				OutputDebugStringW(MESH_DATA.szRoughnessTextureFileName.c_str());
				OutputDebugStringA(" does not exists. Skip texture reading.\n");
			}
		}

		Meshes.push_back(pNewMesh);
	}

	initBoundingBox(pRenderer, MESH_INFOS);
	initBoundingSphere(pRenderer, MESH_INFOS);
}

void Model::InitMeshBuffers(Renderer* pRenderer, const MeshInfo& MESH_INFO, Mesh* pNewMesh)
{
	_ASSERT(pRenderer);

	HRESULT hr = S_OK;
	ResourceManager* pManager = pRenderer->GetResourceManager();

	// vertex buffer.
	hr = pManager->CreateVertexBuffer(sizeof(Vertex),
									  (UINT)MESH_INFO.Vertices.size(),
									  &pNewMesh->Vertex.VertexBufferView,
									  &pNewMesh->Vertex.pBuffer,
									  (void*)MESH_INFO.Vertices.data());
	BREAK_IF_FAILED(hr);
	pNewMesh->Vertex.Count = (UINT)MESH_INFO.Vertices.size();

	// index buffer.
	hr = pManager->CreateIndexBuffer(sizeof(UINT),
									 (UINT)MESH_INFO.Indices.size(),
									 &pNewMesh->Index.IndexBufferView,
									 &pNewMesh->Index.pBuffer,
									 (void*)MESH_INFO.Indices.data());
	BREAK_IF_FAILED(hr);
	pNewMesh->Index.Count = (UINT)MESH_INFO.Indices.size();
}

void Model::UpdateConstantBuffers()
{
	/*if (!bIsVisible)
	{
		return;
	}

	for (UINT64 i = 0, size = Meshes.size(); i < size; ++i)
	{
		Mesh* pCurMesh = Meshes[i];
		pCurMesh->MeshConstant.Upload();
		pCurMesh->MaterialConstant.Upload();
	}

	m_pBoundingBoxMesh->MeshConstant.Upload();
	m_pBoundingSphereMesh->MeshConstant.Upload();*/
}

void Model::UpdateWorld(const Matrix& WORLD)
{
	World = WORLD;
	WorldInverseTranspose = WORLD;
	WorldInverseTranspose.Translation(Vector3(0.0f));
	WorldInverseTranspose = WorldInverseTranspose.Invert().Transpose();

	// bounding sphere 위치 업데이트.
	BoundingSphere.Center = World.Translation();
	BoundingBox.Center = BoundingSphere.Center;

	/*MeshConstant* pBoxMeshConst = (MeshConstant*)m_pBoundingBoxMesh->MeshConstant.pData;
	MeshConstant* pSphereMeshConst = (MeshConstant*)m_pBoundingSphereMesh->MeshConstant.pData;

	pBoxMeshConst->World = World.Transpose();
	pBoxMeshConst->WorldInverseTranspose = WorldInverseTranspose.Transpose();
	pBoxMeshConst->WorldInverse = WorldInverseTranspose;
	pSphereMeshConst->World = pBoxMeshConst->World;
	pSphereMeshConst->WorldInverseTranspose = pBoxMeshConst->WorldInverseTranspose;
	pSphereMeshConst->WorldInverse = pBoxMeshConst->WorldInverse;

	for (UINT64 i = 0, size = Meshes.size(); i < size; ++i)
	{
		Mesh* pCurMesh = Meshes[i];
		MeshConstant* pMeshConst = (MeshConstant*)pCurMesh->MeshConstant.pData;
		pMeshConst->World = WORLD.Transpose();
		pMeshConst->WorldInverseTranspose = WorldInverseTranspose.Transpose();
		pMeshConst->WorldInverse = WorldInverseTranspose.Transpose();
	}*/

	MeshConstant& boxMeshConstantData = m_pBoundingBoxMesh->MeshConstantData;
	MeshConstant& sphereMeshConstantData = m_pBoundingSphereMesh->MeshConstantData;

	boxMeshConstantData.World = World.Transpose();
	boxMeshConstantData.WorldInverseTranspose = WorldInverseTranspose.Transpose();
	boxMeshConstantData.WorldInverse = WorldInverseTranspose;
	sphereMeshConstantData.World = boxMeshConstantData.World;
	sphereMeshConstantData.WorldInverseTranspose = boxMeshConstantData.WorldInverseTranspose;
	sphereMeshConstantData.WorldInverse = boxMeshConstantData.WorldInverse;

	for (UINT64 i = 0, size = Meshes.size(); i < size; ++i)
	{
		Mesh* pCurMesh = Meshes[i];
		MeshConstant& meshConstantData = pCurMesh->MeshConstantData;

		meshConstantData.World = WORLD.Transpose();
		meshConstantData.WorldInverseTranspose = WorldInverseTranspose.Transpose();
		meshConstantData.WorldInverse = WorldInverseTranspose.Transpose();
	}
}

void Model::Render(Renderer* pRenderer, eRenderPSOType psoSetting)
{
	_ASSERT(pRenderer);

	HRESULT hr = S_OK;
	ResourceManager* pManager = pRenderer->GetResourceManager();

	ID3D12Device5* pDevice = pManager->m_pDevice;
	ID3D12GraphicsCommandList* pCommandList = pManager->GetCommandList();
	DynamicDescriptorPool* pDynamicDescriptorPool = pManager->m_pDynamicDescriptorPool;
	ConstantBufferPool* pMeshConstantBufferPool = pManager->m_pConstnatBufferManager->GetConstantBufferPool(ConstantBufferType_Mesh);
	ConstantBufferPool* pMaterialConstantBufferPool = pManager->m_pConstnatBufferManager->GetConstantBufferPool(ConstantBufferType_Material);
	const UINT CBV_SRV_DESCRIPTOR_SIZE = pManager->m_CBVSRVUAVDescriptorSize;

	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuDescriptorTable = {};
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuDescriptorTable = {};

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
			case RenderPSOType_Default: 
			case RenderPSOType_Skybox:
			case RenderPSOType_MirrorBlend: 
			case RenderPSOType_ReflectionDefault: 
			case RenderPSOType_ReflectionSkybox:
			{
				hr = pDynamicDescriptorPool->AllocDescriptorTable(&cpuDescriptorTable, &gpuDescriptorTable, 9);
				BREAK_IF_FAILED(hr);

				CD3DX12_CPU_DESCRIPTOR_HANDLE dstHandle(cpuDescriptorTable, 0, CBV_SRV_DESCRIPTOR_SIZE);

				// b2, b3
				/*pDevice->CopyDescriptorsSimple(2, dstHandle, pCurMesh->MeshConstant.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dstHandle.Offset(2, CBV_SRV_DESCRIPTOR_SIZE);*/
				pDevice->CopyDescriptorsSimple(1, dstHandle, pMeshCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);
				pDevice->CopyDescriptorsSimple(1, dstHandle, pMaterialCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);

				// t0 ~ t5
				pDevice->CopyDescriptorsSimple(6, dstHandle, pCurMesh->Material.Albedo.GetSRVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dstHandle.Offset(6, CBV_SRV_DESCRIPTOR_SIZE);

				// t6
				pDevice->CopyDescriptorsSimple(1, dstHandle, pCurMesh->Material.Height.GetSRVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable);
			}
			break;

			case RenderPSOType_StencilMask:
			case RenderPSOType_DepthOnlyDefault: 
			case RenderPSOType_DepthOnlyCubeDefault: 
			case RenderPSOType_DepthOnlyCascadeDefault: 
			{
				hr = pDynamicDescriptorPool->AllocDescriptorTable(&cpuDescriptorTable, &gpuDescriptorTable, 2);
				BREAK_IF_FAILED(hr);

				CD3DX12_CPU_DESCRIPTOR_HANDLE dstHandle(cpuDescriptorTable, 0, CBV_SRV_DESCRIPTOR_SIZE);

				// b2, b3
				// pDevice->CopyDescriptorsSimple(2, dstHandle, pCurMesh->MeshConstant.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
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
		pCommandList->IASetIndexBuffer(&(pCurMesh->Index.IndexBufferView));
		pCommandList->DrawIndexedInstanced(pCurMesh->Index.Count, 1, 0, 0, 0);
	}
}

void Model::Render(UINT threadIndex, ID3D12GraphicsCommandList* pCommandList, DynamicDescriptorPool* pDescriptorPool, ResourceManager* pManager, int psoSetting)
{
	_ASSERT(pCommandList);
	_ASSERT(pManager);
	_ASSERT(pDescriptorPool);

	HRESULT hr = S_OK;
	ID3D12Device5* pDevice = pManager->m_pDevice;
	ConstantBufferPool* pMeshConstantBufferPool = pManager->m_pConstnatBufferManager->GetConstantBufferPool(ConstantBufferType_Mesh);
	ConstantBufferPool* pMaterialConstantBufferPool = pManager->m_pConstnatBufferManager->GetConstantBufferPool(ConstantBufferType_Material);
	const UINT CBV_SRV_DESCRIPTOR_SIZE = pManager->m_CBVSRVUAVDescriptorSize;

	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuDescriptorTable = {};
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuDescriptorTable = {};

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
			case RenderPSOType_Default: 
			case RenderPSOType_Skybox:
			case RenderPSOType_MirrorBlend: 
			case RenderPSOType_ReflectionDefault:
			case RenderPSOType_ReflectionSkybox:
			{
				hr = pDescriptorPool->AllocDescriptorTable(&cpuDescriptorTable, &gpuDescriptorTable, 9);
				BREAK_IF_FAILED(hr);

				CD3DX12_CPU_DESCRIPTOR_HANDLE dstHandle(cpuDescriptorTable, 0, CBV_SRV_DESCRIPTOR_SIZE);

				// b2, b3
				/*pDevice->CopyDescriptorsSimple(2, dstHandle, pCurMesh->MeshConstant.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dstHandle.Offset(2, CBV_SRV_DESCRIPTOR_SIZE);*/
				pDevice->CopyDescriptorsSimple(1, dstHandle, pMeshCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);
				pDevice->CopyDescriptorsSimple(1, dstHandle, pMaterialCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);

				// t0 ~ t5
				pDevice->CopyDescriptorsSimple(6, dstHandle, pCurMesh->Material.Albedo.GetSRVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dstHandle.Offset(6, CBV_SRV_DESCRIPTOR_SIZE);

				// t6
				pDevice->CopyDescriptorsSimple(1, dstHandle, pCurMesh->Material.Height.GetSRVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable);
			}
			break;

			case RenderPSOType_StencilMask:
			case RenderPSOType_DepthOnlyDefault:
			case RenderPSOType_DepthOnlyCubeDefault:
			case RenderPSOType_DepthOnlyCascadeDefault:
			{
				hr = pDescriptorPool->AllocDescriptorTable(&cpuDescriptorTable, &gpuDescriptorTable, 2);
				BREAK_IF_FAILED(hr);

				CD3DX12_CPU_DESCRIPTOR_HANDLE dstHandle(cpuDescriptorTable, 0, CBV_SRV_DESCRIPTOR_SIZE);

				// b2, b3
				// pDevice->CopyDescriptorsSimple(2, dstHandle, pCurMesh->MeshConstant.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
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
		pCommandList->IASetIndexBuffer(&(pCurMesh->Index.IndexBufferView));
		pCommandList->DrawIndexedInstanced(pCurMesh->Index.Count, 1, 0, 0, 0);
	}
}

void Model::RenderBoundingBox(Renderer* pRenderer, eRenderPSOType psoSetting)
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
	/*pDevice->CopyDescriptorsSimple(1, dstHandle, m_pBoundingBoxMesh->MeshConstant.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);
	pDevice->CopyDescriptorsSimple(1, dstHandle, m_pBoundingBoxMesh->MaterialConstant.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);*/
	pDevice->CopyDescriptorsSimple(1, dstHandle, pMeshCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);
	pDevice->CopyDescriptorsSimple(1, dstHandle, pMaterialCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);

	// t6(null)
	pDevice->CopyDescriptorsSimple(1, dstHandle, nullHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable);

	pCommandList->IASetVertexBuffers(0, 1, &m_pBoundingBoxMesh->Vertex.VertexBufferView);
	pCommandList->IASetIndexBuffer(&m_pBoundingBoxMesh->Index.IndexBufferView);
	pCommandList->DrawIndexedInstanced(m_pBoundingBoxMesh->Index.Count, 1, 0, 0, 0);
}

void Model::RenderBoundingSphere(Renderer* pRenderer, eRenderPSOType psoSetting)
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

	MeshConstant* pMeshConstantData = &m_pBoundingSphereMesh->MeshConstantData;
	MaterialConstant* pMaterialConstantData = &m_pBoundingSphereMesh->MaterialConstantData;
	CBInfo* pMeshCB = pMeshConstantBufferPool->AllocCB();
	CBInfo* pMaterialCB = pMaterialConstantBufferPool->AllocCB();

	// Upload constant buffer(mesh, material).
	BYTE* pMeshConstMem = pMeshCB->pSystemMemAddr;
	BYTE* pMaterialConstMem = pMaterialCB->pSystemMemAddr;
	memcpy(pMeshConstMem, &m_pBoundingSphereMesh->MeshConstantData, sizeof(m_pBoundingSphereMesh->MeshConstantData));
	memcpy(pMaterialConstMem, &m_pBoundingSphereMesh->MaterialConstantData, sizeof(m_pBoundingSphereMesh->MaterialConstantData));

	// b2, b3
	/*pDevice->CopyDescriptorsSimple(1, dstHandle, m_pBoundingSphereMesh->MeshConstant.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);
	pDevice->CopyDescriptorsSimple(1, dstHandle, m_pBoundingSphereMesh->MaterialConstant.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);*/
	pDevice->CopyDescriptorsSimple(1, dstHandle, pMeshCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);
	pDevice->CopyDescriptorsSimple(1, dstHandle, pMaterialCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);

	// t6(null)
	pDevice->CopyDescriptorsSimple(1, dstHandle, nullHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable);

	pCommandList->IASetVertexBuffers(0, 1, &m_pBoundingSphereMesh->Vertex.VertexBufferView);
	pCommandList->IASetIndexBuffer(&m_pBoundingSphereMesh->Index.IndexBufferView);
	pCommandList->DrawIndexedInstanced(m_pBoundingSphereMesh->Index.Count, 1, 0, 0, 0);
}

void Model::Cleanup()
{
	if (m_pBoundingSphereMesh)
	{
		delete m_pBoundingSphereMesh;
		m_pBoundingSphereMesh = nullptr;
	}
	if (m_pBoundingBoxMesh)
	{
		delete m_pBoundingBoxMesh;
		m_pBoundingBoxMesh = nullptr;
	}

	for (UINT64 i = 0, size = Meshes.size(); i < size; ++i)
	{
		delete Meshes[i];
		Meshes[i] = nullptr;
	}
	Meshes.clear();
}

void Model::SetDescriptorHeap(Renderer* pRenderer)
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

	// meshes.
	for (UINT64 i = 0, size = Meshes.size(); i < size; ++i)
	{
		Mesh* pCurMesh = Meshes[i];
		// ConstantBuffer& meshConstant = pCurMesh->MeshConstant;
		// ConstantBuffer& materialConstant = pCurMesh->MaterialConstant;
		Material* pMaterialBuffer = &pCurMesh->Material;

		/*cbvDesc.BufferLocation = meshConstant.GetGPUMemAddr();
		cbvDesc.SizeInBytes = (UINT)meshConstant.GetBufferSize();
		pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvLastHandle);
		meshConstant.SetCBVHandle(cbvSrvLastHandle);
		cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(pManager->m_CBVSRVUAVHeapSize);

		cbvDesc.BufferLocation = materialConstant.GetGPUMemAddr();
		cbvDesc.SizeInBytes = (UINT)materialConstant.GetBufferSize();
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

		pDevice->CreateShaderResourceView(pMaterialBuffer->Height.GetResource(), &srvDesc, cbvSrvLastHandle);
		pMaterialBuffer->Height.SetSRVHandle(cbvSrvLastHandle);
		cbvSrvLastHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(pManager->m_CBVSRVUAVHeapSize);
	}

	// bounding box
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

	// bounding sphere
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
}

void Model::initBoundingBox(Renderer* pRenderer, const std::vector<MeshInfo>& MESH_INFOS)
{
	BoundingBox = getBoundingBox(MESH_INFOS[0].Vertices);
	for (UINT64 i = 1, size = MESH_INFOS.size(); i < size; ++i)
	{
		DirectX::BoundingBox bb = getBoundingBox(MESH_INFOS[i].Vertices);
		extendBoundingBox(bb, &BoundingBox);
	}

	MeshInfo meshData = INIT_MESH_INFO;
	/*MeshConstant* pMeshConst = nullptr;
	MaterialConstant* pMaterialConst = nullptr;

	MakeWireBox(&meshData, BoundingBox.Center, Vector3(BoundingBox.Extents) + Vector3(1e-3f));
	m_pBoundingBoxMesh = new Mesh;
	m_pBoundingBoxMesh->MeshConstant.Initialize(pRenderer, sizeof(MeshConstant));
	m_pBoundingBoxMesh->MaterialConstant.Initialize(pRenderer, sizeof(MaterialConstant));
	pMeshConst = (MeshConstant*)m_pBoundingBoxMesh->MeshConstant.pData;
	pMaterialConst = (MaterialConstant*)m_pBoundingBoxMesh->MaterialConstant.pData;

	pMeshConst->World = Matrix();*/

	MakeWireBox(&meshData, BoundingBox.Center, Vector3(BoundingBox.Extents) + Vector3(1e-3f));
	m_pBoundingBoxMesh = new Mesh;

	MeshConstant& meshConstantData = m_pBoundingBoxMesh->MeshConstantData;
	MaterialConstant& materialConstantData = m_pBoundingBoxMesh->MaterialConstantData;
	meshConstantData.World = Matrix();

	InitMeshBuffers(pRenderer, meshData, m_pBoundingBoxMesh);
}

void Model::initBoundingSphere(Renderer* pRenderer, const std::vector<MeshInfo>& MESH_INFOS)
{
	float maxRadius = 0.0f;
	for (UINT64 i = 0, size = MESH_INFOS.size(); i < size; ++i)
	{
		const MeshInfo& curMesh = MESH_INFOS[i];
		for (UINT64 j = 0, vertSize = curMesh.Vertices.size(); j < vertSize; ++j)
		{
			const Vertex& v = curMesh.Vertices[j];
			maxRadius = Max((Vector3(BoundingBox.Center) - v.Position).Length(), maxRadius);
		}
	}

	maxRadius += 1e-2f; // 살짝 크게 설정.
	BoundingSphere = DirectX::BoundingSphere(BoundingBox.Center, maxRadius);

	MeshInfo meshData = INIT_MESH_INFO;
	/*MeshConstant* pMeshConst = nullptr;
	MaterialConstant* pMaterialConst = nullptr;

	MakeWireSphere(&meshData, BoundingSphere.Center, BoundingSphere.Radius);
	// MakeWireCapsule(&meshData, BoundingSphere.Center, BoundingSphere.Radius, 3.0f);
	m_pBoundingSphereMesh = new Mesh;
	m_pBoundingSphereMesh->MeshConstant.Initialize(pRenderer, sizeof(MeshConstant));
	m_pBoundingSphereMesh->MaterialConstant.Initialize(pRenderer, sizeof(MaterialConstant));
	pMeshConst = (MeshConstant*)m_pBoundingSphereMesh->MeshConstant.pData;
	pMaterialConst = (MaterialConstant*)m_pBoundingSphereMesh->MaterialConstant.pData;

	pMeshConst->World = Matrix();*/

	MakeWireSphere(&meshData, BoundingSphere.Center, BoundingSphere.Radius);
	m_pBoundingSphereMesh = new Mesh;

	MeshConstant& meshConstantData = m_pBoundingSphereMesh->MeshConstantData;
	MaterialConstant& materialConstantData = m_pBoundingSphereMesh->MaterialConstantData;
	meshConstantData.World = Matrix();

	InitMeshBuffers(pRenderer, meshData, m_pBoundingSphereMesh);
}

DirectX::BoundingBox Model::getBoundingBox(const std::vector<Vertex>& VERTICES)
{
	using DirectX::SimpleMath::Vector3;

	if (VERTICES.size() == 0)
	{
		return DirectX::BoundingBox();
	}

	Vector3 minCorner = VERTICES[0].Position;
	Vector3 maxCorner = VERTICES[0].Position;

	for (UINT64 i = 1, size = VERTICES.size(); i < size; ++i)
	{
		minCorner = Vector3::Min(minCorner, VERTICES[i].Position);
		maxCorner = Vector3::Max(maxCorner, VERTICES[i].Position);
	}

	Vector3 center = (minCorner + maxCorner) * 0.5f;
	Vector3 extents = maxCorner - center;

	return DirectX::BoundingBox(center, extents);
}

void Model::extendBoundingBox(const DirectX::BoundingBox& SRC_BOX, DirectX::BoundingBox* pDestBox)
{
	using DirectX::SimpleMath::Vector3;

	Vector3 minCorner = Vector3(SRC_BOX.Center) - Vector3(SRC_BOX.Extents);
	Vector3 maxCorner = Vector3(SRC_BOX.Center) - Vector3(SRC_BOX.Extents);

	minCorner = Vector3::Min(minCorner, Vector3(pDestBox->Center) - Vector3(pDestBox->Extents));
	maxCorner = Vector3::Max(maxCorner, Vector3(pDestBox->Center) + Vector3(pDestBox->Extents));

	pDestBox->Center = (minCorner + maxCorner) * 0.5f;
	pDestBox->Extents = maxCorner - pDestBox->Center;
}
