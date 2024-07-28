#include "../pch.h"
#include "../Util/Utility.h"
#include "ShadowMap.h"

void ShadowMap::Initialize(Renderer* pRenderer, UINT lightType)
{
	_ASSERT(pRenderer);

	m_pRenderer = pRenderer;
	m_LightType = lightType;

	TextureManager* pTextureManager = pRenderer->GetTextureManager();
	int screenDirSize = 0;

	D3D12_RESOURCE_DESC resourceDesc;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Alignment = 0;
	resourceDesc.Width = m_ShadowMapWidth;
	resourceDesc.Height = m_ShadowMapHeight;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ZeroMemory(&dsvDesc, sizeof(D3D12_DEPTH_STENCIL_VIEW_DESC));
	ZeroMemory(&srvDesc, sizeof(D3D12_SHADER_RESOURCE_VIEW_DESC));

	switch (m_LightType & m_TOTAL_LIGHT_TYPE)
	{
		case LIGHT_DIRECTIONAL:
			resourceDesc.DepthOrArraySize = 4;
			resourceDesc.Format = DXGI_FORMAT_R32_TYPELESS;
			// m_DirectionalLightShadowBuffer.Initialize(pRenderer, resourceDesc);

			dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
			dsvDesc.Texture2DArray.ArraySize = 4;

			srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			srvDesc.Texture2DArray.MipLevels = 1;
			srvDesc.Texture2DArray.ArraySize = 4;

			m_pDirectionalLightShadowBuffer = pTextureManager->CreateDepthStencilTexture(resourceDesc, dsvDesc, srvDesc);

			screenDirSize = 4;
			break;

		case LIGHT_POINT:
			resourceDesc.DepthOrArraySize = 6;
			resourceDesc.Format = DXGI_FORMAT_R32_TYPELESS;
			// m_PointLightShadowBuffer.Initialize(pRenderer, resourceDesc);

			dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
			dsvDesc.Texture2DArray.ArraySize = 6;

			srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			srvDesc.TextureCube.MipLevels = 1;

			m_pPointLightShadowBuffer = pTextureManager->CreateDepthStencilTexture(resourceDesc, dsvDesc, srvDesc);

			screenDirSize = 6;
			break;

		case LIGHT_SPOT:
			resourceDesc.DepthOrArraySize = 1;
			// dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
			resourceDesc.Format = DXGI_FORMAT_R32_TYPELESS;
			// m_SpotLightShadowBuffer.Initialize(pRenderer, resourceDesc);

			dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

			srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = 1;

			m_pSpotLightShadowBuffer = pTextureManager->CreateDepthStencilTexture(resourceDesc, dsvDesc, srvDesc);

			screenDirSize = 1;
			break;

		default:
			break;
	}

	for (int i = 0; i < screenDirSize; ++i)
	{
		m_pViewPorts[i] = { 0, 0, (float)m_ShadowMapWidth, (float)m_ShadowMapHeight, 0.0f, 1.0f };
		m_pScissorRects[i] = { 0, 0, (long)m_ShadowMapWidth, (long)m_ShadowMapHeight };
	}
}

