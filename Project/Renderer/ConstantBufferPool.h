#pragma once

struct CBInfo
{
	D3D12_CPU_DESCRIPTOR_HANDLE CBVHandle;
	D3D12_GPU_VIRTUAL_ADDRESS GPUMemAddr;
	BYTE* pSystemMemAddr;
};
class ConstantBufferPool
{
public:
	ConstantBufferPool() = default;
	~ConstantBufferPool() { Cleanup(); }

	void Initialize(ID3D12Device* pDevice, eConstantBufferType type, UINT sizePerCBV, UINT maxCBVNum);

	CBInfo* AllocCB();

	inline void Reset() { m_AllocatedCBVNum = 0; }

	void Cleanup();

private:
	CBInfo* m_pCBContainerList = nullptr;
	ID3D12DescriptorHeap* m_pCBVHeap = nullptr;
	ID3D12Resource* m_pResource = nullptr;
	BYTE* m_pSystemMemAddr = nullptr;

	eConstantBufferType m_ConstantBufferType = ConstantBufferType_ConstantTypeCount;
	UINT m_SizePerCBV = 0;
	UINT m_MaxCBVNum = 0;
	UINT m_AllocatedCBVNum = 0;
};
