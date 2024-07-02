#include "../pch.h"
#include "../Graphics/ConstantDataType.h"
#include "../Graphics/GraphicsUtil.h"
#include "GeometryGenerator.h"
#include "../Util/Utility.h"
#include "Model.h"

DirectX::BoundingBox GetBoundingBox(const std::vector<Vertex>& VERTICES)
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
void ExtendBoundingBox(const DirectX::BoundingBox& SRC_BOX, DirectX::BoundingBox* pDestBox)
{
	using DirectX::SimpleMath::Vector3;

	Vector3 minCorner = Vector3(SRC_BOX.Center) - Vector3(SRC_BOX.Extents);
	Vector3 maxCorner = Vector3(SRC_BOX.Center) - Vector3(SRC_BOX.Extents);

	minCorner = Vector3::Min(minCorner, Vector3(pDestBox->Center) - Vector3(pDestBox->Extents));
	maxCorner = Vector3::Max(maxCorner, Vector3(pDestBox->Center) + Vector3(pDestBox->Extents));

	pDestBox->Center = (minCorner + maxCorner) * 0.5f;
	pDestBox->Extents = maxCorner - pDestBox->Center;
}


Model::Model(ResourceManager* pManager, std::wstring& basePath, std::wstring& fileName)
{
	Initialize(pManager, basePath, fileName);
}

Model::Model(ResourceManager* pManager, const std::vector<MeshInfo>&MESH_INFOS)
{
	Initialize(pManager, MESH_INFOS);
}

void Model::Initialize(ResourceManager* pManager, std::wstring& basePath, std::wstring& fileName)
{
	std::vector<MeshInfo> meshInfos;
	ReadFromFile(meshInfos, basePath, fileName);
	Initialize(pManager, meshInfos);
}

