#include "../pch.h"
#include "../Graphics/ImageFilter.h"
#include "../Graphics/Light.h"
#include "../Model/SkinnedMeshModel.h"
#include "RenderQueue.h"

void RenderQueue::Initialize(UINT maxItemCount)
{
	_ASSERT(maxItemCount > 0);

	m_MaxBufferSize = sizeof(RenderItem) * maxItemCount;
	m_pBuffer = (BYTE*)malloc(m_MaxBufferSize);
#ifdef _DEBUG
	if (!m_pBuffer)
	{
		__debugbreak();
	}
#endif
	ZeroMemory(m_pBuffer, m_MaxBufferSize);
}

bool RenderQueue::Add(const RenderItem* pItem)
{
	_ASSERT(pItem);

	bool bRet = false;
	if (m_AllocatedSize + sizeof(RenderItem) > m_MaxBufferSize)
	{
		goto LB_RETURN;
	}

	// allocated and return.
	{
		BYTE* pDest = m_pBuffer + m_AllocatedSize;
		memcpy(pDest, pItem, sizeof(RenderItem));
		m_AllocatedSize += sizeof(RenderItem);
		++m_RenderObjectCount;
		bRet = true;
	}
	
LB_RETURN:
	return bRet;
}

UINT RenderQueue::Process(UINT threadIndex, ID3D12CommandQueue* pCommandQueue, CommandListPool* pCommandListPool, ResourceManager* pManager, DynamicDescriptorPool* pDescriptorPool, ConstantBufferManager* pConstantBufferManager, int processCountPerCommandList)
{
	_ASSERT(threadIndex >= 0 && threadIndex < MAX_RENDER_THREAD_COUNT);
	_ASSERT(pCommandQueue);
	_ASSERT(pCommandListPool);
	_ASSERT(pManager);
	_ASSERT(pDescriptorPool);
	_ASSERT(pConstantBufferManager);

	ID3D12GraphicsCommandList* ppCommandLists[64] = { };
	int commandListCount = 0;

	ID3D12GraphicsCommandList* pCommandList = nullptr;
	int processedCount = 0;
	int processedPerCommandList = 0;
	const RenderItem* pRenderItem = nullptr;

	while (pRenderItem = dispatch())
	{
		pCommandList = pCommandListPool->GetCurrentCommandList();

		ID3D12DescriptorHeap* ppDescriptorHeaps[2] =
		{
			pDescriptorPool->GetDescriptorHeap(),
			pManager->m_pSamplerHeap,
		};
		pCommandList->SetDescriptorHeaps(2, ppDescriptorHeaps);

		pManager->SetCommonState(threadIndex, pCommandList, pDescriptorPool, pConstantBufferManager, pRenderItem->PSOType);
		switch (pRenderItem->ModelType)
		{
			case RenderObjectType_DefaultType:
			case RenderObjectType_SkyboxType:
			case RenderObjectType_MirrorType:
			{
				Model* pModel = (Model*)pRenderItem->pObjectHandle;
				pModel->Render(threadIndex, pCommandList, pDescriptorPool, pConstantBufferManager, pManager, pRenderItem->PSOType);
			}
				break;

			case RenderObjectType_SkinnedType:
			{
				SkinnedMeshModel* pCharacter = (SkinnedMeshModel*)pRenderItem->pObjectHandle;
				pCharacter->Render(threadIndex, pCommandList, pDescriptorPool, pConstantBufferManager, pManager, pRenderItem->PSOType);
			}
				break;

			default:
				__debugbreak();
				break;
		}

		++processedCount;
		++processedPerCommandList;

		if (processedPerCommandList > processCountPerCommandList)
		{
			pCommandListPool->Close();
			ppCommandLists[commandListCount] = pCommandList;
			++commandListCount;
			pCommandList = nullptr;
			processedPerCommandList = 0;
		}
	}

	if (processedPerCommandList)
	{
		pCommandListPool->Close();
		ppCommandLists[commandListCount] = pCommandList;
		++commandListCount;
		pCommandList = nullptr;
		processedPerCommandList = 0;
	}
	if (commandListCount)
	{
		pCommandQueue->ExecuteCommandLists(commandListCount, (ID3D12CommandList**)ppCommandLists);
	}
	
	m_RenderObjectCount = 0;
	return commandListCount;
}

