#include "../pch.h"
#include "../Renderer/ConstantDataType.h"
#include "../Model/GeometryGenerator.h"
#include "../Model/MeshInfo.h"
#include "PostProcessor.h"

void PostProcessor::Initizlie(Renderer* pRenderer, const PostProcessingBuffers& CONFIG, const int WIDTH, const int HEIGHT, const int BLOOMLEVELS)
{
	_ASSERT(pRenderer);

	m_pRenderer = pRenderer;

	HRESULT hr = S_OK;
	ResourceManager* pResourceManager = pRenderer->GetResourceManager();

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

		m_pScreenMesh = new Mesh;

		hr = pResourceManager->CreateVertexBuffer(sizeof(Vertex),
												  (UINT)meshInfo.Vertices.size(),
												  &m_pScreenMesh->Vertex.VertexBufferView,
												  &m_pScreenMesh->Vertex.pBuffer,
												  (void*)meshInfo.Vertices.data());
		BREAK_IF_FAILED(hr);
		m_pScreenMesh->Vertex.Count = (UINT)meshInfo.Vertices.size();

		hr = pResourceManager->CreateIndexBuffer(sizeof(UINT),
												 (UINT)meshInfo.Indices.size(),
												 &m_pScreenMesh->Index.IndexBufferView,
												 &m_pScreenMesh->Index.pBuffer,
												 (void*)meshInfo.Indices.data());
		BREAK_IF_FAILED(hr);
		m_pScreenMesh->Index.Count = (UINT)meshInfo.Indices.size();
	}

	createPostBackBuffers();

	m_BasicSamplingFilter.Initialize(pRenderer, WIDTH, HEIGHT);

	m_BasicSamplingFilter.SetSRVOffsets(pRenderer, { { m_pFloatBuffer, 0xffffffff, m_FloatBufferSRVOffset } });
	m_BasicSamplingFilter.SetRTVOffsets(pRenderer, { { m_pResolvedBuffer, m_ResolvedRTVOffset, 0xffffffff } });

	// Bloom Down/Up 초기화.
	if (m_BloomResources.size() != BLOOMLEVELS)
	{
		m_BloomResources.resize(BLOOMLEVELS);
	}
	for (int i = 0; i < BLOOMLEVELS; ++i)
	{
		int div = (int)pow(2, i);
		createImageResources(WIDTH / div, HEIGHT / div, &m_BloomResources[i]);
	}

	ImageFilter::ImageResource resource;
	m_BloomDownFilters.resize(BLOOMLEVELS - 1);
	m_BloomUpFilters.resize(BLOOMLEVELS - 1);
	for (int i = 0; i < BLOOMLEVELS - 1; ++i)
	{
		int div = (int)pow(2, i + 1);
		m_BloomDownFilters[i].Initialize(pRenderer, WIDTH / div, HEIGHT / div);
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
		m_BloomDownFilters[i].SetSRVOffsets(pRenderer, { resource });

		resource.pResource = m_BloomResources[i + 1].pResource;
		resource.RTVOffset = m_BloomResources[i + 1].RTVOffset;
		resource.SRVOffset = 0xffffffff;
		m_BloomDownFilters[i].SetRTVOffsets(pRenderer, { resource });

		m_BloomDownFilters[i].UpdateConstantBuffers();
	}
	for (int i = 0; i < BLOOMLEVELS - 1; ++i)
	{
		int level = BLOOMLEVELS - 2 - i;
		int div = (int)pow(2, level);
		m_BloomUpFilters[i].Initialize(pRenderer, WIDTH / div, HEIGHT / div);

		resource.pResource = m_BloomResources[level + 1].pResource;
		resource.RTVOffset = 0xffffffff;
		resource.SRVOffset = m_BloomResources[level + 1].SRVOffset;
		m_BloomUpFilters[i].SetSRVOffsets(pRenderer, { resource });

		resource.pResource = m_BloomResources[level].pResource;
		resource.RTVOffset = m_BloomResources[level].RTVOffset;
		resource.SRVOffset = 0xffffffff;
		m_BloomUpFilters[i].SetRTVOffsets(pRenderer, { resource });

		m_BloomUpFilters[i].UpdateConstantBuffers();
	}

	// combine + tone mapping.
	m_CombineFilter.Initialize(pRenderer, WIDTH, HEIGHT);

	m_CombineFilter.SetSRVOffsets(pRenderer, { { m_pResolvedBuffer, 0xffffffff, m_ResolvedSRVOffset }, { m_BloomResources[0].pResource, 0xffffffff, m_BloomResources[0].SRVOffset }, { m_pPrevBuffer, 0xffffffff, m_PrevBufferSRVOffset } });
	m_CombineFilter.SetRTVOffsets(pRenderer, { { m_ppBackBuffers[0], m_BackBufferRTV1Offset, 0xffffffff }, { m_ppBackBuffers[1], m_BackBufferRTV2Offset, 0xffffffff } });

	ImageFilterConstant* pCombineConst = m_CombineFilter.GetConstantDataPtr();
	pCombineConst->Option1 = 0.8f;  // exposure.
	pCombineConst->Option2 = 1.8f;  // gamma.
	m_CombineFilter.UpdateConstantBuffers();
}

