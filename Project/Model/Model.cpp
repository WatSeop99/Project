#include "../pch.h"
#include "../Renderer/ConstantDataType.h"
#include "../Graphics/GraphicsUtil.h"
#include "GeometryGenerator.h"
#include "../Util/Utility.h"
#include "Model.h"

void Model::Initialize(Renderer* pRenderer, std::wstring& basePath, std::wstring& fileName)
{
	std::vector<MeshInfo> meshInfos;
	ReadFromFile(meshInfos, basePath, fileName);
	Initialize(pRenderer, meshInfos);
}

void Model::Initialize(Renderer* pRenderer, const std::vector<MeshInfo>& MESH_INFOS)
{
	_ASSERT(pRenderer);

	m_pRenderer = pRenderer;

	HRESULT hr = S_OK;
	struct _stat64 sourceFileStat = {};

	ResourceManager* pResourceManager = pRenderer->GetResourceManager();
	TextureManager* pTextureManager = pRenderer->GetTextureManager();
	
	ID3D12Device5* pDevice = pRenderer->GetD3DDevice();
	ID3D12GraphicsCommandList* pCommandList = pRenderer->GetCommandList();

	Meshes.reserve(MESH_INFOS.size());

	for (UINT64 i = 0, meshSize = MESH_INFOS.size(); i < meshSize; ++i)
	{
		const MeshInfo& MESH_DATA = MESH_INFOS[i];
		Mesh* pNewMesh = new Mesh;
		pNewMesh->Initialize();

		MeshConstant& meshConstantData = pNewMesh->MeshConstantData;
		MaterialConstant& materialConstantData = pNewMesh->MaterialConstantData;
		Material* pMeshMaterial = &pNewMesh->Material;

		pMeshMaterial->pAlbedo = nullptr;
		pMeshMaterial->pEmissive = nullptr;
		pMeshMaterial->pNormal = nullptr;
		pMeshMaterial->pHeight = nullptr;
		pMeshMaterial->pAmbientOcclusion = nullptr;
		pMeshMaterial->pMetallic = nullptr;
		pMeshMaterial->pRoughness = nullptr;

		InitMeshBuffers(pRenderer, MESH_DATA, pNewMesh);

		if (!MESH_DATA.szAlbedoTextureFileName.empty())
		{
			std::string albedoTextureA(MESH_DATA.szAlbedoTextureFileName.begin(), MESH_DATA.szAlbedoTextureFileName.end());

			if (_stat64(albedoTextureA.c_str(), &sourceFileStat) != -1)
			{
				pMeshMaterial->pAlbedo = pTextureManager->CreateTextureFromFile(MESH_DATA.szAlbedoTextureFileName.c_str(), true);
				materialConstantData.bUseAlbedoMap = TRUE;
			}
			else
			{
				WCHAR szDebugString[256];
				swprintf_s(szDebugString, 256, L"%s does not exists. Skip texture reading.\n", MESH_DATA.szAlbedoTextureFileName.c_str());
				OutputDebugStringW(szDebugString);
			}
		}
		if (!MESH_DATA.szEmissiveTextureFileName.empty())
		{
			std::string emissiveTextureA(MESH_DATA.szEmissiveTextureFileName.begin(), MESH_DATA.szEmissiveTextureFileName.end());

			if (_stat64(emissiveTextureA.c_str(), &sourceFileStat) != -1)
			{
				pMeshMaterial->pEmissive = pTextureManager->CreateTextureFromFile(MESH_DATA.szEmissiveTextureFileName.c_str(), true);
				materialConstantData.bUseEmissiveMap = TRUE;
			}
			else
			{
				WCHAR szDebugString[256];
				swprintf_s(szDebugString, 256, L"%s does not exists. Skip texture reading.\n", MESH_DATA.szEmissiveTextureFileName.c_str());
				OutputDebugStringW(szDebugString);
			}
		}
		if (!MESH_DATA.szNormalTextureFileName.empty())
		{
			std::string normalTextureA(MESH_DATA.szNormalTextureFileName.begin(), MESH_DATA.szNormalTextureFileName.end());

			if (_stat64(normalTextureA.c_str(), &sourceFileStat) != -1)
			{
				pMeshMaterial->pNormal = pTextureManager->CreateTextureFromFile(MESH_DATA.szNormalTextureFileName.c_str(), false);
				materialConstantData.bUseNormalMap = TRUE;
			}
			else
			{
				WCHAR szDebugString[256];
				swprintf_s(szDebugString, 256, L"%s does not exists. Skip texture reading.\n", MESH_DATA.szNormalTextureFileName.c_str());
				OutputDebugStringW(szDebugString);
			}
		}
		if (!MESH_DATA.szHeightTextureFileName.empty())
		{
			std::string heightTextureA(MESH_DATA.szHeightTextureFileName.begin(), MESH_DATA.szHeightTextureFileName.end());

			if (_stat64(heightTextureA.c_str(), &sourceFileStat) != -1)
			{
				pMeshMaterial->pHeight = pTextureManager->CreateTextureFromFile(MESH_DATA.szHeightTextureFileName.c_str(), false);
				meshConstantData.bUseHeightMap = TRUE;
			}
			else
			{
				WCHAR szDebugString[256];
				swprintf_s(szDebugString, 256, L"%s does not exists. Skip texture reading.\n", MESH_DATA.szHeightTextureFileName.c_str());
				OutputDebugStringW(szDebugString);
			}
		}
		if (!MESH_DATA.szAOTextureFileName.empty())
		{
			std::string aoTextureA(MESH_DATA.szAOTextureFileName.begin(), MESH_DATA.szAOTextureFileName.end());

			if (_stat64(aoTextureA.c_str(), &sourceFileStat) != -1)
			{
				pMeshMaterial->pAmbientOcclusion = pTextureManager->CreateTextureFromFile(MESH_DATA.szAOTextureFileName.c_str(), false);
				materialConstantData.bUseAOMap = TRUE;
			}
			else
			{
				WCHAR szDebugString[256];
				swprintf_s(szDebugString, 256, L"%s does not exists. Skip texture reading.\n", MESH_DATA.szAOTextureFileName.c_str());
				OutputDebugStringW(szDebugString);
			}
		}
		if (!MESH_DATA.szMetallicTextureFileName.empty())
		{
			std::string metallicTextureA(MESH_DATA.szMetallicTextureFileName.begin(), MESH_DATA.szMetallicTextureFileName.end());

			if (_stat64(metallicTextureA.c_str(), &sourceFileStat) != -1)
			{
				pMeshMaterial->pMetallic = pTextureManager->CreateTextureFromFile(MESH_DATA.szMetallicTextureFileName.c_str(), false);
				materialConstantData.bUseMetallicMap = TRUE;
			}
			else
			{
				WCHAR szDebugString[256];
				swprintf_s(szDebugString, 256, L"%s does not exists. Skip texture reading.\n", MESH_DATA.szMetallicTextureFileName.c_str());
				OutputDebugStringW(szDebugString);
			}
		}
		if (!MESH_DATA.szRoughnessTextureFileName.empty())
		{
			std::string roughnessTextureA(MESH_DATA.szRoughnessTextureFileName.begin(), MESH_DATA.szRoughnessTextureFileName.end());

			if (_stat64(roughnessTextureA.c_str(), &sourceFileStat) != -1)
			{
				pMeshMaterial->pRoughness = pTextureManager->CreateTextureFromFile(MESH_DATA.szRoughnessTextureFileName.c_str(), false);
				materialConstantData.bUseRoughnessMap = TRUE;
			}
			else
			{
				WCHAR szDebugString[256];
				swprintf_s(szDebugString, 256, L"%s does not exists. Skip texture reading.\n", MESH_DATA.szRoughnessTextureFileName.c_str());
				OutputDebugStringW(szDebugString);
			}
		}

		Meshes.push_back(pNewMesh);
	}

	initBoundingBox(MESH_INFOS);
	initBoundingSphere(MESH_INFOS);
}

