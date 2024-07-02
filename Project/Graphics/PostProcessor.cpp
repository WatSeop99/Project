#include "../pch.h"
#include "ConstantDataType.h"
#include "../Model/GeometryGenerator.h"
#include "../Model/MeshInfo.h"
#include "PostProcessor.h"

void PostProcessor::Initizlie(ResourceManager* pManager, const PostProcessingBuffers& CONFIG, const int WIDTH, const int HEIGHT, const int BLOOMLEVELS)
{
	_ASSERT(pManager);

	HRESULT hr = S_OK;

	Clear();

	// 후처리용 리소스 설정.
	setRenderConfig(CONFIG);

	m_ScreenWidth = WIDTH;
	m_ScreenHeight = HEIGHT;

	m_Viewport.TopLeftX = 0;
	m_Viewport.TopLeftY = 0;
	m_Viewport.Width = (float)m_ScreenWidth;
	m_Viewport.Height = (float)m_ScreenHeight;
	m_Viewport.MinDepth = 0.0f;
	m_Viewport.MaxDepth = 1.0f;

	m_ScissorRect.left = 0;
	m_ScissorRect.top = 0;
	m_ScissorRect.right = m_ScreenWidth;
	m_ScissorRect.bottom = m_ScreenHeight;

	// 스크린 공간 설정.
	{
		MeshInfo meshInfo = INIT_MESH_INFO;
		MakeSquare(&meshInfo);

		m_pScreenMesh = (Mesh*)malloc(sizeof(Mesh));
		*m_pScreenMesh = INIT_MESH;

		hr = pManager->CreateVertexBuffer(sizeof(Vertex),
										  (UINT)meshInfo.Vertices.size(),
										  &(m_pScreenMesh->VertexBufferView),
										  &(m_pScreenMesh->pVertexBuffer),
										  (void*)meshInfo.Vertices.data());
		BREAK_IF_FAILED(hr);
		m_pScreenMesh->VertexCount = (UINT)meshInfo.Vertices.size();

		hr = pManager->CreateIndexBuffer(sizeof(UINT),
										 (UINT)meshInfo.Indices.size(),
										 &(m_pScreenMesh->IndexBufferView),
										 &(m_pScreenMesh->pIndexBuffer),
										 (void*)meshInfo.Indices.data());
		BREAK_IF_FAILED(hr);
		m_pScreenMesh->IndexCount = (UINT)meshInfo.Indices.size();
	}

	createPostBackBuffers(pManager);

	m_BasicSamplingFilter.Initialize(pManager, WIDTH, HEIGHT);

	m_BasicSamplingFilter.SetSRVOffsets(pManager, { { m_pFloatBuffer, 0xffffffff, m_FloatBufferSRVOffset } });
	m_BasicSamplingFilter.SetRTVOffsets(pManager, { { m_pResolvedBuffer, m_ResolvedRTVOffset, 0xffffffff } });

	// Bloom Down/Up 초기화.
	if (m_BloomResources.size() != BLOOMLEVELS)
	{
		m_BloomResources.resize(BLOOMLEVELS);
	}
	for (int i = 0; i < BLOOMLEVELS; ++i)
	{
		int div = (int)pow(2, i);
		createImageResources(pManager, WIDTH / div, HEIGHT / div, &m_BloomResources[i]);
	}

	ImageFilter::ImageResource resource;
	m_BloomDownFilters.resize(BLOOMLEVELS - 1);
	m_BloomUpFilters.resize(BLOOMLEVELS - 1);
	for (int i = 0; i < BLOOMLEVELS - 1; ++i)
	{
		int div = (int)pow(2, i + 1);
		m_BloomDownFilters[i].Initialize(pManager, WIDTH / div, HEIGHT / div);
		if (i == 0)
		{
			resource.pResource = m_pResolvedBuffer;
			resource.RTVOffset = 0xffffffff;
			resource.SRVOffset = m_ResolvedSRVOffset;
		}
		else
		{
			resource.pResource = m_BloomResources[i].pResource;
			resource.RTVOffset = 0xffffffff;
			resource.SRVOffset = m_BloomResources[i].SRVOffset;
		}
		m_BloomDownFilters[i].SetSRVOffsets(pManager, { resource });
		
		resource.pResource = m_BloomResources[i + 1].pResource;
		resource.RTVOffset = m_BloomResources[i + 1].RTVOffset;
		resource.SRVOffset = 0xffffffff;
		m_BloomDownFilters[i].SetRTVOffsets(pManager, { resource });

		m_BloomDownFilters[i].UpdateConstantBuffers();
	}
	for (int i = 0; i < BLOOMLEVELS - 1; ++i)
	{
		int level = BLOOMLEVELS - 2 - i;
		int div = (int)pow(2, level);
		m_BloomUpFilters[i].Initialize(pManager, WIDTH / div, HEIGHT / div);

		resource.pResource = m_BloomResources[level + 1].pResource;
		resource.RTVOffset = 0xffffffff;
		resource.SRVOffset = m_BloomResources[level + 1].SRVOffset;
		m_BloomUpFilters[i].SetSRVOffsets(pManager, { resource });

		resource.pResource = m_BloomResources[level].pResource;
		resource.RTVOffset = m_BloomResources[level].RTVOffset;
		resource.SRVOffset = 0xffffffff;
		m_BloomUpFilters[i].SetRTVOffsets(pManager, { resource });

		m_BloomUpFilters[i].UpdateConstantBuffers();
	}

	// combine + tone mapping.
	m_CombineFilter.Initialize(pManager, WIDTH, HEIGHT);

	m_CombineFilter.SetSRVOffsets(pManager, { { m_pResolvedBuffer, 0xffffffff, m_ResolvedSRVOffset }, { m_BloomResources[0].pResource, 0xffffffff, m_BloomResources[0].SRVOffset }, { m_pPrevBuffer, 0xffffffff, m_PrevBufferSRVOffset } });
	m_CombineFilter.SetRTVOffsets(pManager, { { m_ppBackBuffers[0], m_BackBufferRTV1Offset, 0xffffffff }, { m_ppBackBuffers[1], m_BackBufferRTV2Offset, 0xffffffff } });

	ImageFilterConstant* pCombineConst = (ImageFilterConstant*)m_CombineFilter.GetConstantPtr()->pData;
	pCombineConst->Option1 = 1.0f;  // exposure.
	pCombineConst->Option2 = 2.1f;  // gamma.
	m_CombineFilter.UpdateConstantBuffers();
}