void ShadowMap::Update(LightProperty& property, Camera& lightCam, Camera& mainCamera)
{
	_ASSERT(m_pRenderer);

	Matrix lightView;
	Matrix lightProjection = lightCam.GetProjection();

	switch (m_LightType & m_TOTAL_LIGHT_TYPE)
	{
		case LIGHT_DIRECTIONAL:
		{
			bool bOriginalFPS = mainCamera.bUseFirstPersonView;
			mainCamera.bUseFirstPersonView = true;

			Matrix camView = mainCamera.GetView();
			Matrix camProjection = mainCamera.GetProjection();
			Matrix lightSectionView;
			Matrix lightSectionProjection;
			Vector3 lightSectionPosition;

			for (int i = 0; i < 4; ++i)
			{
				GlobalConstant* pShadowGlobalConstantData = &m_ShadowConstantBufferDatas[i];
				
				calculateCascadeLightViewProjection(&lightSectionPosition, &lightSectionView, &lightSectionProjection, camView, camProjection, property.Direction, i);

				pShadowGlobalConstantData->EyeWorld = lightSectionPosition;
				pShadowGlobalConstantData->View = lightSectionView.Transpose();
				pShadowGlobalConstantData->Projection = lightSectionProjection.Transpose();
				pShadowGlobalConstantData->InverseProjection = lightSectionProjection.Invert().Transpose();
				pShadowGlobalConstantData->ViewProjection = (lightSectionView * lightSectionProjection).Transpose();

				m_ShadowConstantsBufferDataForGS.ViewProjects[i] = pShadowGlobalConstantData->ViewProjection;
			}

			mainCamera.bUseFirstPersonView = bOriginalFPS;
		}
		break;

		case LIGHT_POINT:
		{
			const Vector3 pVIEW_DIRs[6] = // cubemap view vector.
			{
				Vector3(1.0f, 0.0f, 0.0f),	// right
				Vector3(-1.0f, 0.0f, 0.0f), // left
				Vector3(0.0f, 1.0f, 0.0f),	// up
				Vector3(0.0f, -1.0f, 0.0f), // down
				Vector3(0.0f, 0.0f, 1.0f),	// front
				Vector3(0.0f, 0.0f, -1.0f)	// back
			};
			const Vector3 pUP_DIRs[6] = // 위에서 정의한 view vector에 대한 up vector.
			{
				Vector3(0.0f, 1.0f, 0.0f),
				Vector3(0.0f, 1.0f, 0.0f),
				Vector3(0.0f, 0.0f, -1.0f),
				Vector3(0.0f, 0.0f, 1.0f),
				Vector3(0.0f, 1.0f, 0.0f),
				Vector3(0.0f, 1.0f, 0.0f)
			};

			for (int i = 0; i < 6; ++i)
			{
				GlobalConstant* pShadowGlobalConstantData = &m_ShadowConstantBufferDatas[i];

				lightView = DirectX::XMMatrixLookAtLH(property.Position, property.Position + pVIEW_DIRs[i], pUP_DIRs[i]);

				pShadowGlobalConstantData->EyeWorld = property.Position;
				pShadowGlobalConstantData->View = lightView.Transpose();
				pShadowGlobalConstantData->Projection = lightProjection.Transpose();
				pShadowGlobalConstantData->InverseProjection = lightProjection.Invert().Transpose();
				pShadowGlobalConstantData->ViewProjection = (lightView * lightProjection).Transpose();

				m_ShadowConstantsBufferDataForGS.ViewProjects[i] = pShadowGlobalConstantData->ViewProjection;
			}
		}
		break;

		case LIGHT_SPOT:
		{
			GlobalConstant* pShadowGlobalConstantData = &m_ShadowConstantBufferDatas[0];

			lightView = DirectX::XMMatrixLookAtLH(property.Position, property.Position + property.Direction, lightCam.GetUpDir());

			pShadowGlobalConstantData->EyeWorld = property.Position;
			pShadowGlobalConstantData->View = lightView.Transpose();
			pShadowGlobalConstantData->Projection = lightProjection.Transpose();
			pShadowGlobalConstantData->InverseProjection = lightProjection.Invert().Transpose();
			pShadowGlobalConstantData->ViewProjection = (lightView * lightProjection).Transpose();
		}
		break;

		default:
			break;
	}
}

