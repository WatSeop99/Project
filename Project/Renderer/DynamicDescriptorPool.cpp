#include "../pch.h"
#include "DynamicDescriptorPool.h"

void DynamicDescriptorPool::Initialize(ID3D12Device5* pDevice, UINT maxDescriptorCount)
{
	_ASSERT(pDevice);

	HRESULT hr = S_OK;

	m_pDevice = pDevice;
	m_MaxDescriptorCount = maxDescriptorCount;
	m_CBVSRVUAVDescriptorSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_DESCRIPTOR_HEAP_DESC commonHeapDesc = {};
	commonHeapDesc.NumDescriptors = (UINT)m_MaxDescriptorCount;
	commonHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	commonHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	hr = m_pDevice->CreateDescriptorHeap(&commonHeapDesc, IID_PPV_ARGS(&m_pDescriptorHeap));
	BREAK_IF_FAILED(hr);
	m_pDescriptorHeap->SetName(L"DynamicDescriptorHeap");

	m_CPUDescriptorHandle = m_pDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	m_GPUDescriptorHandle = m_pDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
}

HRESULT DynamicDescriptorPool::AllocDescriptorTable(D3D12_CPU_DESCRIPTOR_HANDLE* pOutCPUDescriptor, D3D12_GPU_DESCRIPTOR_HANDLE* pOutGPUDescriptorHandle, UINT descriptorCount)
{
	if (m_AllocatedDescriptorCount + (UINT64)descriptorCount > m_MaxDescriptorCount)
	{
		return E_FAIL;
	}

//#ifdef _DEBUG
//	char debugString[256];
//	sprintf_s(debugString, 256, "Before: %llu  DescriptorCount: %u  After: %llu\n", m_AllocatedDescriptorCount, descriptorCount, m_AllocatedDescriptorCount + descriptorCount);
//	OutputDebugStringA(debugString);
//#endif

	*pOutCPUDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_CPUDescriptorHandle, (int)m_AllocatedDescriptorCount, m_CBVSRVUAVDescriptorSize);
	*pOutGPUDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_GPUDescriptorHandle, (int)m_AllocatedDescriptorCount, m_CBVSRVUAVDescriptorSize);
	m_AllocatedDescriptorCount += descriptorCount;

	return S_OK;
}

void DynamicDescriptorPool::Reset()
{
	m_AllocatedDescriptorCount = 0;
}

void DynamicDescriptorPool::Cleanup()
{
	m_AllocatedDescriptorCount = 0;
	m_MaxDescriptorCount = 0;
	m_CBVSRVUAVDescriptorSize = 0;
	m_CPUDescriptorHandle = { 0xffffffffffffffff };
	m_GPUDescriptorHandle = { 0xffffffffffffffff };
	SAFE_RELEASE(m_pDescriptorHeap);

	m_pDevice = nullptr;
}