void PostProcessor::Update(ResourceManager* pManager)
{
	_ASSERT(pManager);
	// 설정을 바꾸지 않으므로 update 없음.
}

void PostProcessor::Render(ResourceManager* pManager, UINT frameIndex)
{
	_ASSERT(pManager);

	ID3D12Device5* pDevice = pManager->m_pDevice;
	ID3D12GraphicsCommandList* pCommandList = pManager->m_pSingleCommandList;

	pCommandList->RSSetViewports(1, &m_Viewport);
	pCommandList->RSSetScissorRects(1, &m_ScissorRect);

	// 스크린 렌더링을 위한 정점 버퍼와 인텍스 버퍼를 미리 설정.
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	pCommandList->IASetVertexBuffers(0, 1, &(m_pScreenMesh->VertexBufferView));
	pCommandList->IASetIndexBuffer(&(m_pScreenMesh->IndexBufferView));

	// basic sampling.
	pManager->SetCommonState(Sampling);
	renderImageFilter(pManager, m_BasicSamplingFilter, Sampling, frameIndex);

	// post processing.
	renderPostProcessing(pManager, frameIndex);
}

void PostProcessor::Render(ResourceManager* pManager, ID3D12GraphicsCommandList* pCommandList, UINT frameIndex)
{
	_ASSERT(pManager);
	_ASSERT(pCommandList);

	ID3D12Device5* pDevice = pManager->m_pDevice;

	pCommandList->RSSetViewports(1, &m_Viewport);
	pCommandList->RSSetScissorRects(1, &m_ScissorRect);

	// 스크린 렌더링을 위한 정점 버퍼와 인텍스 버퍼를 미리 설정.
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	pCommandList->IASetVertexBuffers(0, 1, &(m_pScreenMesh->VertexBufferView));
	pCommandList->IASetIndexBuffer(&(m_pScreenMesh->IndexBufferView));

	// basic sampling.
	pManager->SetCommonState(pCommandList, Sampling);
	renderImageFilter(pManager, pCommandList, m_BasicSamplingFilter, Sampling, frameIndex);

	// post processing.
	renderPostProcessing(pManager, pCommandList, frameIndex);
}