void PostProcessor::Update()
{
	_ASSERT(m_pRenderer);
	// 설정을 바꾸지 않으므로 update 없음.
}

void PostProcessor::Render(UINT frameIndex)
{
	_ASSERT(m_pRenderer);

	ResourceManager* pResourceManager = m_pRenderer->GetResourceManager();
	ID3D12GraphicsCommandList* pCommandList = m_pRenderer->GetCommandList();

	pCommandList->RSSetViewports(1, &m_Viewport);
	pCommandList->RSSetScissorRects(1, &m_ScissorRect);

	// 스크린 렌더링을 위한 정점 버퍼와 인텍스 버퍼를 미리 설정.
	pCommandList->IASetVertexBuffers(0, 1, &m_pScreenMesh->Vertex.VertexBufferView);
	pCommandList->IASetIndexBuffer(&m_pScreenMesh->Index.IndexBufferView);

	// basic sampling.
	pResourceManager->SetCommonState(RenderPSOType_Sampling);
	renderImageFilter(m_BasicSamplingFilter, RenderPSOType_Sampling, frameIndex);

	// post processing.
	renderPostProcessing(frameIndex);
}

void PostProcessor::Render(UINT threadIndex, ID3D12GraphicsCommandList* pCommandList, DynamicDescriptorPool* pDescriptorPool, ConstantBufferManager* pConstantBufferManager, ResourceManager* pManager, UINT frameIndex)
{
	_ASSERT(pCommandList);
	_ASSERT(pDescriptorPool);
	_ASSERT(pConstantBufferManager);
	_ASSERT(pManager);

	pCommandList->RSSetViewports(1, &m_Viewport);
	pCommandList->RSSetScissorRects(1, &m_ScissorRect);

	// 스크린 렌더링을 위한 정점 버퍼와 인텍스 버퍼를 미리 설정.
	pCommandList->IASetVertexBuffers(threadIndex, 1, &m_pScreenMesh->Vertex.VertexBufferView);
	pCommandList->IASetIndexBuffer(&m_pScreenMesh->Index.IndexBufferView);

	// basic sampling.
	pManager->SetCommonState(threadIndex, pCommandList, pDescriptorPool, pConstantBufferManager, RenderPSOType_Sampling);
	renderImageFilter(threadIndex, pCommandList, pDescriptorPool, pConstantBufferManager, pManager, m_BasicSamplingFilter, RenderPSOType_Sampling, frameIndex);

	// post processing.
	renderPostProcessing(threadIndex, pCommandList, pDescriptorPool, pConstantBufferManager, pManager, frameIndex);
}