UINT RenderQueue::ProcessLight(UINT threadIndex, ID3D12CommandQueue* pCommandQueue, CommandListPool* pCommandListPool, ResourceManager* pManager, DynamicDescriptorPool* pDescriptorPool, ConstantBufferManager* pConstantBufferManager, int processCountPerCommandList)
{
	_ASSERT(threadIndex >= 0 && threadIndex < MAX_RENDER_THREAD_COUNT);
	_ASSERT(pCommandQueue);
	_ASSERT(pCommandListPool);
	_ASSERT(pManager);
	_ASSERT(pDescriptorPool);
	_ASSERT(pConstantBufferManager);

	ID3D12GraphicsCommandList* ppCommandLists[64] = { };
	int commandListCount = 0;

	ID3D12GraphicsCommandList* pCommandList = nullptr;
	int processedCount = 0;
	int processedPerCommandList = 0;
	const RenderItem* pRenderItem = nullptr;
	ConstantBufferPool* pLightConstantBufferPool = pConstantBufferManager->GetConstantBufferPool(ConstantBufferType_ShadowConstant);

	while (pRenderItem = dispatch())
	{
		pCommandList = pCommandListPool->GetCurrentCommandList();

		Light* pCurLight = (Light*)pRenderItem->pLight;
		TextureHandle* pShadowBuffer = nullptr;
		CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle;
		ID3D12Resource* pDepthStencilResource = nullptr;

		ID3D12DescriptorHeap* ppDescriptorHeaps[2] =
		{
			pDescriptorPool->GetDescriptorHeap(),
			pManager->m_pSamplerHeap,
		};
		pCommandList->SetDescriptorHeaps(2, ppDescriptorHeaps);

		CBInfo* pLightCB = pLightConstantBufferPool->AllocCB();
		ShadowConstant* pShadowConstantData = pCurLight->LightShadowMap.GetShadowConstantBufferDataForGSPtr();

		// Upload constant buffer(mesh, material).
		BYTE* pLightCBConstMem = pLightCB->pSystemMemAddr;
		memcpy(pLightCBConstMem, &pShadowConstantData, sizeof(ShadowConstant));

		pManager->SetCommonState(threadIndex, pCommandList, pDescriptorPool, pConstantBufferManager, pRenderItem->PSOType);
		switch (pCurLight->Property.LightType & (LIGHT_DIRECTIONAL | LIGHT_POINT | LIGHT_SPOT))
		{
			case LIGHT_DIRECTIONAL:
				pShadowBuffer = pCurLight->LightShadowMap.GetDirectionalLightShadowBufferPtr();
				pCommandList->SetGraphicsRootConstantBufferView(1, pLightCB->GPUMemAddr);
				break;

			case LIGHT_POINT:
				pShadowBuffer = pCurLight->LightShadowMap.GetPointLightShadowBufferPtr();
				pCommandList->SetGraphicsRootConstantBufferView(1, pLightCB->GPUMemAddr);
				break;

			case LIGHT_SPOT:
				pShadowBuffer = pCurLight->LightShadowMap.GetSpotLightShadowBufferPtr();
				pCommandList->SetGraphicsRootConstantBufferView(1, pLightCB->GPUMemAddr);
				break;

			default:
				__debugbreak();
				break;
		}
		dsvHandle = pShadowBuffer->DSVHandle;
		pDepthStencilResource = pShadowBuffer->pTextureResource;

		pCurLight->LightShadowMap.SetViewportsAndScissorRect(pCommandList);
		pCommandList->OMSetRenderTargets(0, nullptr, FALSE, &dsvHandle);

		switch (pRenderItem->ModelType)
		{
			case RenderObjectType_DefaultType:
			case RenderObjectType_SkyboxType:
			case RenderObjectType_MirrorType:
			{
				Model* pModel = (Model*)pRenderItem->pObjectHandle;
				pModel->Render(threadIndex, pCommandList, pDescriptorPool, pConstantBufferManager, pManager, pRenderItem->PSOType);
			}
			break;

			case RenderObjectType_SkinnedType:
			{
				SkinnedMeshModel* pCharacter = (SkinnedMeshModel*)pRenderItem->pObjectHandle;
				pCharacter->Render(threadIndex, pCommandList, pDescriptorPool, pConstantBufferManager, pManager, pRenderItem->PSOType);
			}
			break;

			default:
				__debugbreak();
				break;
		}

		++processedCount;
		++processedPerCommandList;

		if (processedPerCommandList > processCountPerCommandList)
		{
			pCommandListPool->Close();
			ppCommandLists[commandListCount] = pCommandList;
			++commandListCount;
			pCommandList = nullptr;
			processedPerCommandList = 0;
		}
	}

	if (processedPerCommandList)
	{
		pCommandListPool->Close();
		ppCommandLists[commandListCount] = pCommandList;
		++commandListCount;
		pCommandList = nullptr;
		processedPerCommandList = 0;
	}
	if (commandListCount)
	{
		pCommandQueue->ExecuteCommandLists(commandListCount, (ID3D12CommandList**)ppCommandLists);
	}

	m_RenderObjectCount = 0;
	return commandListCount;
}