void Model::Initialize(ResourceManager* pManager, const std::vector<MeshInfo>& MESH_INFOS)
{
	_ASSERT(pManager);

	HRESULT hr = S_OK;
	struct _stat64 sourceFileStat;
	ID3D12Device5* pDevice = pManager->m_pDevice;
	ID3D12GraphicsCommandList* pCommandList = pManager->m_pSingleCommandList;

	Meshes.reserve(MESH_INFOS.size());

	for (UINT64 i = 0, meshSize = MESH_INFOS.size(); i < meshSize; ++i)
	{
		const MeshInfo& MESH_DATA = MESH_INFOS[i];
		Mesh* pNewMesh = (Mesh*)malloc(sizeof(Mesh));
		MeshConstant* pMeshConst = nullptr;
		MaterialConstant* pMaterialConst = nullptr;

		*pNewMesh = INIT_MESH;

		pNewMesh->pMaterialBuffer = (Material*)malloc(sizeof(Material));
		*(pNewMesh->pMaterialBuffer) = INIT_MATERIAL;

		pNewMesh->MeshConstant.Initialize(pManager, sizeof(MeshConstant));
		pNewMesh->MaterialConstant.Initialize(pManager, sizeof(MaterialConstant));
		pMeshConst = (MeshConstant*)pNewMesh->MeshConstant.pData;
		pMaterialConst = (MaterialConstant*)pNewMesh->MaterialConstant.pData;

		pMeshConst->World = Matrix();
		
		InitMeshBuffers(pManager, MESH_DATA, pNewMesh);

		if (!MESH_DATA.szAlbedoTextureFileName.empty())
		{
			std::string albedoTextureA(MESH_DATA.szAlbedoTextureFileName.begin(), MESH_DATA.szAlbedoTextureFileName.end());

			if (_stat64(albedoTextureA.c_str(), &sourceFileStat) != -1)
			{
				pNewMesh->pMaterialBuffer->Albedo.Initialize(pManager, MESH_DATA.szAlbedoTextureFileName.c_str(), true);
				pMaterialConst->bUseAlbedoMap = TRUE;
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
				pNewMesh->pMaterialBuffer->Emissive.Initialize(pManager, MESH_DATA.szEmissiveTextureFileName.c_str(), true);
				pMaterialConst->bUseEmissiveMap = TRUE;
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
				pNewMesh->pMaterialBuffer->Normal.Initialize(pManager, MESH_DATA.szNormalTextureFileName.c_str(), false);
				pMaterialConst->bUseNormalMap = TRUE;
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
				pNewMesh->pMaterialBuffer->Height.Initialize(pManager, MESH_DATA.szHeightTextureFileName.c_str(), false);
				pMeshConst->bUseHeightMap = TRUE;
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
				pNewMesh->pMaterialBuffer->AmbientOcclusion.Initialize(pManager, MESH_DATA.szAOTextureFileName.c_str(), false);
				pMaterialConst->bUseAOMap = TRUE;
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
				pNewMesh->pMaterialBuffer->Metallic.Initialize(pManager, MESH_DATA.szMetallicTextureFileName.c_str(), false);
				pMaterialConst->bUseMetallicMap = TRUE;
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
				pNewMesh->pMaterialBuffer->Roughness.Initialize(pManager, MESH_DATA.szRoughnessTextureFileName.c_str(), false);
				pMaterialConst->bUseRoughnessMap = TRUE;
			}
			else
			{
				OutputDebugStringW(MESH_DATA.szRoughnessTextureFileName.c_str());
				OutputDebugStringA(" does not exists. Skip texture reading.\n");
			}
		}

		// physx rigid body 추가.
		// mesh가 각 bone 마다 분리되어 있는 것이 아님.
		// PxRigidBody를 적용하기 위해서는 animation transform 초기화 혹은 bone 초기화 시 적용해야 할듯.
		// 

		Meshes.push_back(pNewMesh);
	}

	// Bounding box 초기화.
	{
		BoundingBox = GetBoundingBox(MESH_INFOS[0].Vertices);
		for (UINT64 i = 1, size = MESH_INFOS.size(); i < size; ++i)
		{
			DirectX::BoundingBox bb = GetBoundingBox(MESH_INFOS[0].Vertices);
			ExtendBoundingBox(bb, &BoundingBox);
		}

		MeshInfo meshData = INIT_MESH_INFO;
		MeshConstant* pMeshConst = nullptr;
		MaterialConstant* pMaterialConst = nullptr;

		MakeWireBox(&meshData, BoundingBox.Center, Vector3(BoundingBox.Extents) + Vector3(1e-3f));
		m_pBoundingBoxMesh = (Mesh*)malloc(sizeof(Mesh));
		*m_pBoundingBoxMesh = INIT_MESH;

		m_pBoundingBoxMesh->pMaterialBuffer = (Material*)malloc(sizeof(Material));
		*(m_pBoundingBoxMesh->pMaterialBuffer) = INIT_MATERIAL;

		m_pBoundingBoxMesh->MeshConstant.Initialize(pManager, sizeof(MeshConstant));
		m_pBoundingBoxMesh->MaterialConstant.Initialize(pManager, sizeof(MaterialConstant));
		pMeshConst = (MeshConstant*)m_pBoundingBoxMesh->MeshConstant.pData;
		pMaterialConst = (MaterialConstant*)m_pBoundingBoxMesh->MaterialConstant.pData;

		pMeshConst->World = Matrix();

		InitMeshBuffers(pManager, meshData, m_pBoundingBoxMesh);
	}

	// Bounding sphere 초기화.
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
		MeshConstant* pMeshConst = nullptr;
		MaterialConstant* pMaterialConst = nullptr;

		MakeWireSphere(&meshData, BoundingSphere.Center, BoundingSphere.Radius);
		m_pBoundingSphereMesh = (Mesh*)malloc(sizeof(Mesh));
		*m_pBoundingSphereMesh = INIT_MESH;

		m_pBoundingSphereMesh->pMaterialBuffer = (Material*)malloc(sizeof(Material));
		*(m_pBoundingSphereMesh->pMaterialBuffer) = INIT_MATERIAL;

		m_pBoundingSphereMesh->MeshConstant.Initialize(pManager, sizeof(MeshConstant));
		m_pBoundingSphereMesh->MaterialConstant.Initialize(pManager, sizeof(MaterialConstant));
		pMeshConst = (MeshConstant*)m_pBoundingSphereMesh->MeshConstant.pData;
		pMaterialConst = (MaterialConstant*)m_pBoundingSphereMesh->MaterialConstant.pData;

		pMeshConst->World = Matrix();

		InitMeshBuffers(pManager, meshData, m_pBoundingSphereMesh);
	}
}

