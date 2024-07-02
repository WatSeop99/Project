#include "../pch.h"
#include "ConstantDataType.h"
#include "ImageFilter.h"

void ImageFilter::Initialize(ResourceManager* pManager, const int WIDTH, const int HEIGHT)
{
	_ASSERT(pManager);

	HRESULT hr = S_OK;

	Clear();

	m_ConstantBuffer.Initialize(pManager, sizeof(ImageFilterConstant));

	ImageFilterConstant* pConstantData = (ImageFilterConstant*)m_ConstantBuffer.pData;
	pConstantData->DX = 1.0f / WIDTH;
	pConstantData->DY = 1.0f / HEIGHT;
}

void ImageFilter::UpdateConstantBuffers()
{
	m_ConstantBuffer.Upload();
}

void ImageFilter::BeforeRender(ResourceManager* pManager, ePipelineStateSetting psoSetting, UINT frameIndex)
{
	_ASSERT(pManager);
	_ASSERT(m_RTVHandles.size() > 0);
	_ASSERT(m_SRVHandles.size() > 0);

	HRESULT hr = S_OK;

	ID3D12Device5* pDevice = pManager->m_pDevice;
	ID3D12GraphicsCommandList* pCommandList = pManager->m_pSingleCommandList;
	DynamicDescriptorPool* pDynamicDescriptorPool = pManager->m_pDynamicDescriptorPool;
	const UINT CBV_SRV_UAV_DESCRIPTOR_SIZE = pManager->m_CBVSRVUAVDescriptorSize;

	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuDescriptorTable = {};
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuDescriptorTable = {};

	switch (psoSetting)
	{
		case Sampling:
		case BloomDown: case BloomUp:
		{
			hr = pDynamicDescriptorPool->AllocDescriptorTable(&cpuDescriptorTable, &gpuDescriptorTable, 2);
			BREAK_IF_FAILED(hr);

			const CD3DX12_RESOURCE_BARRIER BARRIER = CD3DX12_RESOURCE_BARRIER::Transition(m_RTVHandles[0].pResource, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
			CD3DX12_CPU_DESCRIPTOR_HANDLE dstHandle(cpuDescriptorTable, 0, CBV_SRV_UAV_DESCRIPTOR_SIZE);

			// t0
			pDevice->CopyDescriptorsSimple(1, dstHandle, m_SRVHandles[0].CPUHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);

			// b4
			pDevice->CopyDescriptorsSimple(1, dstHandle, m_ConstantBuffer.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			pCommandList->ResourceBarrier(1, &BARRIER);
			pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable);
			pCommandList->OMSetRenderTargets(1, &m_RTVHandles[0].CPUHandle, FALSE, nullptr);
		}
		break;

		case Combine:
		{
			hr = pDynamicDescriptorPool->AllocDescriptorTable(&cpuDescriptorTable, &gpuDescriptorTable, 4);
			BREAK_IF_FAILED(hr);

			CD3DX12_CPU_DESCRIPTOR_HANDLE dstHandle(cpuDescriptorTable, 0, CBV_SRV_UAV_DESCRIPTOR_SIZE);

			// t0
			pDevice->CopyDescriptorsSimple(1, dstHandle, m_SRVHandles[0].CPUHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);

			// t1
			pDevice->CopyDescriptorsSimple(1, dstHandle, m_SRVHandles[1].CPUHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);

			// t2
			pDevice->CopyDescriptorsSimple(1, dstHandle, m_SRVHandles[2].CPUHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);

			// b4
			pDevice->CopyDescriptorsSimple(1, dstHandle, m_ConstantBuffer.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable);
			pCommandList->OMSetRenderTargets(1, &m_RTVHandles[frameIndex].CPUHandle, FALSE, nullptr);
		}
		break;

		default:
			__debugbreak();
			break;
	}
}

void ImageFilter::BeforeRender(ResourceManager* pManager, ID3D12GraphicsCommandList* pCommandList, ePipelineStateSetting psoSetting, UINT frameIndex)
{
	_ASSERT(pManager);
	_ASSERT(pCommandList);
	_ASSERT(m_RTVHandles.size() > 0);
	_ASSERT(m_SRVHandles.size() > 0);

	HRESULT hr = S_OK;

	ID3D12Device5* pDevice = pManager->m_pDevice;
	DynamicDescriptorPool* pDynamicDescriptorPool = pManager->m_pDynamicDescriptorPool;
	const UINT CBV_SRV_UAV_DESCRIPTOR_SIZE = pManager->m_CBVSRVUAVDescriptorSize;

	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuDescriptorTable = {};
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuDescriptorTable = {};

	switch (psoSetting)
	{
		case Sampling:
		case BloomDown: case BloomUp:
		{
			hr = pDynamicDescriptorPool->AllocDescriptorTable(&cpuDescriptorTable, &gpuDescriptorTable, 2);
			BREAK_IF_FAILED(hr);

			const CD3DX12_RESOURCE_BARRIER BARRIER = CD3DX12_RESOURCE_BARRIER::Transition(m_RTVHandles[0].pResource, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
			CD3DX12_CPU_DESCRIPTOR_HANDLE dstHandle(cpuDescriptorTable, 0, CBV_SRV_UAV_DESCRIPTOR_SIZE);

			// t0
			pDevice->CopyDescriptorsSimple(1, dstHandle, m_SRVHandles[0].CPUHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);

			// b4
			pDevice->CopyDescriptorsSimple(1, dstHandle, m_ConstantBuffer.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			pCommandList->ResourceBarrier(1, &BARRIER);
			pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable);
			pCommandList->OMSetRenderTargets(1, &m_RTVHandles[0].CPUHandle, FALSE, nullptr);
		}
		break;

		case Combine:
		{
			hr = pDynamicDescriptorPool->AllocDescriptorTable(&cpuDescriptorTable, &gpuDescriptorTable, 4);
			BREAK_IF_FAILED(hr);

			CD3DX12_CPU_DESCRIPTOR_HANDLE dstHandle(cpuDescriptorTable, 0, CBV_SRV_UAV_DESCRIPTOR_SIZE);

			// t0
			pDevice->CopyDescriptorsSimple(1, dstHandle, m_SRVHandles[0].CPUHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);

			// t1
			pDevice->CopyDescriptorsSimple(1, dstHandle, m_SRVHandles[1].CPUHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);

			// t2
			pDevice->CopyDescriptorsSimple(1, dstHandle, m_SRVHandles[2].CPUHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);

			// b4
			pDevice->CopyDescriptorsSimple(1, dstHandle, m_ConstantBuffer.GetCBVHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable);
			pCommandList->OMSetRenderTargets(1, &m_RTVHandles[frameIndex].CPUHandle, FALSE, nullptr);
		}
		break;

		default:
			__debugbreak();
			break;
	}
}

void ImageFilter::AfterRender(ResourceManager* pManager, ePipelineStateSetting psoSetting, UINT frameIndex)
{
	_ASSERT(pManager);
	_ASSERT(m_RTVHandles.size() > 0);
	_ASSERT(m_SRVHandles.size() > 0);

	switch (psoSetting)
	{
		case Sampling:
		case BloomDown: case BloomUp:
		{
			const CD3DX12_RESOURCE_BARRIER BARRIER = CD3DX12_RESOURCE_BARRIER::Transition(m_RTVHandles[0].pResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON);
			pManager->m_pSingleCommandList->ResourceBarrier(1, &BARRIER);
		}
		break;

		case Combine:
			break;

		default:
			__debugbreak();
			break;
	}
}

void ImageFilter::AfterRender(ResourceManager* pManager, ID3D12GraphicsCommandList* pCommandList, ePipelineStateSetting psoSetting, UINT frameIndex)
{
	_ASSERT(pManager);
	_ASSERT(pCommandList);
	_ASSERT(m_RTVHandles.size() > 0);
	_ASSERT(m_SRVHandles.size() > 0);

	switch (psoSetting)
	{
		case Sampling:
		case BloomDown: case BloomUp:
		{
			const CD3DX12_RESOURCE_BARRIER BARRIER = CD3DX12_RESOURCE_BARRIER::Transition(m_RTVHandles[0].pResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON);
			pCommandList->ResourceBarrier(1, &BARRIER);
		}
		break;

		case Combine:
			break;

		default:
			__debugbreak();
			break;
	}
}

void ImageFilter::Clear()
{
	m_SRVHandles.clear();
	m_RTVHandles.clear();
	m_ConstantBuffer.Clear();
}

void ImageFilter::SetSRVOffsets(ResourceManager* pManager, const std::vector<ImageResource>& SRVs)
{
	_ASSERT(pManager);

	CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(pManager->m_pCBVSRVUAVHeap->GetCPUDescriptorHandleForHeapStart());
	CD3DX12_CPU_DESCRIPTOR_HANDLE startSrvHandle(srvHandle);
	const UINT CBV_SRV_DESCRIPTOR_SIZE = pManager->m_CBVSRVUAVDescriptorSize;

	m_SRVHandles.clear();
	m_SRVHandles.resize(SRVs.size());
	for (UINT64 i = 0, size = SRVs.size(); i < size; ++i)
	{
		m_SRVHandles[i] = { SRVs[i].pResource, srvHandle.Offset(SRVs[i].SRVOffset, CBV_SRV_DESCRIPTOR_SIZE) };
		srvHandle = startSrvHandle;
	}
}

void ImageFilter::SetRTVOffsets(ResourceManager* pManager, const std::vector<ImageResource>& RTVs)
{
	_ASSERT(pManager);

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(pManager->m_pRTVHeap->GetCPUDescriptorHandleForHeapStart());
	CD3DX12_CPU_DESCRIPTOR_HANDLE startRtvHandle(rtvHandle);
	const UINT RTV_DESCRIPTOR_SIZE = pManager->m_RTVDescriptorSize;

	m_RTVHandles.clear();
	m_RTVHandles.resize(RTVs.size());
	for (UINT64 i = 0, size = RTVs.size(); i < size; ++i)
	{
		m_RTVHandles[i] = { RTVs[i].pResource, rtvHandle.Offset(RTVs[i].RTVOffset, RTV_DESCRIPTOR_SIZE) };
		rtvHandle = startRtvHandle;
	}
}

void ImageFilter::SetDescriptorHeap(ResourceManager* pManager)
{
	_ASSERT(pManager);

	ID3D12Device5* pDevice = pManager->m_pDevice;
	ID3D12DescriptorHeap* pCBVSRVHeap = pManager->m_pCBVSRVUAVHeap;
	const UINT CBV_SRV_UAV_DESCRIPTOR_SIZE = pManager->m_CBVSRVUAVDescriptorSize;
	CD3DX12_CPU_DESCRIPTOR_HANDLE cbvSrvLastHandle;

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
	cbvDesc.BufferLocation = m_ConstantBuffer.GetGPUMemAddr();
	cbvDesc.SizeInBytes = (UINT)m_ConstantBuffer.GetBufferSize();
	
	if (m_ConstantBuffer.GetCBVHandle().ptr == 0xffffffffffffffff)
	{
		cbvSrvLastHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(pCBVSRVHeap->GetCPUDescriptorHandleForHeapStart(), pManager->m_CBVSRVUAVHeapSize, CBV_SRV_UAV_DESCRIPTOR_SIZE);

		pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvLastHandle);
		m_ConstantBuffer.SetCBVHandle(cbvSrvLastHandle);
		++(pManager->m_CBVSRVUAVHeapSize);
	}
	else
	{
		cbvSrvLastHandle = m_ConstantBuffer.GetCBVHandle();
		pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvLastHandle);
	}
}