UINT RenderQueue::ProcessPostProcessing(UINT threadIndex, ID3D12CommandQueue* pCommandQueue, CommandListPool* pCommandListPool, ResourceManager* pManager, DynamicDescriptorPool* pDescriptorPool, ConstantBufferManager* pConstantBufferManager, int processCountPerCommandList)
{
	_ASSERT(threadIndex >= 0 && threadIndex < MAX_RENDER_THREAD_COUNT);
	_ASSERT(pCommandQueue);
	_ASSERT(pCommandListPool);
	_ASSERT(pManager);
	_ASSERT(pDescriptorPool);
	_ASSERT(pConstantBufferManager);

	ID3D12GraphicsCommandList* ppCommandLists[64] = { };
	int commandListCount = 0;

	ID3D12GraphicsCommandList* pCommandList = nullptr;
	int processedCount = 0;
	int processedPerCommandList = 0;
	const RenderItem* pRenderItem = nullptr;

	while (pRenderItem = dispatch())
	{
		pCommandList = pCommandListPool->GetCurrentCommandList();

		ImageFilter* pImageFilter = (ImageFilter*)pRenderItem->pFilter;
		Mesh* pScreenMesh = (Mesh*)pRenderItem->pObjectHandle;

		ID3D12DescriptorHeap* ppDescriptorHeaps[2] =
		{
			pDescriptorPool->GetDescriptorHeap(),
			pManager->m_pSamplerHeap,
		};
		pCommandList->SetDescriptorHeaps(2, ppDescriptorHeaps);

		pManager->SetCommonState(threadIndex, pCommandList, pDescriptorPool, pConstantBufferManager, pRenderItem->PSOType);
		
		pImageFilter->BeforeRender(threadIndex, pCommandList, pDescriptorPool, pConstantBufferManager, pManager, pRenderItem->PSOType);
		pCommandList->IASetVertexBuffers(0, 1, &pScreenMesh->Vertex.VertexBufferView);
		pCommandList->IASetIndexBuffer(&pScreenMesh->Index.IndexBufferView);
		pCommandList->DrawIndexedInstanced(pScreenMesh->Index.Count, 1, 0, 0, 0);
		pImageFilter->AfterRender(pCommandList, pRenderItem->PSOType);

		++processedCount;
		++processedPerCommandList;

		if (processedPerCommandList > processCountPerCommandList)
		{
			pCommandListPool->Close();
			ppCommandLists[commandListCount] = pCommandList;
			++commandListCount;
			pCommandList = nullptr;
			processedPerCommandList = 0;
		}
	}

	if (processedPerCommandList)
	{
		pCommandListPool->Close();
		ppCommandLists[commandListCount] = pCommandList;
		++commandListCount;
		pCommandList = nullptr;
		processedPerCommandList = 0;
	}
	if (commandListCount)
	{
		pCommandQueue->ExecuteCommandLists(commandListCount, (ID3D12CommandList**)ppCommandLists);
	}

	m_RenderObjectCount = 0;
	return commandListCount;
}

void RenderQueue::Reset()
{
	m_AllocatedSize = 0;
	m_ReadBufferPos = 0;
}

void RenderQueue::Cleanup()
{
	if (m_pBuffer)
	{
		free(m_pBuffer);
		m_pBuffer = nullptr;
	}
	m_MaxBufferSize = 0;
	m_AllocatedSize = 0;
	m_ReadBufferPos = 0;
	m_RenderObjectCount = 0;
}

const RenderItem* RenderQueue::dispatch()
{
	const RenderItem* pItem = nullptr;
	if (m_ReadBufferPos + sizeof(RenderItem) > m_AllocatedSize)
	{
		goto LB_RETURN;
	}

	pItem = (const RenderItem*)(m_pBuffer + m_ReadBufferPos);
	m_ReadBufferPos += sizeof(RenderItem);

LB_RETURN:
	return pItem;
}