void Model::InitMeshBuffers(ResourceManager* pManager, const MeshInfo& MESH_INFO, Mesh* pNewMesh)
{
	_ASSERT(pManager);

	HRESULT hr = S_OK;

	// vertex buffer.
	{
		hr = pManager->CreateVertexBuffer(sizeof(Vertex),
										  (UINT)MESH_INFO.Vertices.size(),
										  &(pNewMesh->VertexBufferView),
										  &(pNewMesh->pVertexBuffer),
										  (void*)MESH_INFO.Vertices.data());
		BREAK_IF_FAILED(hr);
		pNewMesh->VertexCount = (UINT)MESH_INFO.Vertices.size();
	}

	// index buffer.
	{
		hr = pManager->CreateIndexBuffer(sizeof(UINT),
										 (UINT)MESH_INFO.Indices.size(),
										 &(pNewMesh->IndexBufferView),
										 &(pNewMesh->pIndexBuffer),
										 (void*)MESH_INFO.Indices.data());
		BREAK_IF_FAILED(hr);
		pNewMesh->IndexCount = (UINT)MESH_INFO.Indices.size();
	}
}

void Model::UpdateConstantBuffers()
{
	if (bIsVisible == false)
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
	m_pBoundingSphereMesh->MeshConstant.Upload();
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

	MeshConstant* pBoxMeshConst = (MeshConstant*)m_pBoundingBoxMesh->MeshConstant.pData;
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
	}
}