void PostProcessor::Cleanup()
{
	if (!m_pRenderer)
	{
		return;
	}

	ResourceManager* pResourceManager = m_pRenderer->GetResourceManager();
	ID3D12DescriptorHeap* pRTVHeap = m_pRenderer->GetRTVAllocator()->GetDescriptorHeap();
	ID3D12DescriptorHeap* pCBVSRVHeap = m_pRenderer->GetSRVUAVAllocator()->GetDescriptorHeap();

	if (m_ResolvedRTVOffset != 0xffffffff)
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(pRTVHeap->GetCPUDescriptorHandleForHeapStart(), m_ResolvedRTVOffset, pResourceManager->RTVDescriptorSize);
		m_pRenderer->GetRTVAllocator()->FreeDescriptorHandle(rtvHandle);
		m_ResolvedRTVOffset = 0xffffffff;
	}
	if (m_ResolvedSRVOffset != 0xffffffff)
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(pCBVSRVHeap->GetCPUDescriptorHandleForHeapStart(), m_ResolvedSRVOffset, pResourceManager->CBVSRVUAVDescriptorSize);
		m_pRenderer->GetSRVUAVAllocator()->FreeDescriptorHandle(srvHandle);
		m_ResolvedSRVOffset = 0xffffffff;
	}
	SAFE_RELEASE(m_pResolvedBuffer);

	for (UINT64 i = 0, bloomLevels = m_BloomResources.size(); i < bloomLevels; ++i)
	{
		ImageFilter::ImageResource* pImageResource = &m_BloomResources[i];

		if (pImageResource->RTVOffset != 0xffffffff)
		{
			CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(pRTVHeap->GetCPUDescriptorHandleForHeapStart(), pImageResource->RTVOffset, pResourceManager->RTVDescriptorSize);
			m_pRenderer->GetRTVAllocator()->FreeDescriptorHandle(rtvHandle);
			pImageResource->RTVOffset = 0xffffffff;
		}
		if (pImageResource->SRVOffset != 0xffffffff)
		{
			CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(pCBVSRVHeap->GetCPUDescriptorHandleForHeapStart(), pImageResource->SRVOffset, pResourceManager->CBVSRVUAVDescriptorSize);
			m_pRenderer->GetSRVUAVAllocator()->FreeDescriptorHandle(srvHandle);
			pImageResource->SRVOffset = 0xffffffff;
		}

		SAFE_RELEASE(pImageResource->pResource);
	}

	m_ppBackBuffers[0] = nullptr;
	m_ppBackBuffers[1] = nullptr;
	m_pFloatBuffer = nullptr;
	m_pPrevBuffer = nullptr;
	m_pGlobalConstantData = nullptr;
	m_BackBufferRTV1Offset = 0xffffffff;
	m_BackBufferRTV2Offset = 0xffffffff;
	m_FloatBufferSRVOffset = 0xffffffff;
	m_PrevBufferSRVOffset = 0xffffffff;

	for (UINT64 i = 0, size = m_BloomDownFilters.size(); i < size; ++i)
	{
		m_BloomDownFilters[i].Cleanup();
		m_BloomUpFilters[i].Cleanup();
	}
	m_BloomResources.clear();
	m_BloomDownFilters.clear();
	m_BloomUpFilters.clear();

	m_BasicSamplingFilter.Cleanup();
	m_CombineFilter.Cleanup();

	m_ScreenWidth = 0;
	m_ScreenHeight = 0;

	if (m_pScreenMesh)
	{
		delete m_pScreenMesh;
		m_pScreenMesh = nullptr;
	}

	m_pRenderer = nullptr;
}

void PostProcessor::SetViewportsAndScissorRects(ID3D12GraphicsCommandList* pCommandList)
{
	_ASSERT(pCommandList);

	pCommandList->RSSetViewports(1, &m_Viewport);
	pCommandList->RSSetScissorRects(1, &m_ScissorRect);
}

void PostProcessor::createPostBackBuffers()
{
	_ASSERT(m_pRenderer);

	HRESULT hr = S_OK;
	ID3D12Device5* pDevice = m_pRenderer->GetD3DDevice();
	
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

	CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

	hr = pDevice->CreateCommittedResource(&heapProps,
										  D3D12_HEAP_FLAG_NONE,
										  &resourceDesc,
										  D3D12_RESOURCE_STATE_COMMON,
										  nullptr,
										  IID_PPV_ARGS(&m_pResolvedBuffer));
	BREAK_IF_FAILED(hr);
	m_pResolvedBuffer->SetName(L"ResolvedBuffer");


	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle = {};
	CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle = {};

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = resourceDesc.Format;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;

	m_ResolvedRTVOffset = m_pRenderer->GetRTVAllocator()->AllocDescriptorHandle(&rtvHandle);
	pDevice->CreateRenderTargetView(m_pResolvedBuffer, &rtvDesc, rtvHandle);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = resourceDesc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.PlaneSlice = 0;

	m_ResolvedSRVOffset = m_pRenderer->GetSRVUAVAllocator()->AllocDescriptorHandle(&srvHandle);
	pDevice->CreateShaderResourceView(m_pResolvedBuffer, &srvDesc, srvHandle);
}

void PostProcessor::createImageResources(const int WIDTH, const int HEIGHT, ImageFilter::ImageResource* pImageResource)
{
	_ASSERT(m_pRenderer);
	_ASSERT(pImageResource);

	HRESULT hr = S_OK;
	ID3D12Device5* pDevice = m_pRenderer->GetD3DDevice();
	
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

	CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

	hr = pDevice->CreateCommittedResource(&heapProps,
										  D3D12_HEAP_FLAG_NONE,
										  &resourceDesc,
										  D3D12_RESOURCE_STATE_COMMON,
										  nullptr,
										  IID_PPV_ARGS(&(pImageResource->pResource)));
	BREAK_IF_FAILED(hr);

	WCHAR szDebugName[256] = { 0, };
	swprintf_s(szDebugName, 256, L"ImageResource[%d]", s_ResourceCount++);
	pImageResource->pResource->SetName(szDebugName);


	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle = {};
	CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle = {};

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = resourceDesc.Format;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;

	pImageResource->RTVOffset = m_pRenderer->GetRTVAllocator()->AllocDescriptorHandle(&rtvHandle);
	pDevice->CreateRenderTargetView(pImageResource->pResource, &rtvDesc, rtvHandle);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = resourceDesc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.PlaneSlice = 0;

	pImageResource->SRVOffset = m_pRenderer->GetSRVUAVAllocator()->AllocDescriptorHandle(&srvHandle);
	pDevice->CreateShaderResourceView(pImageResource->pResource, &srvDesc, srvHandle);
}