void PostProcessor::Clear()
{
	m_ppBackBuffers[0] = nullptr;
	m_ppBackBuffers[1] = nullptr;
	m_pFloatBuffer = nullptr;
	m_pPrevBuffer = nullptr;
	m_pGlobalConstant = nullptr;
	m_BackBufferRTV1Offset = 0xffffffff;
	m_BackBufferRTV2Offset = 0xffffffff;
	m_FloatBufferSRVOffset = 0xffffffff;
	m_PrevBufferSRVOffset = 0xffffffff;

	for (UINT64 i = 0, size = m_BloomResources.size(); i < size; ++i)
	{
		SAFE_RELEASE(m_BloomResources[i].pResource);
	}
	for (UINT64 i = 0, size = m_BloomDownFilters.size(); i < size; ++i)
	{
		m_BloomDownFilters[i].Clear();
		m_BloomUpFilters[i].Clear();
	}
	// m_BloomResources.clear();
	m_BloomDownFilters.clear();
	m_BloomUpFilters.clear();

	m_BasicSamplingFilter.Clear();
	m_CombineFilter.Clear();

	m_ScreenWidth = 0;
	m_ScreenHeight = 0;

	SAFE_RELEASE(m_pResolvedBuffer);
	/*m_ResolvedRTVOffset = 0xffffffff;
	m_ResolvedSRVOffset = 0xffffffff;*/

	if (m_pScreenMesh)
	{
		ReleaseMesh(&m_pScreenMesh);
	}
}

void PostProcessor::SetDescriptorHeap(ResourceManager* pManager)
{
	m_BasicSamplingFilter.SetDescriptorHeap(pManager);
	m_CombineFilter.SetDescriptorHeap(pManager);
	for (UINT64 i = 0, size = m_BloomDownFilters.size(); i < size; ++i)
	{
		m_BloomDownFilters[i].SetDescriptorHeap(pManager);
	}
	for (UINT64 i = 0, size = m_BloomUpFilters.size(); i < size; ++i)
	{
		m_BloomUpFilters[i].SetDescriptorHeap(pManager);
	}
}

void PostProcessor::createPostBackBuffers(ResourceManager* pManager)
{
	_ASSERT(pManager);

	HRESULT hr = S_OK;
	ID3D12Device5* pDevice = pManager->m_pDevice;
	ID3D12DescriptorHeap* pRTVHeap = pManager->m_pRTVHeap;
	ID3D12DescriptorHeap* pCBVSRVHeap = pManager->m_pCBVSRVUAVHeap;
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle;
	CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(pCBVSRVHeap->GetCPUDescriptorHandleForHeapStart(), pManager->m_CBVSRVUAVHeapSize, pManager->m_CBVSRVUAVDescriptorSize);

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Alignment = 0;
	resourceDesc.Width = m_ScreenWidth;
	resourceDesc.Height = m_ScreenHeight;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProps.CreationNodeMask = 1;
	heapProps.VisibleNodeMask = 1;

	hr = pDevice->CreateCommittedResource(&heapProps,
										  D3D12_HEAP_FLAG_NONE,
										  &resourceDesc,
										  D3D12_RESOURCE_STATE_COMMON,
										  nullptr,
										  IID_PPV_ARGS(&m_pResolvedBuffer));
	BREAK_IF_FAILED(hr);
	m_pResolvedBuffer->SetName(L"ResolvedBuffer");

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = resourceDesc.Format;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = resourceDesc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.PlaneSlice = 0;

	if (m_ResolvedRTVOffset == 0xffffffff)
	{
		rtvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(pRTVHeap->GetCPUDescriptorHandleForHeapStart(), pManager->m_RTVHeapSize, pManager->m_RTVDescriptorSize);
		pDevice->CreateRenderTargetView(m_pResolvedBuffer, &rtvDesc, rtvHandle);
		m_ResolvedRTVOffset = pManager->m_RTVHeapSize;
		++(pManager->m_RTVHeapSize);
	}
	else
	{
		rtvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(pRTVHeap->GetCPUDescriptorHandleForHeapStart(), m_ResolvedRTVOffset, pManager->m_RTVDescriptorSize);
		pDevice->CreateRenderTargetView(m_pResolvedBuffer, &rtvDesc, rtvHandle);
	}

	if (m_ResolvedSRVOffset == 0xffffffff)
	{
		srvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(pCBVSRVHeap->GetCPUDescriptorHandleForHeapStart(), pManager->m_CBVSRVUAVHeapSize, pManager->m_CBVSRVUAVDescriptorSize);
		pDevice->CreateShaderResourceView(m_pResolvedBuffer, &srvDesc, srvHandle);
		m_ResolvedSRVOffset = pManager->m_CBVSRVUAVHeapSize;
		++(pManager->m_CBVSRVUAVHeapSize);
	}
	else
	{
		srvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(pCBVSRVHeap->GetCPUDescriptorHandleForHeapStart(), m_ResolvedSRVOffset, pManager->m_CBVSRVUAVDescriptorSize);
		pDevice->CreateShaderResourceView(m_pResolvedBuffer, &srvDesc, srvHandle);
	}
}

