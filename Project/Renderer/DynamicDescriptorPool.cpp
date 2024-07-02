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
	commonHeapDesc.NumDescriptors = m_MaxDescriptorCount;
	commonHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	commonHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	hr = m_pDevice->CreateDescriptorHeap(&commonHeapDesc, IID_PPV_ARGS(&m_pDescriptorHeap));
	BREAK_IF_FAILED(hr);
	m_pDescriptorHeap->SetName(L"DynamicDescriptorHeap");

	m_CPUDescriptorHandle = m_pDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	m_GPUDescriptorHandle = m_pDescriptorHeap->GetGPUDescriptorHandleForHeapStart();

	InitializeCriticalSection(&m_AllocatedCountLock);
}

HRESULT DynamicDescriptorPool::AllocDescriptorTable(D3D12_CPU_DESCRIPTOR_HANDLE* pCPUDescriptor, D3D12_GPU_DESCRIPTOR_HANDLE* pGPUDescriptorHandle, UINT descriptorCount)
{
	EnterCriticalSection(&m_AllocatedCountLock);
	if (m_AllocatedDescriptorCount + descriptorCount > m_MaxDescriptorCount)
	{
		LeaveCriticalSection(&m_AllocatedCountLock);
		return E_FAIL;
	}

#ifdef _DEBUG
	std::wstring debugStr = std::wstring(L"Before:") + std::to_wstring(m_AllocatedDescriptorCount) + std::wstring(L" DescriptorCount:") + std::to_wstring(descriptorCount) + std::wstring(L" After:") + std::to_wstring(m_AllocatedDescriptorCount + descriptorCount) + L"\n";
	OutputDebugString(debugStr.c_str());
#endif

	*pCPUDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_CPUDescriptorHandle, m_AllocatedDescriptorCount, m_CBVSRVUAVDescriptorSize);
	*pGPUDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_GPUDescriptorHandle, m_AllocatedDescriptorCount, m_CBVSRVUAVDescriptorSize);
	m_AllocatedDescriptorCount += descriptorCount;
	LeaveCriticalSection(&m_AllocatedCountLock);

	return S_OK;
}

void DynamicDescriptorPool::Reset()
{
	EnterCriticalSection(&m_AllocatedCountLock);
	m_AllocatedDescriptorCount = 0;
	LeaveCriticalSection(&m_AllocatedCountLock);
}

void DynamicDescriptorPool::Clear()
{
	m_AllocatedDescriptorCount = 0;
	m_MaxDescriptorCount = 0;
	m_CBVSRVUAVDescriptorSize = 0;
	m_CPUDescriptorHandle = { 0xffffffffffffffff };
	m_GPUDescriptorHandle = { 0xffffffffffffffff };
	SAFE_RELEASE(m_pDescriptorHeap);

	DeleteCriticalSection(&m_AllocatedCountLock);

	m_pDevice = nullptr;
}
