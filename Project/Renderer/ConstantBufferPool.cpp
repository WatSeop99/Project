#include "../pch.h"
#include "ConstantBufferPool.h"

void ConstantBufferPool::Initialize(ID3D12Device* pDevice, eConstantBufferType type, UINT sizePerCBV, UINT maxCBVNum)
{
	_ASSERT(pDevice);
	_ASSERT(maxCBVNum > 0);

	HRESULT hr = S_OK;
	UINT byteWidth = 0;

	m_ConstantBufferType = type;
	m_MaxCBVNum = maxCBVNum;
	m_SizePerCBV = sizePerCBV;
	byteWidth = sizePerCBV * m_MaxCBVNum;


	CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(byteWidth);
	CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);

	// Create the constant buffer.
	hr = pDevice->CreateCommittedResource(&heapProps,
										  D3D12_HEAP_FLAG_NONE,
										  &bufferDesc,
										  D3D12_RESOURCE_STATE_GENERIC_READ,
										  nullptr,
										  IID_PPV_ARGS(&m_pResource));
	BREAK_IF_FAILED(hr);


	// Create descriptor heap.
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = m_MaxCBVNum;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	
	hr = pDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_pCBVHeap));
	BREAK_IF_FAILED(hr);


	// Map cpu pointer to gpu cb pointer.
	CD3DX12_RANGE writeRange(0, 0);		// We do not intend to write from this resource on the CPU.
	m_pResource->Map(0, &writeRange, (void**)(&m_pSystemMemAddr));


	m_pCBContainerList = new CBInfo[m_MaxCBVNum];


	// Create CBVs.
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = m_pResource->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = m_SizePerCBV;

	BYTE* pSystemMemPtr = m_pSystemMemAddr;
	CD3DX12_CPU_DESCRIPTOR_HANDLE	heapHandle(m_pCBVHeap->GetCPUDescriptorHandleForHeapStart());

	UINT descriptorSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	for (UINT i = 0; i < m_MaxCBVNum; ++i)
	{
		pDevice->CreateConstantBufferView(&cbvDesc, heapHandle);

		m_pCBContainerList[i].CBVHandle = heapHandle;
		m_pCBContainerList[i].GPUMemAddr = cbvDesc.BufferLocation;
		m_pCBContainerList[i].pSystemMemAddr = pSystemMemPtr;

		heapHandle.Offset(1, descriptorSize);
		cbvDesc.BufferLocation += m_SizePerCBV;
		pSystemMemPtr += m_SizePerCBV;
	}
}

CBInfo* ConstantBufferPool::AllocCB()
{
	CBInfo* pCB = nullptr;

	if (m_AllocatedCBVNum >= m_MaxCBVNum)
	{
		goto LB_RETURN;
	}

	pCB = m_pCBContainerList + m_AllocatedCBVNum;
	++m_AllocatedCBVNum;

LB_RETURN:
	return pCB;
}

void ConstantBufferPool::Cleanup()
{
	m_SizePerCBV = 0;
	m_MaxCBVNum = 0;
	m_AllocatedCBVNum = 0;

	if (m_pCBContainerList)
	{
		delete[] m_pCBContainerList;
		m_pCBContainerList = nullptr;
	}
	SAFE_RELEASE(m_pCBVHeap);
	SAFE_RELEASE(m_pResource);
}