void PostProcessor::createImageResources(ResourceManager* pManager, const int WIDTH, const int HEIGHT, ImageFilter::ImageResource* pImageResource)
{
	_ASSERT(pManager);
	_ASSERT(pImageResource);

	HRESULT hr = S_OK;
	ID3D12Device5* pDevice = pManager->m_pDevice;
	ID3D12DescriptorHeap* pRTVHeap = pManager->m_pRTVHeap;
	ID3D12DescriptorHeap* pCBVSRVHeap = pManager->m_pCBVSRVUAVHeap;
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle;
	CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle;
	static int s_ResourceCount = 0;

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Alignment = 0;
	resourceDesc.Width = WIDTH;
	resourceDesc.Height = HEIGHT;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProps.CreationNodeMask = 1;
	heapProps.VisibleNodeMask = 1;

	hr = pDevice->CreateCommittedResource(&heapProps,
										  D3D12_HEAP_FLAG_NONE,
										  &resourceDesc,
										  D3D12_RESOURCE_STATE_COMMON,
										  nullptr,
										  IID_PPV_ARGS(&(pImageResource->pResource)));
	BREAK_IF_FAILED(hr);

	std::wstring post(std::to_wstring(s_ResourceCount++) + L"]");
	std::wstring debugString(L"ImageResource[");
	debugString += post;
	pImageResource->pResource->SetName(debugString.c_str());

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = resourceDesc.Format;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = resourceDesc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.PlaneSlice = 0;

	if (pImageResource->RTVOffset == 0xffffffff)
	{
		rtvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(pRTVHeap->GetCPUDescriptorHandleForHeapStart(), pManager->m_RTVHeapSize, pManager->m_RTVDescriptorSize);
		pDevice->CreateRenderTargetView(pImageResource->pResource, &rtvDesc, rtvHandle);
		pImageResource->RTVOffset = pManager->m_RTVHeapSize;
		++(pManager->m_RTVHeapSize);
	}
	else
	{
		rtvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(pRTVHeap->GetCPUDescriptorHandleForHeapStart(), pImageResource->RTVOffset, pManager->m_RTVDescriptorSize);
		pDevice->CreateRenderTargetView(pImageResource->pResource, &rtvDesc, rtvHandle);
	}

	if (pImageResource->SRVOffset == 0xffffffff) 
	{
		srvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(pCBVSRVHeap->GetCPUDescriptorHandleForHeapStart(), pManager->m_CBVSRVUAVHeapSize, pManager->m_CBVSRVUAVDescriptorSize);
		pDevice->CreateShaderResourceView(pImageResource->pResource, &srvDesc, srvHandle);
		pImageResource->SRVOffset = pManager->m_CBVSRVUAVHeapSize;
		++(pManager->m_CBVSRVUAVHeapSize);
	}
	else
	{
		srvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(pCBVSRVHeap->GetCPUDescriptorHandleForHeapStart(), pImageResource->SRVOffset, pManager->m_CBVSRVUAVDescriptorSize);
		pDevice->CreateShaderResourceView(pImageResource->pResource, &srvDesc, srvHandle);
	}
}

