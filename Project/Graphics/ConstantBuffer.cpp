#include "../pch.h"
#include "../Renderer/ResourceManager.h"
#include "ConstantBuffer.h"

void ConstantBuffer::Initialize(Renderer* pRenderer, UINT64 bufferSize)
{
	_ASSERT(pRenderer);

	Cleanup();

	HRESULT hr = S_OK;
	ResourceManager* pManager = pRenderer->GetResourceManager();
	ID3D12Device5* pDevice = pManager->m_pDevice;

	m_BufferSize = (bufferSize + 255) & ~(255);
	m_DataSize = bufferSize;

	pData = malloc(m_DataSize);
	ZeroMemory(pData, m_DataSize);

	D3D12_RESOURCE_DESC resourceDesc;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Alignment = 0;
	resourceDesc.Width = m_BufferSize;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	D3D12_HEAP_PROPERTIES heapProps;
	heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProps.CreationNodeMask = 1;
	heapProps.VisibleNodeMask = 1;

	hr = pDevice->CreateCommittedResource(&heapProps,
										  D3D12_HEAP_FLAG_NONE,
										  &resourceDesc,
										  D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
										  nullptr,
										  IID_PPV_ARGS(&m_pResource));
	BREAK_IF_FAILED(hr);
	m_pResource->SetName(L"ConstantResource");

	m_GPUMemAddr = m_pResource->GetGPUVirtualAddress();

	CD3DX12_RANGE writeRange(0, 0);
	m_pResource->Map(0, &writeRange, &m_pSystemMemAddr);
}

void ConstantBuffer::Upload()
{
	_ASSERT(m_pResource);
	_ASSERT(m_pSystemMemAddr);
	_ASSERT(pData);

	memcpy(m_pSystemMemAddr, pData, m_DataSize);
}

void ConstantBuffer::Cleanup()
{
	if (pData)
	{
		free(pData);
		pData = nullptr;
	}

	m_DataSize = 0;
	m_BufferSize = 0;

	m_GPUMemAddr = 0xffffffffffffffff;
	// m_CBVHandle = { 0xffffffffffffffff, };

	if (m_pResource)
	{
		CD3DX12_RANGE writeRange(0, 0);
		m_pResource->Unmap(0, &writeRange);
		m_pSystemMemAddr = nullptr;

		m_pResource->Release();
		m_pResource = nullptr;
	}
}
