#pragma once

class DynamicDescriptorPool
{
public:
	DynamicDescriptorPool() = default;
	~DynamicDescriptorPool() { Clear(); }

	void Initialize(ID3D12Device5* pDevice, UINT maxDescriptorCount);

	HRESULT AllocDescriptorTable(D3D12_CPU_DESCRIPTOR_HANDLE* pCPUDescriptor, D3D12_GPU_DESCRIPTOR_HANDLE* pGPUDescriptorHandle, UINT descriptorCount);

	void Reset();

	void Clear();

	inline ID3D12DescriptorHeap* GetDescriptorHeap() { return m_pDescriptorHeap; }

private:
	ID3D12Device* m_pDevice = nullptr;
	ID3D12DescriptorHeap* m_pDescriptorHeap = nullptr;
	UINT m_AllocatedDescriptorCount = 0;
	UINT m_MaxDescriptorCount = 0;
	UINT m_CBVSRVUAVDescriptorSize = 0;
	D3D12_CPU_DESCRIPTOR_HANDLE m_CPUDescriptorHandle = { 0xffffffffffffffff, };
	D3D12_GPU_DESCRIPTOR_HANDLE m_GPUDescriptorHandle = { 0xffffffffffffffff, };
};
