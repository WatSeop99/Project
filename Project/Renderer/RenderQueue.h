#pragma once

#include "ResourceManager.h"

class ResourceManager;

struct RenderItem
{
	eRenderObjectType ModelType;
	eRenderPSOType PSOType;
	void* pObjectHandle;
	void* pLight; // for shadow pass.
	void* pFilter;
};

class RenderQueue
{
public:
	RenderQueue() = default;
	~RenderQueue() { Cleanup(); }

	void Initialize(UINT maxItemCount);

	bool Add(const RenderItem* pItem);

	UINT Process(UINT threadIndex, ID3D12CommandQueue* pCommandQueue, CommandListPool* pCommandListPool, ResourceManager* pManager, DynamicDescriptorPool* pDescriptorPool, ConstantBufferManager* pConstantBufferManager, int processCountPerCommandList);
	UINT ProcessLight(UINT threadIndex, ID3D12CommandQueue* pCommandQueue, CommandListPool* pCommandListPool, ResourceManager* pManager, DynamicDescriptorPool* pDescriptorPool, ConstantBufferManager* pConstantBufferManager, int processCountPerCommandList);
	UINT ProcessPostProcessing(UINT threadIndex, ID3D12CommandQueue* pCommandQueue, CommandListPool* pCommandListPool, ResourceManager* pManager, DynamicDescriptorPool* pDescriptorPool, ConstantBufferManager* pConstantBufferManager, int processCountPerCommandList);

	void Reset();

	void Cleanup();

protected:
	const RenderItem* dispatch();
	
private:
	BYTE* m_pBuffer = nullptr;
	UINT m_MaxBufferSize = 0;
	UINT m_AllocatedSize = 0;
	UINT m_ReadBufferPos = 0;
	UINT m_RenderObjectCount = 0;
};