void PostProcessor::renderPostProcessing(ResourceManager* pManager, UINT frameIndex)
{
	_ASSERT(pManager);

	ID3D12GraphicsCommandList* pCommandList = pManager->m_pSingleCommandList;
	
	// bloom pass 제외.
	/*pManager->SetCommonState(BloomDown);
	for (UINT64 i = 0, size = m_BloomDownFilters.size(); i < size; ++i)
	{
		renderImageFilter(pManager, m_BloomDownFilters[i], BloomDown, frameIndex);
	}
	pManager->SetCommonState(BloomUp);
	for (UINT64 i = 0, size = m_BloomUpFilters.size(); i < size; ++i)
	{
		renderImageFilter(pManager, m_BloomUpFilters[i], BloomUp, frameIndex);
	}*/

	// combine pass
	pManager->SetCommonState(Combine);
	renderImageFilter(pManager, m_CombineFilter, Combine, frameIndex);

	const CD3DX12_RESOURCE_BARRIER BEFORE_BARRIERs[2] =
	{
		CD3DX12_RESOURCE_BARRIER::Transition(m_ppBackBuffers[frameIndex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE),
		CD3DX12_RESOURCE_BARRIER::Transition(m_pPrevBuffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST)
	};
	const CD3DX12_RESOURCE_BARRIER AFTER_BARRIER = CD3DX12_RESOURCE_BARRIER::Transition(m_pPrevBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
	pCommandList->ResourceBarrier(2, BEFORE_BARRIERs);
	pCommandList->CopyResource(m_pPrevBuffer, m_ppBackBuffers[frameIndex]);
	pCommandList->ResourceBarrier(1, &AFTER_BARRIER);
}

void PostProcessor::renderPostProcessing(ResourceManager* pManager, ID3D12GraphicsCommandList* pCommandList, UINT frameIndex)
{
	_ASSERT(pManager);
	_ASSERT(pCommandList);

	// bloom pass 제외.
	/*pManager->SetCommonState(pCommandList, BloomDown);
	for (UINT64 i = 0, size = m_BloomDownFilters.size(); i < size; ++i)
	{
		renderImageFilter(pManager, pCommandList, m_BloomDownFilters[i], BloomDown, frameIndex);
	}
	pManager->SetCommonState(pCommandList, BloomUp);
	for (UINT64 i = 0, size = m_BloomUpFilters.size(); i < size; ++i)
	{
		renderImageFilter(pManager, pCommandList, m_BloomUpFilters[i], BloomUp, frameIndex);
	}*/

	// combine pass
	pManager->SetCommonState(pCommandList, Combine);
	renderImageFilter(pManager, pCommandList, m_CombineFilter, Combine, frameIndex);

	const CD3DX12_RESOURCE_BARRIER BEFORE_BARRIERs[2] =
	{
		CD3DX12_RESOURCE_BARRIER::Transition(m_ppBackBuffers[frameIndex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE),
		CD3DX12_RESOURCE_BARRIER::Transition(m_pPrevBuffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST)
	};
	const CD3DX12_RESOURCE_BARRIER AFTER_BARRIER = CD3DX12_RESOURCE_BARRIER::Transition(m_pPrevBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
	pCommandList->ResourceBarrier(2, BEFORE_BARRIERs);
	pCommandList->CopyResource(m_pPrevBuffer, m_ppBackBuffers[frameIndex]);
	pCommandList->ResourceBarrier(1, &AFTER_BARRIER);
}

void PostProcessor::renderImageFilter(ResourceManager* pManager, ImageFilter& imageFilter, ePipelineStateSetting psoSetting, UINT frameIndex)
{
	imageFilter.BeforeRender(pManager, psoSetting, frameIndex);
	pManager->m_pSingleCommandList->DrawIndexedInstanced(m_pScreenMesh->IndexCount, 1, 0, 0, 0);
	imageFilter.AfterRender(pManager, psoSetting, frameIndex);
}

void PostProcessor::renderImageFilter(ResourceManager* pManager, ID3D12GraphicsCommandList* pCommandList, ImageFilter& imageFilter, ePipelineStateSetting psoSetting, UINT frameIndex)
{
	imageFilter.BeforeRender(pManager, pCommandList, psoSetting, frameIndex);
	pCommandList->DrawIndexedInstanced(m_pScreenMesh->IndexCount, 1, 0, 0, 0);
	imageFilter.AfterRender(pManager, pCommandList, psoSetting, frameIndex);
}

void PostProcessor::setRenderConfig(const PostProcessingBuffers& CONFIG)
{
	m_ppBackBuffers[0] = CONFIG.ppBackBuffers[0];
	m_ppBackBuffers[1] = CONFIG.ppBackBuffers[1];
	m_pFloatBuffer = CONFIG.pFloatBuffer;
	m_pPrevBuffer = CONFIG.pPrevBuffer;
	m_pGlobalConstant = CONFIG.pGlobalConstant;
	m_BackBufferRTV1Offset = CONFIG.BackBufferRTV1Offset;
	m_BackBufferRTV2Offset = CONFIG.BackBufferRTV2Offset;
	m_FloatBufferSRVOffset = CONFIG.FloatBufferSRVOffset;
	m_PrevBufferSRVOffset = CONFIG.PrevBufferSRVOffset;
}