void Model::InitMeshBuffers(Renderer* pRenderer, const MeshInfo& MESH_INFO, Mesh* pNewMesh)
{
	_ASSERT(pRenderer);
	_ASSERT(pNewMesh);

	HRESULT hr = S_OK;
	ResourceManager* pResourceManager = pRenderer->GetResourceManager();

	// Create vertex buffer.
	hr = pResourceManager->CreateVertexBuffer(sizeof(Vertex),
											  (UINT)MESH_INFO.Vertices.size(),
											  &pNewMesh->Vertex.VertexBufferView,
											  &pNewMesh->Vertex.pBuffer,
											  (void*)MESH_INFO.Vertices.data());
	BREAK_IF_FAILED(hr);
	pNewMesh->Vertex.Count = (UINT)MESH_INFO.Vertices.size();

	// Create index buffer.
	hr = pResourceManager->CreateIndexBuffer(sizeof(UINT),
											 (UINT)MESH_INFO.Indices.size(),
											 &pNewMesh->Index.IndexBufferView,
											 &pNewMesh->Index.pBuffer,
											 (void*)MESH_INFO.Indices.data());
	BREAK_IF_FAILED(hr);
	pNewMesh->Index.Count = (UINT)MESH_INFO.Indices.size();
}

void Model::UpdateWorld(const Matrix& WORLD)
{
	World = WORLD;
	InverseWorldTranspose = WORLD;
	// InverseWorldTranspose.Translation(Vector3(0.0f));
	InverseWorldTranspose = InverseWorldTranspose.Invert().Transpose();

	// bounding box, sphere 위치 업데이트.
	BoundingSphere.Center = World.Translation();
	BoundingBox.Center = BoundingSphere.Center;

	MeshConstant& boxMeshConstantData = m_pBoundingBoxMesh->MeshConstantData;
	MeshConstant& sphereMeshConstantData = m_pBoundingSphereMesh->MeshConstantData;

	boxMeshConstantData.World = World.Transpose();
	boxMeshConstantData.InverseWorldTranspose = InverseWorldTranspose.Transpose();
	boxMeshConstantData.InverseWorld = InverseWorldTranspose;
	sphereMeshConstantData.World = boxMeshConstantData.World;
	sphereMeshConstantData.InverseWorldTranspose = boxMeshConstantData.InverseWorldTranspose;
	sphereMeshConstantData.InverseWorld = boxMeshConstantData.InverseWorld;

	for (UINT64 i = 0, size = Meshes.size(); i < size; ++i)
	{
		Mesh* pCurMesh = Meshes[i];
		MeshConstant& meshConstantData = pCurMesh->MeshConstantData;

		meshConstantData.World = WORLD.Transpose();
		meshConstantData.InverseWorldTranspose = InverseWorldTranspose.Transpose();
		meshConstantData.InverseWorld = InverseWorldTranspose;
	}
}