void ShadowMap::Render(std::vector<Model*>* pRenderObjects)
{
	_ASSERT(m_pRenderer);

	ResourceManager* pResourceManager = m_pRenderer->GetResourceManager();
	ID3D12GraphicsCommandList* pCommandList = pResourceManager->GetCommandList();
	ConstantBufferPool* pShadowConstantBufferGSPool = pResourceManager->m_pConstantBufferManager->GetConstantBufferPool(ConstantBufferType_ShadowConstant);
	ConstantBufferPool* pShadowConstantBufferPool = pResourceManager->m_pConstantBufferManager->GetConstantBufferPool(ConstantBufferType_GlobalConstant);
	const UINT DSV_DESCRIPTOR_SIZE = pResourceManager->m_DSVDescriptorSize;
	const UINT CBV_SRV_UAV_DESCRIPTOR_SIZE = pResourceManager->m_CBVSRVUAVDescriptorSize;

	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle;
	ID3D12Resource* pDepthStencilResource = nullptr;
	CD3DX12_RESOURCE_BARRIER beforeBarrier;
	CD3DX12_RESOURCE_BARRIER afterBarrier;
	eRenderPSOType pso;

	setShadowViewport(pCommandList);
	setShadowScissorRect(pCommandList);

	switch (m_LightType & m_TOTAL_LIGHT_TYPE)
	{
		case LIGHT_DIRECTIONAL:
			// dsvHandle = m_DirectionalLightShadowBuffer.GetDSVHandle();
			dsvHandle = m_pDirectionalLightShadowBuffer->DSVHandle;
			// pDepthStencilResource = m_DirectionalLightShadowBuffer.GetResource();
			pDepthStencilResource = m_pDirectionalLightShadowBuffer->pTextureResource;
			pso = RenderPSOType_DepthOnlyCascadeDefault;
			break;

		case LIGHT_POINT:
			// dsvHandle = m_PointLightShadowBuffer.GetDSVHandle();
			dsvHandle = m_pPointLightShadowBuffer->DSVHandle;
			// pDepthStencilResource = m_PointLightShadowBuffer.GetResource();
			pDepthStencilResource = m_pPointLightShadowBuffer->pTextureResource;
			pso = RenderPSOType_DepthOnlyCubeDefault;
			break;

		case LIGHT_SPOT:
			// dsvHandle = m_SpotLightShadowBuffer.GetDSVHandle();
			dsvHandle = m_pSpotLightShadowBuffer->DSVHandle;
			// pDepthStencilResource = m_SpotLightShadowBuffer.GetResource();
			pDepthStencilResource = m_pSpotLightShadowBuffer->pTextureResource;
			pso = RenderPSOType_DepthOnlyDefault;
			break;

		default:
			break;
	}
	_ASSERT(pDepthStencilResource);

	beforeBarrier = CD3DX12_RESOURCE_BARRIER::Transition(pDepthStencilResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	pCommandList->ResourceBarrier(1, &beforeBarrier);

	pCommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	pCommandList->OMSetRenderTargets(0, nullptr, FALSE, &dsvHandle);


	CBInfo* pShadowCBForGS = pShadowConstantBufferGSPool->AllocCB();
	CBInfo* pShadowCB = pShadowConstantBufferPool->AllocCB();

	// Upload constant buffer(mesh, material).
	BYTE* pShadowGSConstMem = pShadowCBForGS->pSystemMemAddr;
	BYTE* pShadowConstMem = pShadowCB->pSystemMemAddr;
	memcpy(pShadowGSConstMem, &m_ShadowConstantsBufferDataForGS, sizeof(ShadowConstant));
	memcpy(pShadowConstMem, &m_ShadowConstantBufferDatas[0], sizeof(GlobalConstant));


	for (UINT64 i = 0, size = pRenderObjects->size(); i < size; ++i)
	{
		Model* pModel = (*pRenderObjects)[i];

		if (!pModel->bIsVisible || !pModel->bCastShadow)
		{
			continue;
		}

		switch (pModel->ModelType)
		{
			case RenderObjectType_DefaultType:
			case RenderObjectType_MirrorType:
			{
				pResourceManager->SetCommonState(pso);
				
				if ((m_LightType & m_TOTAL_LIGHT_TYPE) == LIGHT_DIRECTIONAL || (m_LightType & m_TOTAL_LIGHT_TYPE) == LIGHT_POINT)
				{
					pCommandList->SetGraphicsRootConstantBufferView(1, pShadowCBForGS->GPUMemAddr);
				}
				else
				{
					pCommandList->SetGraphicsRootConstantBufferView(1, pShadowCB->GPUMemAddr);
				}
				
				pModel->Render(pso);
			}
			break;

			case RenderObjectType_SkinnedType:
			{
				SkinnedMeshModel* pCharacter = (SkinnedMeshModel*)pModel;
				pResourceManager->SetCommonState((eRenderPSOType)(pso + 1));
				
				if ((m_LightType & m_TOTAL_LIGHT_TYPE) == LIGHT_DIRECTIONAL || (m_LightType & m_TOTAL_LIGHT_TYPE) == LIGHT_POINT)
				{
					pCommandList->SetGraphicsRootConstantBufferView(1, pShadowCBForGS->GPUMemAddr);
				}
				else
				{
					pCommandList->SetGraphicsRootConstantBufferView(1, pShadowCB->GPUMemAddr);
				}
				
				pCharacter->Render((eRenderPSOType)(pso + 1));
			}
			break;

			default:
				break;
		}
	}

	afterBarrier = CD3DX12_RESOURCE_BARRIER::Transition(pDepthStencilResource, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);
	pCommandList->ResourceBarrier(1, &afterBarrier);
}

void ShadowMap::Cleanup()
{
	TextureManager* pTextureManager = m_pRenderer->GetTextureManager();

	switch (m_LightType & m_TOTAL_LIGHT_TYPE)
	{
		case LIGHT_DIRECTIONAL:
			// m_DirectionalLightShadowBuffer.Cleanup();
			pTextureManager->DeleteTexture(m_pDirectionalLightShadowBuffer);
			break;

		case LIGHT_POINT:
			// m_PointLightShadowBuffer.Cleanup();
			pTextureManager->DeleteTexture(m_pPointLightShadowBuffer);
			break;

		case LIGHT_SPOT:
			// m_SpotLightShadowBuffer.Cleanup();
			pTextureManager->DeleteTexture(m_pSpotLightShadowBuffer);
			break;

		default:
			break;
	}

	m_pRenderer = nullptr;
}

void ShadowMap::SetShadowWidth(const UINT WIDTH)
{
	m_ShadowMapWidth = WIDTH;

	for (int i = 0; i < 6; ++i)
	{
		m_pViewPorts[i] = { 0, 0, (float)m_ShadowMapWidth, (float)m_ShadowMapHeight, 0.0f, 1.0f };
		m_pScissorRects[i] = { 0, 0, (long)m_ShadowMapWidth, (long)m_ShadowMapHeight };
	}
}

void ShadowMap::SetShadowHeight(const UINT HEIGHT)
{
	m_ShadowMapHeight = HEIGHT;

	for (int i = 0; i < 6; ++i)
	{
		m_pViewPorts[i] = { 0, 0, (float)m_ShadowMapWidth, (float)m_ShadowMapHeight, 0.0f, 1.0f };
		m_pScissorRects[i] = { 0, 0, (long)m_ShadowMapWidth, (long)m_ShadowMapHeight };
	}
}

void ShadowMap::SetDescriptorHeap(Renderer* pRenderer)
{
	/*_ASSERT(pRenderer);

	ResourceManager* pManager = pRenderer->GetResourceManager();
	ID3D12Device5* pDevice = pManager->m_pDevice;
	const UINT DSV_DESCRIPTOR_SIZE = pManager->m_DSVDescriptorSize;
	const UINT CBV_SRV_DESCRIPTOR_SIZE = pManager->m_CBVSRVUAVDescriptorSize;
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(pManager->m_pDSVHeap->GetCPUDescriptorHandleForHeapStart(), pManager->m_DSVHeapSize, DSV_DESCRIPTOR_SIZE);
	CD3DX12_CPU_DESCRIPTOR_HANDLE cbvSrvHandle(pManager->m_pCBVSRVUAVHeap->GetCPUDescriptorHandleForHeapStart(), pManager->m_CBVSRVUAVHeapSize, CBV_SRV_DESCRIPTOR_SIZE);
	ID3D12Resource* pResource = nullptr;

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	ZeroMemory(&dsvDesc, sizeof(dsvDesc));
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ZeroMemory(&srvDesc, sizeof(srvDesc));
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	switch (m_LightType & m_TOTAL_LIGHT_TYPE)
	{
		case LIGHT_DIRECTIONAL:
		{
			pResource = m_DirectionalLightShadowBuffer.GetResource();

			dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
			dsvDesc.Texture2DArray.FirstArraySlice = 0;
			dsvDesc.Texture2DArray.MipSlice = 0;
			dsvDesc.Texture2DArray.ArraySize = 4;
			pDevice->CreateDepthStencilView(pResource, &dsvDesc, dsvHandle);
			m_DirectionalLightShadowBuffer.SetDSVHandle(dsvHandle);

			srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			srvDesc.Texture2DArray.MostDetailedMip = 0;
			srvDesc.Texture2DArray.MipLevels = 1;
			srvDesc.Texture2DArray.FirstArraySlice = 0;
			srvDesc.Texture2DArray.ArraySize = 4;
			pDevice->CreateShaderResourceView(pResource, &srvDesc, cbvSrvHandle);
			m_DirectionalLightShadowBuffer.SetSRVHandle(cbvSrvHandle);
		}
		break;

		case LIGHT_POINT:
		{
			pResource = m_PointLightShadowBuffer.GetResource();

			dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
			dsvDesc.Texture2DArray.FirstArraySlice = 0;
			dsvDesc.Texture2DArray.MipSlice = 0;
			dsvDesc.Texture2DArray.ArraySize = 6;
			pDevice->CreateDepthStencilView(pResource, &dsvDesc, dsvHandle);
			m_PointLightShadowBuffer.SetDSVHandle(dsvHandle);

			srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			srvDesc.TextureCube.MostDetailedMip = 0;
			srvDesc.TextureCube.MipLevels = 1;
			pDevice->CreateShaderResourceView(pResource, &srvDesc, cbvSrvHandle);
			m_PointLightShadowBuffer.SetSRVHandle(cbvSrvHandle);
		}
		break;

		case LIGHT_SPOT:
		{
			pResource = m_SpotLightShadowBuffer.GetResource();

			dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			dsvDesc.Texture2D.MipSlice = 0;
			pDevice->CreateDepthStencilView(pResource, &dsvDesc, dsvHandle);
			m_SpotLightShadowBuffer.SetDSVHandle(dsvHandle);

			srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MostDetailedMip = 0;
			srvDesc.Texture2D.MipLevels = 1;
			pDevice->CreateShaderResourceView(pResource, &srvDesc, cbvSrvHandle);
			m_SpotLightShadowBuffer.SetSRVHandle(cbvSrvHandle);
		}
		break;

		default:
			__debugbreak();
			break;
	}

	++(pManager->m_DSVHeapSize);
	++(pManager->m_CBVSRVUAVHeapSize);*/
}

void ShadowMap::SetViewportsAndScissorRect(ID3D12GraphicsCommandList* pCommandList)
{
	setShadowViewport(pCommandList);
	setShadowScissorRect(pCommandList);
}

void ShadowMap::setShadowViewport(ID3D12GraphicsCommandList* pCommandList)
{
	_ASSERT(pCommandList);

	switch (m_LightType & m_TOTAL_LIGHT_TYPE)
	{
		case LIGHT_DIRECTIONAL:
			pCommandList->RSSetViewports(4, m_pViewPorts);
			break;

		case LIGHT_POINT:
			pCommandList->RSSetViewports(6, m_pViewPorts);
			break;

		case LIGHT_SPOT:
			pCommandList->RSSetViewports(1, m_pViewPorts);
			break;

		default:
			break;
	}
}

void ShadowMap::setShadowScissorRect(ID3D12GraphicsCommandList* pCommandList)
{
	_ASSERT(pCommandList);

	switch (m_LightType & m_TOTAL_LIGHT_TYPE)
	{
		case LIGHT_DIRECTIONAL:
			pCommandList->RSSetScissorRects(4, m_pScissorRects);
			break;

		case LIGHT_POINT:
			pCommandList->RSSetScissorRects(6, m_pScissorRects);
			break;

		case LIGHT_SPOT:
			pCommandList->RSSetScissorRects(1, m_pScissorRects);
			break;

		default:
			break;
	}
}

void ShadowMap::calculateCascadeLightViewProjection(Vector3* pPosition, Matrix* pView, Matrix* pProjection, const Matrix& VIEW, const Matrix& PROJECTION, const Vector3& DIR, int cascadeIndex)
{
	_ASSERT(pPosition);
	_ASSERT(pView);
	_ASSERT(pProjection);

	const float FRUSTUM_Zs[5] = { 0.001f, 5.0f, 10.0f, 40.0f, 500.0f }; // 고정 값들로 우선 설정.
	Matrix inverseView = VIEW.Invert();
	Vector3 frustumCenter(0.0f);
	float boundingSphereRadius = 0.0f;

	float fov = 45.0f;
	float aspectRatio = 1270.0f / 720.0f;
	float nearZ = FRUSTUM_Zs[0];
	float farZ = FRUSTUM_Zs[4];
	float tanHalfVFov = tanf(DirectX::XMConvertToRadians(fov * 0.5f)); // 수직 시야각.
	float tanHalfHFov = tanHalfVFov * aspectRatio; // 수평 시야각.

	float xn = FRUSTUM_Zs[cascadeIndex] * tanHalfHFov;
	float xf = FRUSTUM_Zs[cascadeIndex + 1] * tanHalfHFov;
	float yn = FRUSTUM_Zs[cascadeIndex] * tanHalfVFov;
	float yf = FRUSTUM_Zs[cascadeIndex + 1] * tanHalfVFov;

	Vector3 frustumCorners[8] =
	{
		Vector3(xn, yn, FRUSTUM_Zs[cascadeIndex]),
		Vector3(-xn, yn, FRUSTUM_Zs[cascadeIndex]),
		Vector3(xn, -yn, FRUSTUM_Zs[cascadeIndex]),
		Vector3(-xn, -yn, FRUSTUM_Zs[cascadeIndex]),
		Vector3(xf, yf, FRUSTUM_Zs[cascadeIndex + 1]),
		Vector3(-xf, yf, FRUSTUM_Zs[cascadeIndex + 1]),
		Vector3(xf, -yf, FRUSTUM_Zs[cascadeIndex + 1]),
		Vector3(-xf, -yf, FRUSTUM_Zs[cascadeIndex + 1]),
	};

	for (int i = 0; i < 8; ++i)
	{
		frustumCorners[i] = Vector3::Transform(frustumCorners[i], inverseView);
		frustumCenter += frustumCorners[i];
	}
	frustumCenter /= 8.0f;

	for (int i = 0; i < 8; ++i)
	{
		float dist = (frustumCorners[i] - frustumCenter).Length();
		boundingSphereRadius = Max(boundingSphereRadius, dist);
	}
	boundingSphereRadius = ceil(boundingSphereRadius * 16.0f) / 16.0f;

	Vector3 frustumMax(boundingSphereRadius);
	Vector3 frustumMin = -frustumMax;
	Vector3 cascadeExtents = frustumMax - frustumMin;

	*pPosition = frustumCenter - DIR * fabs(frustumMin.z);
	*pView = DirectX::XMMatrixLookAtLH(*pPosition, frustumCenter, Vector3(0.0f, 1.0f, 0.0f));
	*pProjection = DirectX::XMMatrixOrthographicOffCenterLH(frustumMin.x, frustumMax.x, frustumMin.y, frustumMax.y, 0.001f, cascadeExtents.z);
}
