#pragma once

#include "../Util/IndexCreator.h"

class DescriptorAllocator
{
public:
	DescriptorAllocator() = default;
	~DescriptorAllocator() { Cleanup(); };

	void Initialize(ID3D12Device5* pDevice, UINT maxCount, D3D12_DESCRIPTOR_HEAP_FLAGS flag, D3D12_DESCRIPTOR_HEAP_TYPE heapType);

	UINT AllocDescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE* pOutCPUHandle);
	void FreeDescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle);

	bool Check(D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle);

	void Cleanup();

	inline ID3D12DescriptorHeap* GetDescriptorHeap() { return m_pHeap; }
	inline UINT GetDescriptorSize() { return m_DescriptorSize; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle);

private:
	ID3D12Device5* m_pDevice = nullptr;
	ID3D12DescriptorHeap* m_pHeap = nullptr;
	IndexCreator m_IndexCreator;
	UINT m_DescriptorSize = 0;
};

