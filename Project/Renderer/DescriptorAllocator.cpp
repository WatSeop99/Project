#include "../pch.h"
#include "DescriptorAllocator.h"

void DescriptorAllocator::Initialize(ID3D12Device5* pDevice, UINT maxCount, D3D12_DESCRIPTOR_HEAP_FLAGS flag, D3D12_DESCRIPTOR_HEAP_TYPE heapType)
{
	_ASSERT(pDevice);
	_ASSERT(maxCount > 0);
	_ASSERT(heapType < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES);

	m_pDevice = pDevice;
	m_pDevice->AddRef();

	D3D12_DESCRIPTOR_HEAP_DESC commonHeapDesc = {};
	commonHeapDesc.NumDescriptors = maxCount;
	commonHeapDesc.Type = heapType;
	commonHeapDesc.Flags = flag;

	if (FAILED(m_pDevice->CreateDescriptorHeap(&commonHeapDesc, IID_PPV_ARGS(&m_pHeap))))
	{
		__debugbreak();
	}

	m_IndexCreator.Initialize(maxCount);
	m_DescriptorSize = m_pDevice->GetDescriptorHandleIncrementSize(heapType);
}

UINT DescriptorAllocator::AllocDescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE* pCPUHandle)
{
	_ASSERT(pCPUHandle);

	UINT index = m_IndexCreator.Alloc();
	if (index == -1)
	{
		__debugbreak();
	}

	CD3DX12_CPU_DESCRIPTOR_HANDLE DescriptorHandle(m_pHeap->GetCPUDescriptorHandleForHeapStart(), index, m_DescriptorSize);
	*pCPUHandle = DescriptorHandle;

	return index;
}

void DescriptorAllocator::FreeDescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle)
{
	D3D12_CPU_DESCRIPTOR_HANDLE base = m_pHeap->GetCPUDescriptorHandleForHeapStart();
#ifdef _DEBUG
	if (base.ptr > CPUHandle.ptr)
	{
		__debugbreak();
	}
#endif

	UINT index = (UINT)(CPUHandle.ptr - base.ptr) / m_DescriptorSize;
	m_IndexCreator.Free(index);
}

bool DescriptorAllocator::Check(D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle)
{
	bool bRet = true;

	D3D12_CPU_DESCRIPTOR_HANDLE base = m_pHeap->GetCPUDescriptorHandleForHeapStart();
	if (base.ptr > CPUHandle.ptr)
	{
		__debugbreak();
		bRet = false;
	}

	return bRet;
}

void DescriptorAllocator::Cleanup()
{
#ifdef _DEBUG
	m_IndexCreator.Check();
#endif
	m_DescriptorSize = 0;
	SAFE_RELEASE(m_pHeap);
	SAFE_RELEASE(m_pDevice);
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorAllocator::GetGPUHandle(D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle)
{
	D3D12_CPU_DESCRIPTOR_HANDLE base = m_pHeap->GetCPUDescriptorHandleForHeapStart();
#ifdef _DEBUG
	if (base.ptr > CPUHandle.ptr)
	{
		__debugbreak();
	}
#endif
	UINT index = (UINT)(CPUHandle.ptr - base.ptr) / m_DescriptorSize;
	CD3DX12_GPU_DESCRIPTOR_HANDLE GPUHandle(m_pHeap->GetGPUDescriptorHandleForHeapStart(), index, m_DescriptorSize);

	return GPUHandle;
}