void Model::Render(eRenderPSOType psoSetting)
{
	_ASSERT(m_pRenderer);

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

			case RenderPSOType_StencilMask:
			case RenderPSOType_DepthOnlyDefault:
			case RenderPSOType_DepthOnlyCubeDefault:
			case RenderPSOType_DepthOnlyCascadeDefault:
			{
				hr = pDynamicDescriptorPool->AllocDescriptorTable(&cpuDescriptorTable, &gpuDescriptorTable, 2);
				BREAK_IF_FAILED(hr);

				CD3DX12_CPU_DESCRIPTOR_HANDLE dstHandle(cpuDescriptorTable, 0, CBV_SRV_DESCRIPTOR_SIZE);

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
		pCommandList->IASetIndexBuffer(&(pCurMesh->Index.IndexBufferView));
		pCommandList->DrawIndexedInstanced(pCurMesh->Index.Count, 1, 0, 0, 0);
	}
}

void Model::Render(UINT threadIndex, ID3D12GraphicsCommandList* pCommandList, DynamicDescriptorPool* pDescriptorPool, ConstantBufferManager* pConstantBufferManager, ResourceManager* pManager, int psoSetting)
{
	_ASSERT(pCommandList);
	_ASSERT(pManager);
	_ASSERT(pDescriptorPool);
	_ASSERT(pConstantBufferManager);

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

			case RenderPSOType_StencilMask:
			case RenderPSOType_DepthOnlyDefault:
			case RenderPSOType_DepthOnlyCubeDefault:
			case RenderPSOType_DepthOnlyCascadeDefault:
			{
				hr = pDescriptorPool->AllocDescriptorTable(&cpuDescriptorTable, &gpuDescriptorTable, 2);
				BREAK_IF_FAILED(hr);

				CD3DX12_CPU_DESCRIPTOR_HANDLE dstHandle(cpuDescriptorTable, 0, CBV_SRV_DESCRIPTOR_SIZE);

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
		pCommandList->IASetIndexBuffer(&(pCurMesh->Index.IndexBufferView));
		pCommandList->DrawIndexedInstanced(pCurMesh->Index.Count, 1, 0, 0, 0);
	}
}