void Model::Render(ResourceManager* pManager, ePipelineStateSetting psoSetting)
{
	if (!bIsVisible)
	{
		return;
	}

	_ASSERT(pManager);

	HRESULT hr = S_OK;

	ID3D12Device5* pDevice = pManager->m_pDevice;
	ID3D12GraphicsCommandList* pCommandList = pManager->m_pSingleCommandList;
	DynamicDescriptorPool* pDynamicDescriptorPool = pManager->m_pDynamicDescriptorPool;
	const UINT CBV_SRV_DESCRIPTOR_SIZE = pManager->m_CBVSRVUAVDescriptorSize;

	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuDescriptorTable = {};
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuDescriptorTable = {};

	for (UINT64 i = 0, size = Meshes.size(); i < size; ++i)
	{
		Mesh* pCurMesh = Meshes[i];

		switch (psoSetting)
		{
		case Default: case Skybox:
		case MirrorBlend: case ReflectionDefault: case ReflectionSkybox:
		{
			hr = pDynamicDescriptorPool->AllocDescriptorTable(&cpuDescriptorTable, &gpuDescriptorTable, 9);
			BREAK_IF_FAILED(hr);

			CD3DX12_CPU_DESCRIPTOR_HANDLE dstHandle(cpuDescriptorTable, 0, CBV_SRV_DESCRIPTOR_SIZE);
			
			// b2, b3
			pDevice->CopyDescriptorsSimple(2, dstHandle, pCurMesh->MeshConstant.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(2, CBV_SRV_DESCRIPTOR_SIZE);

			// t0 ~ t5
			pDevice->CopyDescriptorsSimple(6, dstHandle, pCurMesh->pMaterialBuffer->Albedo.GetSRVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(6, CBV_SRV_DESCRIPTOR_SIZE);

			// t6
			pDevice->CopyDescriptorsSimple(1, dstHandle, pCurMesh->pMaterialBuffer->Height.GetSRVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		
			pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable);
		}
		break;

		case DepthOnlyDefault: case DepthOnlyCubeDefault: case DepthOnlyCascadeDefault: case StencilMask:
		{
			hr = pDynamicDescriptorPool->AllocDescriptorTable(&cpuDescriptorTable, &gpuDescriptorTable, 2);
			BREAK_IF_FAILED(hr);

			CD3DX12_CPU_DESCRIPTOR_HANDLE dstHandle(cpuDescriptorTable, 0, CBV_SRV_DESCRIPTOR_SIZE);

			// b2, b3
			pDevice->CopyDescriptorsSimple(2, dstHandle, pCurMesh->MeshConstant.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable);
		}
		break;

		default:
			__debugbreak();
			break;
		}

		pCommandList->IASetVertexBuffers(0, 1, &(pCurMesh->VertexBufferView));
		pCommandList->IASetIndexBuffer(&(pCurMesh->IndexBufferView));
		pCommandList->DrawIndexedInstanced(pCurMesh->IndexCount, 1, 0, 0, 0);
	}
}

void Model::Render(ResourceManager* pManager, ID3D12GraphicsCommandList* pCommandList, ePipelineStateSetting psoSetting)
{
	if (!bIsVisible)
	{
		return;
	}

	_ASSERT(pManager);
	_ASSERT(pCommandList);

	HRESULT hr = S_OK;

	ID3D12Device5* pDevice = pManager->m_pDevice;
	DynamicDescriptorPool* pDynamicDescriptorPool = pManager->m_pDynamicDescriptorPool;
	const UINT CBV_SRV_DESCRIPTOR_SIZE = pManager->m_CBVSRVUAVDescriptorSize;

	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuDescriptorTable;
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuDescriptorTable;

	for (UINT64 i = 0, size = Meshes.size(); i < size; ++i)
	{
		Mesh* pCurMesh = Meshes[i];

		switch (psoSetting)
		{
			case Default: case Skybox: case MirrorBlend: 
			case ReflectionDefault: case ReflectionSkybox:
			{
				hr = pDynamicDescriptorPool->AllocDescriptorTable(&cpuDescriptorTable, &gpuDescriptorTable, 9);
				BREAK_IF_FAILED(hr);

				CD3DX12_CPU_DESCRIPTOR_HANDLE dstHandle(cpuDescriptorTable, 0, CBV_SRV_DESCRIPTOR_SIZE);

				// b2, b3
				pDevice->CopyDescriptorsSimple(2, dstHandle, pCurMesh->MeshConstant.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dstHandle.Offset(2, CBV_SRV_DESCRIPTOR_SIZE);

				// t0 ~ t5
				pDevice->CopyDescriptorsSimple(6, dstHandle, pCurMesh->pMaterialBuffer->Albedo.GetSRVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dstHandle.Offset(6, CBV_SRV_DESCRIPTOR_SIZE);

				// t6
				pDevice->CopyDescriptorsSimple(1, dstHandle, pCurMesh->pMaterialBuffer->Height.GetSRVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable);
			}
			break;

			case DepthOnlyDefault: case DepthOnlyCubeDefault: case DepthOnlyCascadeDefault: 
			case StencilMask:
			{
				hr = pDynamicDescriptorPool->AllocDescriptorTable(&cpuDescriptorTable, &gpuDescriptorTable, 2);
				BREAK_IF_FAILED(hr);

				CD3DX12_CPU_DESCRIPTOR_HANDLE dstHandle(cpuDescriptorTable, 0, CBV_SRV_DESCRIPTOR_SIZE);

				// b2, b3
				pDevice->CopyDescriptorsSimple(2, dstHandle, pCurMesh->MeshConstant.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable);
			}
			break;

			default:
				__debugbreak();
				break;
		}

		pCommandList->IASetVertexBuffers(0, 1, &(pCurMesh->VertexBufferView));
		pCommandList->IASetIndexBuffer(&(pCurMesh->IndexBufferView));
		pCommandList->DrawIndexedInstanced(pCurMesh->IndexCount, 1, 0, 0, 0);
	}
}

void Model::Clear()
{
	if (m_pBoundingSphereMesh)
	{
		ReleaseMesh(&m_pBoundingSphereMesh);
	}
	if (m_pBoundingBoxMesh)
	{
		ReleaseMesh(&m_pBoundingBoxMesh);
	}

	for (UINT64 i = 0, size = Meshes.size(); i < size; ++i)
	{
		ReleaseMesh(&Meshes[i]);
	}
	Meshes.clear();
}

void Model::SetDescriptorHeap(ResourceManager* pManager)
{
	_ASSERT(pManager);

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

	for (UINT64 i = 0, size = Meshes.size(); i < size; ++i)
	{
		Mesh* pCurMesh = Meshes[i];
		ConstantBuffer& meshConstant = pCurMesh->MeshConstant;
		ConstantBuffer& materialConstant = pCurMesh->MaterialConstant;
		Material* pMaterialBuffer = pCurMesh->pMaterialBuffer;

		cbvDesc.BufferLocation = meshConstant.GetGPUMemAddr();
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
		++(pManager->m_CBVSRVUAVHeapSize);

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
}