void PostProcessor::renderPostProcessing(UINT frameIndex)
{
	_ASSERT(m_pRenderer);

	ResourceManager* pResourceManager = m_pRenderer->GetResourceManager();
	ID3D12GraphicsCommandList* pCommandList = m_pRenderer->GetCommandList();

	// bloom pass.
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
	pResourceManager->SetCommonState(RenderPSOType_Combine);
	renderImageFilter(m_CombineFilter, RenderPSOType_Combine, frameIndex);

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

void PostProcessor::renderPostProcessing(UINT threadIndex, ID3D12GraphicsCommandList* pCommandList, DynamicDescriptorPool* pDescriptorPool, ConstantBufferManager* pConstantBufferManager, ResourceManager* pResourceManager, UINT frameIndex)
{
	_ASSERT(pCommandList);
	_ASSERT(pDescriptorPool);
	_ASSERT(pConstantBufferManager);
	_ASSERT(pResourceManager);

	// bloom pass.
	/*pManager->SetCommonState(threadIndex, pCommandList, pDescriptorPool, BloomDown);
	for (UINT64 i = 0, size = m_BloomDownFilters.size(); i < size; ++i)
	{
		renderImageFilter(threadIndex, pCommandList, pDescriptorPool, pManager, m_BloomDownFilters[i], BloomDown);
	}
	pManager->SetCommonState(threadIndex, pCommandList, pDescriptorPool, BloomUp);
	for (UINT64 i = 0, size = m_BloomUpFilters.size(); i < size; ++i)
	{
		renderImageFilter(threadIndex, pCommandList, pDescriptorPool, pManager, m_BloomUpFilters[i], BloomUp);
	}*/

	// combine pass
	pResourceManager->SetCommonState(threadIndex, pCommandList, pDescriptorPool, pConstantBufferManager, RenderPSOType_Combine);
	renderImageFilter(threadIndex, pCommandList, pDescriptorPool, pConstantBufferManager, pResourceManager, m_CombineFilter, RenderPSOType_Combine, frameIndex);

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

void PostProcessor::renderImageFilter(ImageFilter& imageFilter, eRenderPSOType psoSetting, UINT frameIndex)
{
	_ASSERT(m_pRenderer);

	ID3D12GraphicsCommandList* pCommandList = m_pRenderer->GetCommandList();

	imageFilter.BeforeRender(m_pRenderer, psoSetting, frameIndex);
	pCommandList->DrawIndexedInstanced(m_pScreenMesh->Index.Count, 1, 0, 0, 0);
	imageFilter.AfterRender(m_pRenderer, psoSetting, frameIndex);
}

void PostProcessor::renderImageFilter(UINT threadIndex, ID3D12GraphicsCommandList* pCommandList, DynamicDescriptorPool* pDescriptorPool, ConstantBufferManager* pConstantBufferManager, ResourceManager* pResourceManager, ImageFilter& imageFilter, int psoSetting, UINT frameIndex)
{
	imageFilter.BeforeRender(threadIndex, pCommandList, pDescriptorPool, pConstantBufferManager, pResourceManager, psoSetting, frameIndex);
	pCommandList->DrawIndexedInstanced(m_pScreenMesh->Index.Count, 1, 0, 0, 0);
	imageFilter.AfterRender(pCommandList, psoSetting);
}

void PostProcessor::setRenderConfig(const PostProcessingBuffers& CONFIG)
{
	m_ppBackBuffers[0] = CONFIG.ppBackBuffers[0];
	m_ppBackBuffers[1] = CONFIG.ppBackBuffers[1];
	m_pFloatBuffer = CONFIG.pFloatBuffer;
	m_pPrevBuffer = CONFIG.pPrevBuffer;
	m_pGlobalConstantData = CONFIG.pGlobalConstantData;
	m_BackBufferRTV1Offset = CONFIG.BackBufferRTV1Offset;
	m_BackBufferRTV2Offset = CONFIG.BackBufferRTV2Offset;
	m_FloatBufferSRVOffset = CONFIG.FloatBufferSRVOffset;
	m_PrevBufferSRVOffset = CONFIG.PrevBufferSRVOffset;
}