void Model::RenderBoundingBox(eRenderPSOType psoSetting)
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

	pCommandList->IASetVertexBuffers(0, 1, &m_pBoundingBoxMesh->Vertex.VertexBufferView);
	pCommandList->IASetIndexBuffer(&m_pBoundingBoxMesh->Index.IndexBufferView);
	pCommandList->DrawIndexedInstanced(m_pBoundingBoxMesh->Index.Count, 1, 0, 0, 0);
}

void Model::RenderBoundingSphere(eRenderPSOType psoSetting)
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
	pDevice->CopyDescriptorsSimple(1, dstHandle, pMeshCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);
	pDevice->CopyDescriptorsSimple(1, dstHandle, pMaterialCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	dstHandle.Offset(1, CBV_SRV_DESCRIPTOR_SIZE);

	// t6(null)
	pDevice->CopyDescriptorsSimple(1, dstHandle, pResourceManager->NullSRVDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

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

	TextureManager* pTextureManager = m_pRenderer->GetTextureManager();
	for (UINT64 i = 0, size = Meshes.size(); i < size; ++i)
	{
		Mesh** pMesh = &Meshes[i];
		Material* pMaterial = &(*pMesh)->Material;

		if (pMaterial->pAlbedo)
		{
			pTextureManager->DeleteTexture(pMaterial->pAlbedo);
			pMaterial->pAlbedo = nullptr;
		}
		if (pMaterial->pEmissive)
		{
			pTextureManager->DeleteTexture(pMaterial->pEmissive);
			pMaterial->pEmissive = nullptr;
		}
		if (pMaterial->pNormal)
		{
			pTextureManager->DeleteTexture(pMaterial->pNormal);
			pMaterial->pNormal = nullptr;
		}
		if (pMaterial->pHeight)
		{
			pTextureManager->DeleteTexture(pMaterial->pHeight);
			pMaterial->pHeight = nullptr;
		}
		if (pMaterial->pAmbientOcclusion)
		{
			pTextureManager->DeleteTexture(pMaterial->pAmbientOcclusion);
			pMaterial->pAmbientOcclusion = nullptr;
		}
		if (pMaterial->pMetallic)
		{
			pTextureManager->DeleteTexture(pMaterial->pMetallic);
			pMaterial->pMetallic = nullptr;
		}
		if (pMaterial->pRoughness)
		{
			pTextureManager->DeleteTexture(pMaterial->pRoughness);
			pMaterial->pRoughness = nullptr;
		}

		delete (*pMesh);
		*pMesh = nullptr;
	}
	Meshes.clear();
}

void Model::initBoundingBox(const std::vector<MeshInfo>& MESH_INFOS)
{
	BoundingBox = getBoundingBox(MESH_INFOS[0].Vertices);
	for (UINT64 i = 1, size = MESH_INFOS.size(); i < size; ++i)
	{
		DirectX::BoundingBox bb = getBoundingBox(MESH_INFOS[i].Vertices);
		extendBoundingBox(bb, &BoundingBox);
	}

	MeshInfo meshData = INIT_MESH_INFO;
	// MakeWireBox(&meshData, BoundingBox.Center, Vector3(BoundingBox.Extents) + Vector3(1e-3f));
	MakeWireBox(&meshData, Vector3(0.0f), Vector3(BoundingBox.Extents) + Vector3(1e-3f));
	
	m_pBoundingBoxMesh = new Mesh;
	m_pBoundingBoxMesh->Initialize();

	InitMeshBuffers(m_pRenderer, meshData, m_pBoundingBoxMesh);
}

void Model::initBoundingSphere(const std::vector<MeshInfo>& MESH_INFOS)
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
	MakeWireSphere(&meshData, Vector3(0.0f), BoundingSphere.Radius);
	
	m_pBoundingSphereMesh = new Mesh;
	m_pBoundingSphereMesh->Initialize();

	InitMeshBuffers(m_pRenderer, meshData, m_pBoundingSphereMesh);
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
