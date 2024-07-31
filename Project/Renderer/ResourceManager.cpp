#include "../pch.h"
#include "../Graphics/GraphicsUtil.h"
#include "../Util/Utility.h"
#include "ResourceManager.h"

void ResourceManager::Initialize(Renderer* pRenderer)
{
	_ASSERT(pRenderer);

	m_pRenderer = pRenderer;
	m_pDevice = pRenderer->GetD3DDevice();
	m_pCommandQueue = pRenderer->GetCommandQueue();

	m_RTVDescriptorSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	m_DSVDescriptorSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	m_CBVSRVUAVDescriptorSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	m_SamplerDescriptorSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

	{
		HRESULT hr = S_OK;
		WCHAR szDebugName[256] = { 0, };

		hr = m_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_pCommandAllocator));
		BREAK_IF_FAILED(hr);
		swprintf_s(szDebugName, 256, L"CommandAllocatorInResourceManager");
		m_pCommandAllocator->SetName(szDebugName);

		hr = m_pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_pCommandAllocator, nullptr, IID_PPV_ARGS(&m_pCommandList));
		BREAK_IF_FAILED(hr);
		swprintf_s(szDebugName, 256, L"CommandListInResourceManager");
		m_pCommandList->SetName(szDebugName);

		// Command lists are created in the recording state, but there is nothing
		// to record yet. The main loop expects it to be closed, so close it now.
		m_pCommandList->Close();
	}

	initSamplers();
	initRasterizerStateDescs();
	initBlendStateDescs();
	initDepthStencilStateDescs();
	initShaders();
	initPipelineStates();
}

HRESULT ResourceManager::CreateVertexBuffer(UINT sizePerVertex, UINT numVertex, D3D12_VERTEX_BUFFER_VIEW* pOutVertexBufferView, ID3D12Resource** ppOutBuffer, void* pInitData)
{
	_ASSERT(m_pRenderer);
	_ASSERT(m_pDevice);
	_ASSERT(m_pCommandQueue);

	HRESULT hr = S_OK;

	D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};
	ID3D12Resource* pVertexBuffer = nullptr;
	ID3D12Resource* pUploadBuffer = nullptr;
	UINT vertexBufferSize = sizePerVertex * numVertex;

	// create vertexbuffer for rendering
	CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
	CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
	hr = m_pDevice->CreateCommittedResource(&heapProps,
											D3D12_HEAP_FLAG_NONE,
											&resourceDesc,
											D3D12_RESOURCE_STATE_COMMON,
											nullptr,
											IID_PPV_ARGS(&pVertexBuffer));
	BREAK_IF_FAILED(hr);
	
	if (pInitData)
	{
		hr = m_pCommandAllocator->Reset();
		BREAK_IF_FAILED(hr);

		hr = m_pCommandList->Reset(m_pCommandAllocator, nullptr);
		BREAK_IF_FAILED(hr);

		heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
		hr = m_pDevice->CreateCommittedResource(&heapProps,
												D3D12_HEAP_FLAG_NONE,
												&resourceDesc,
												D3D12_RESOURCE_STATE_COMMON,
												nullptr,
												IID_PPV_ARGS(&pUploadBuffer));
		BREAK_IF_FAILED(hr);
		pUploadBuffer->SetName(L"tempVertexUploader");

		UINT8* pVertexDataBegin = nullptr;
		CD3DX12_RANGE readRange(0, 0);

		hr = pUploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin));
		BREAK_IF_FAILED(hr);

		memcpy(pVertexDataBegin, pInitData, vertexBufferSize);
		pUploadBuffer->Unmap(0, nullptr);

		const CD3DX12_RESOURCE_BARRIER BEFORE_BARRIER = CD3DX12_RESOURCE_BARRIER::Transition(pVertexBuffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
		const CD3DX12_RESOURCE_BARRIER AFTER_BARRIER = CD3DX12_RESOURCE_BARRIER::Transition(pVertexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		m_pCommandList->ResourceBarrier(1, &BEFORE_BARRIER);
		m_pCommandList->CopyBufferRegion(pVertexBuffer, 0, pUploadBuffer, 0, vertexBufferSize);
		m_pCommandList->ResourceBarrier(1, &AFTER_BARRIER);

		m_pCommandList->Close();

		ID3D12CommandList* ppCommandLists[1] = { m_pCommandList };
		m_pCommandQueue->ExecuteCommandLists(1, ppCommandLists);

		UINT64 lastFenceValue = m_pRenderer->Fence();
		m_pRenderer->WaitForFenceValue(lastFenceValue);

		SAFE_RELEASE(pUploadBuffer);
	}


	// Initialize the vertex buffer view.
	vertexBufferView.BufferLocation = pVertexBuffer->GetGPUVirtualAddress();
	vertexBufferView.StrideInBytes = sizePerVertex;
	vertexBufferView.SizeInBytes = vertexBufferSize;

	*pOutVertexBufferView = vertexBufferView;
	*ppOutBuffer = pVertexBuffer;
	
	return hr;
}

HRESULT ResourceManager::CreateIndexBuffer(UINT sizePerIndex, UINT numIndex, D3D12_INDEX_BUFFER_VIEW* pOutIndexBufferView, ID3D12Resource** ppOutBuffer, void* pInitData)
{
	_ASSERT(m_pDevice);
	_ASSERT(m_pCommandQueue);

	HRESULT hr = S_OK;

	D3D12_INDEX_BUFFER_VIEW indexBufferView = {};
	ID3D12Resource* pIndexBuffer = nullptr;
	ID3D12Resource* pUploadBuffer = nullptr;
	UINT indexBufferSize = sizePerIndex * numIndex;

	// create vertexbuffer for rendering
	CD3DX12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);
	hr = m_pDevice->CreateCommittedResource(&heapProps,
											D3D12_HEAP_FLAG_NONE,
											&resourceDesc,
											D3D12_RESOURCE_STATE_COMMON,
											nullptr,
											IID_PPV_ARGS(&pIndexBuffer));
	BREAK_IF_FAILED(hr);

	if (pInitData)
	{
		hr = m_pCommandAllocator->Reset();
		BREAK_IF_FAILED(hr);

		hr = m_pCommandList->Reset(m_pCommandAllocator, nullptr);
		BREAK_IF_FAILED(hr);

		heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);
		hr = m_pDevice->CreateCommittedResource(&heapProps,
												D3D12_HEAP_FLAG_NONE,
												&resourceDesc,
												D3D12_RESOURCE_STATE_COMMON,
												nullptr,
												IID_PPV_ARGS(&pUploadBuffer));
		BREAK_IF_FAILED(hr);
		pUploadBuffer->SetName(L"tempIndexUploader");

		UINT8* pIndexDataBegin = nullptr;
		CD3DX12_RANGE readRange(0, 0);

		hr = pUploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pIndexDataBegin));
		BREAK_IF_FAILED(hr);

		memcpy(pIndexDataBegin, pInitData, indexBufferSize);
		pUploadBuffer->Unmap(0, nullptr);

		const CD3DX12_RESOURCE_BARRIER BEFORE_BARRIER = CD3DX12_RESOURCE_BARRIER::Transition(pIndexBuffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
		const CD3DX12_RESOURCE_BARRIER AFTER_BARRIER = CD3DX12_RESOURCE_BARRIER::Transition(pIndexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		m_pCommandList->ResourceBarrier(1, &BEFORE_BARRIER);
		m_pCommandList->CopyBufferRegion(pIndexBuffer, 0, pUploadBuffer, 0, indexBufferSize);
		m_pCommandList->ResourceBarrier(1, &AFTER_BARRIER);

		m_pCommandList->Close();

		ID3D12CommandList* ppCommandLists[1] = { m_pCommandList };
		m_pCommandQueue->ExecuteCommandLists(1, ppCommandLists);

		UINT64 lastFenceValue = m_pRenderer->Fence();
		m_pRenderer->WaitForFenceValue(lastFenceValue);

		SAFE_RELEASE(pUploadBuffer);
	}


	// Initialize the vertex buffer view.
	indexBufferView.BufferLocation = pIndexBuffer->GetGPUVirtualAddress();
	indexBufferView.SizeInBytes = indexBufferSize;
	indexBufferView.Format = DXGI_FORMAT_R32_UINT;

	*pOutIndexBufferView = indexBufferView;
	*ppOutBuffer = pIndexBuffer;
	
	return hr;
}

HRESULT ResourceManager::CreateTextureFromFile(ID3D12Resource** ppOutResource, D3D12_RESOURCE_DESC* pOutDesc, const WCHAR* pszFileName, bool bUseSRGB)
{
	_ASSERT(m_pDevice);

	HRESULT hr = S_OK;
	std::vector<UCHAR> image;
	DXGI_FORMAT pixelFormat = (bUseSRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM);

	int width = 0;
	int height = 0;

	if (GetFileExtension(pszFileName).compare(L"exr") == 0)
	{
		hr = ReadEXRImage(pszFileName, image, &width, &height, &pixelFormat);
	}
	else
	{
		hr = ReadImage(pszFileName, image, &width, &height);
	}
	BREAK_IF_FAILED(hr);


	ID3D12Resource* pResource = nullptr;
	D3D12_RESOURCE_DESC textureDesc;
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	textureDesc.Alignment = 0;
	textureDesc.Width = width;
	textureDesc.Height = height;
	textureDesc.DepthOrArraySize = 1;
	textureDesc.MipLevels = 1;
	textureDesc.Format = pixelFormat;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
	D3D12_RESOURCE_STATES curState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	hr = m_pDevice->CreateCommittedResource(&heapProps,
											D3D12_HEAP_FLAG_NONE,
											&textureDesc,
											curState,
											nullptr,
											IID_PPV_ARGS(&pResource));
	BREAK_IF_FAILED(hr);
	pResource->SetName(L"TextureResource");


	ID3D12Resource* pUploadResource = nullptr;
	BYTE* pMappedPtr = nullptr;
	UINT64 pixelSize = GetPixelSize(textureDesc.Format);
	UINT num2DSubresources = textureDesc.DepthOrArraySize * textureDesc.MipLevels;
	UINT64 uploadBufferSize = GetRequiredIntermediateSize(pResource, 0, num2DSubresources);

	D3D12_RESOURCE_DESC desc = pResource->GetDesc();
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footPrint;
	UINT rows = 0;
	UINT64 rowSize = 0;
	UINT64 totalBytes = 0;
	CD3DX12_RESOURCE_DESC uploadResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

	m_pDevice->GetCopyableFootprints(&desc, 0, 1, 0, &footPrint, &rows, &rowSize, &totalBytes);
	heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
	hr = m_pDevice->CreateCommittedResource(&heapProps,
											D3D12_HEAP_FLAG_NONE,
											&uploadResourceDesc,
											D3D12_RESOURCE_STATE_GENERIC_READ,
											nullptr,
											IID_PPV_ARGS(&pUploadResource));
	BREAK_IF_FAILED(hr);
	pUploadResource->SetName(L"TextureUploader");

	CD3DX12_RANGE writeRange(0, 0);
	hr = pUploadResource->Map(0, &writeRange, (void**)(&pMappedPtr));
	BREAK_IF_FAILED(hr);

	const BYTE* pSrc = image.data();
	BYTE* pDest = pMappedPtr;
	for (int i = 0; i < width; ++i)
	{
		memcpy(pDest, pSrc, width * pixelSize);
		pSrc += (width * pixelSize);
		pDest += footPrint.Footprint.RowPitch;
	}

	pUploadResource->Unmap(0, nullptr);
	UpdateTexture(pResource, pUploadResource, &curState);

	SAFE_RELEASE(pUploadResource);

	*ppOutResource = pResource;
	*pOutDesc = textureDesc;

	return hr;
}

HRESULT ResourceManager::CreateTextureCubeFromFile(ID3D12Resource** ppOutResource, D3D12_RESOURCE_DESC* pOutDesc, const WCHAR* pszFileName)
{
	HRESULT hr = S_OK;

	hr = ReadDDSImage(m_pDevice, m_pCommandQueue, pszFileName, ppOutResource);
	BREAK_IF_FAILED(hr);
	(*ppOutResource)->SetName(L"TextureResource");

	*pOutDesc = (*ppOutResource)->GetDesc();

	UINT64 lastFenceValue = m_pRenderer->Fence();
	m_pRenderer->WaitForFenceValue(lastFenceValue);

	return hr;
}

HRESULT ResourceManager::CreateTexturePair(ID3D12Resource** ppOutResource, ID3D12Resource** ppOutUploadBuffer, UINT width, UINT height, DXGI_FORMAT format)
{
	_ASSERT(m_pDevice);

	HRESULT hr = S_OK;

	ID3D12Resource* pTextureResource = nullptr;
	ID3D12Resource* pUploadBuffer = nullptr;

	D3D12_RESOURCE_DESC textureDesc;
	textureDesc.MipLevels = 1;
	textureDesc.Format = format;	// ex) DXGI_FORMAT_R8G8B8A8_UNORM, etc...
	textureDesc.Width = width;
	textureDesc.Height = height;
	textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	textureDesc.DepthOrArraySize = 1;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

	hr = m_pDevice->CreateCommittedResource(&heapProps,
											D3D12_HEAP_FLAG_NONE,
											&textureDesc,
											D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
											nullptr,
											IID_PPV_ARGS(&pTextureResource));
	BREAK_IF_FAILED(hr);
	pTextureResource->SetName(L"TextureResource");


	UINT64 uploadBufferSize = GetRequiredIntermediateSize(pTextureResource, 0, 1);
	CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC uploadHeapDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

	hr = m_pDevice->CreateCommittedResource(&uploadHeapProps,
											D3D12_HEAP_FLAG_NONE,
											&uploadHeapDesc,
											D3D12_RESOURCE_STATE_GENERIC_READ,
											nullptr,
											IID_PPV_ARGS(&pUploadBuffer));
	BREAK_IF_FAILED(hr);
	pUploadBuffer->SetName(L"TextureUploader");

	*ppOutResource = pTextureResource;
	*ppOutUploadBuffer = pUploadBuffer;

	return hr;
}

HRESULT ResourceManager::CreateTexture(ID3D12Resource** ppOutResource, UINT width, UINT height, DXGI_FORMAT format, const BYTE* pInitImage)
{
	_ASSERT(m_pDevice);

	HRESULT hr = S_OK;

	ID3D12Resource* pTextureResource = nullptr;
	ID3D12Resource* pUploadBuffer = nullptr;

	D3D12_RESOURCE_DESC textureDesc;
	textureDesc.MipLevels = 1;
	textureDesc.Format = format;	// ex) DXGI_FORMAT_R8G8B8A8_UNORM, etc...
	textureDesc.Width = width;
	textureDesc.Height = height;
	textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	textureDesc.DepthOrArraySize = 1;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

	hr = m_pDevice->CreateCommittedResource(&heapProps,
											D3D12_HEAP_FLAG_NONE,
											&textureDesc,
											D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
											nullptr,
											IID_PPV_ARGS(&pTextureResource));
	BREAK_IF_FAILED(hr);
	pTextureResource->SetName(L"TextureResource");

	if (pInitImage)
	{
		D3D12_RESOURCE_DESC desc = pTextureResource->GetDesc();
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
		UINT rows = 0;
		UINT64 rowSize = 0;
		UINT64 totalBytes = 0;

		m_pDevice->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, &rows, &rowSize, &totalBytes);

		BYTE* pMappedPtr = nullptr;
		CD3DX12_RANGE writeRange(0, 0);

		UINT64 uploadBufferSize = GetRequiredIntermediateSize(pTextureResource, 0, 1);
		CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
		CD3DX12_RESOURCE_DESC uploadResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

		hr = m_pDevice->CreateCommittedResource(&heapProps,
												D3D12_HEAP_FLAG_NONE,
												&uploadResourceDesc,
												D3D12_RESOURCE_STATE_GENERIC_READ,
												nullptr,
												IID_PPV_ARGS(&pUploadBuffer));
		BREAK_IF_FAILED(hr);
		pUploadBuffer->SetName(L"TextureUploader");

		hr = pUploadBuffer->Map(0, &writeRange, reinterpret_cast<void**>(&pMappedPtr));
		BREAK_IF_FAILED(hr);

		const BYTE* pSrc = pInitImage;
		BYTE* pDest = pMappedPtr;
		for (UINT y = 0; y < height; ++y)
		{
			memcpy(pDest, pSrc, width * 4);
			pSrc += (width * 4);
			pDest += footprint.Footprint.RowPitch;
		}
		// Unmap
		pUploadBuffer->Unmap(0, nullptr);

		// D3D12_RESOURCE_STATES curState = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
		D3D12_RESOURCE_STATES curState = D3D12_RESOURCE_STATE_GENERIC_READ;
		UpdateTexture(pTextureResource, pUploadBuffer, &curState);

		SAFE_RELEASE(pUploadBuffer);
	}
	*ppOutResource = pTextureResource;

	return hr;
}

HRESULT ResourceManager::CreateNonImageUploadTexture(ID3D12Resource** ppOutResource, UINT numElement, UINT elementSize)
{
	_ASSERT(m_pDevice);
	_ASSERT(numElement > 0);
	_ASSERT(elementSize > 0);

	HRESULT hr = S_OK;

	ID3D12Resource* pTextureResource = nullptr;
	ID3D12Resource* pUploadBuffer = nullptr;

	D3D12_RESOURCE_DESC resourceDesc;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Alignment = 0;
	resourceDesc.Width = numElement * elementSize;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);

	hr = m_pDevice->CreateCommittedResource(&heapProps,
											D3D12_HEAP_FLAG_NONE,
											&resourceDesc,
											D3D12_RESOURCE_STATE_GENERIC_READ,
											nullptr,
											IID_PPV_ARGS(&pTextureResource));
	BREAK_IF_FAILED(hr);
	pTextureResource->SetName(L"NonImageTextureResource");

	*ppOutResource = pTextureResource;

	return hr;
}

HRESULT ResourceManager::UpdateTexture(ID3D12Resource* pDestResource, ID3D12Resource* pSrcResource, D3D12_RESOURCE_STATES* pOriginalState)
{
	_ASSERT(m_pRenderer);
	_ASSERT(m_pDevice);

	HRESULT hr = S_OK;

	const UINT MAX_SUB_RESOURCE_NUM = 32;
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint[MAX_SUB_RESOURCE_NUM] = {};
	UINT rows[MAX_SUB_RESOURCE_NUM] = {};
	UINT64 rowSize[MAX_SUB_RESOURCE_NUM] = {};
	UINT64 totalBytes = 0;

	D3D12_RESOURCE_DESC Desc = pDestResource->GetDesc();
	if (Desc.MipLevels > (UINT)_countof(footprint))
	{
		__debugbreak();
	}

	m_pDevice->GetCopyableFootprints(&Desc, 0, Desc.MipLevels, 0, footprint, rows, rowSize, &totalBytes);

	hr = m_pCommandAllocator->Reset();
	BREAK_IF_FAILED(hr);

	hr = m_pCommandList->Reset(m_pCommandAllocator, nullptr);
	BREAK_IF_FAILED(hr);

	const CD3DX12_RESOURCE_BARRIER BEFORE_BARRIER = CD3DX12_RESOURCE_BARRIER::Transition(pDestResource, *pOriginalState, D3D12_RESOURCE_STATE_COPY_DEST);
	const CD3DX12_RESOURCE_BARRIER AFTER_BARRIER = CD3DX12_RESOURCE_BARRIER::Transition(pDestResource, D3D12_RESOURCE_STATE_COPY_DEST, *pOriginalState);
	
	m_pCommandList->ResourceBarrier(1, &BEFORE_BARRIER);
	for (UINT i = 0; i < Desc.MipLevels; ++i)
	{
		D3D12_TEXTURE_COPY_LOCATION	destLocation;
		destLocation.PlacedFootprint = footprint[i];
		destLocation.pResource = pDestResource;
		destLocation.SubresourceIndex = i;
		destLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

		D3D12_TEXTURE_COPY_LOCATION	srcLocation;
		srcLocation.PlacedFootprint = footprint[i];
		srcLocation.pResource = pSrcResource;
		srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;

		m_pCommandList->CopyTextureRegion(&destLocation, 0, 0, 0, &srcLocation, nullptr);
	}
	m_pCommandList->ResourceBarrier(1, &AFTER_BARRIER);
	m_pCommandList->Close();

	ID3D12CommandList* ppCommandLists[1] = { m_pCommandList };
	m_pCommandQueue->ExecuteCommandLists(1, ppCommandLists);

	UINT64 lastFenceValue = m_pRenderer->Fence();
	m_pRenderer->WaitForFenceValue(lastFenceValue);

	return hr;
}

void ResourceManager::Cleanup()
{
	SAFE_RELEASE(m_pCommandAllocator);
	SAFE_RELEASE(m_pCommandList);

	m_pGlobalConstantData = nullptr;
	m_pLightConstantData = nullptr;
	m_pReflectionConstantData = nullptr;
	for (int i = 0; i < MAX_LIGHTS; ++i)
	{
		m_ppLightShadowMaps[i] = nullptr;
	}
	m_pEnvTexture = nullptr;
	m_pIrradianceTexture = nullptr;
	m_pSpecularTexture = nullptr;
	m_pBRDFTexture = nullptr;

	m_GlobalShaderResourceViewStartOffset = 0xffffffff; // t8 ~ t16

	m_RTVDescriptorSize = 0;
	m_DSVDescriptorSize = 0;
	m_CBVSRVUAVDescriptorSize = 0;
	m_SamplerDescriptorSize = 0;

	m_RTVHeapSize = 0;
	m_DSVHeapSize = 0;
	m_CBVSRVUAVHeapSize = 0;
	m_SamplerHeapSize = 0;

	SAFE_RELEASE(m_pDefaultSolidPSO);
	SAFE_RELEASE(m_pSkinnedSolidPSO);
	SAFE_RELEASE(m_pSkyboxSolidPSO);
	SAFE_RELEASE(m_pStencilMaskPSO);
	SAFE_RELEASE(m_pMirrorBlendSolidPSO);
	SAFE_RELEASE(m_pReflectDefaultSolidPSO);
	SAFE_RELEASE(m_pReflectSkinnedSolidPSO);
	SAFE_RELEASE(m_pReflectSkyboxSolidPSO);
	SAFE_RELEASE(m_pDepthOnlyPSO);
	SAFE_RELEASE(m_pDepthOnlySkinnedPSO);
	SAFE_RELEASE(m_pDepthOnlyCubePSO);
	SAFE_RELEASE(m_pDepthOnlyCubeSkinnedPSO);
	SAFE_RELEASE(m_pDepthOnlyCascadePSO);
	SAFE_RELEASE(m_pDepthOnlyCascadeSkinnedPSO);
	SAFE_RELEASE(m_pSamplingPSO);
	SAFE_RELEASE(m_pBloomDownPSO);
	SAFE_RELEASE(m_pBloomUpPSO);
	SAFE_RELEASE(m_pCombinePSO);
	SAFE_RELEASE(m_pDefaultWirePSO);

	SAFE_RELEASE(m_pDefaultRootSignature);
	SAFE_RELEASE(m_pSkinnedRootSignature);
	SAFE_RELEASE(m_pDepthOnlyRootSignature);
	SAFE_RELEASE(m_pDepthOnlySkinnedRootSignature);
	SAFE_RELEASE(m_pDepthOnlyAroundRootSignature);
	SAFE_RELEASE(m_pDepthOnlyAroundSkinnedRootSignature);
	SAFE_RELEASE(m_pSamplingRootSignature);
	SAFE_RELEASE(m_pCombineRootSignature);
	SAFE_RELEASE(m_pDefaultWireRootSignature);

	SAFE_RELEASE(m_pDepthOnlyCascadeGS);
	SAFE_RELEASE(m_pDepthOnlyCubeGS);
	SAFE_RELEASE(m_pColorPS);
	SAFE_RELEASE(m_pBloomUpPS);
	SAFE_RELEASE(m_pBloomDownPS);
	SAFE_RELEASE(m_pCombinePS);
	SAFE_RELEASE(m_pSamplingPS);
	SAFE_RELEASE(m_pDepthOnlyCascadePS);
	SAFE_RELEASE(m_pDepthOnlyCubePS);
	SAFE_RELEASE(m_pDepthOnlyPS);
	SAFE_RELEASE(m_pSkyboxPS);
	SAFE_RELEASE(m_pBasicPS);
	SAFE_RELEASE(m_pSamplingVS);
	SAFE_RELEASE(m_pDepthOnlyCascadeSkinnedVS);
	SAFE_RELEASE(m_pDepthOnlyCascadeVS);
	SAFE_RELEASE(m_pDepthOnlyCubeSkinnedVS);
	SAFE_RELEASE(m_pDepthOnlyCubeVS);
	SAFE_RELEASE(m_pDepthOnlySkinnedVS);
	SAFE_RELEASE(m_pDepthOnlyVS);
	SAFE_RELEASE(m_pSkyboxVS);
	SAFE_RELEASE(m_pSkinnedVS);
	SAFE_RELEASE(m_pBasicVS);

	SAFE_RELEASE(m_pSamplerHeap);

	m_pCommandQueue = nullptr;
	m_pDevice = nullptr;
}

void ResourceManager::SetGlobalConstants(GlobalConstant* pGlobal, LightConstant* pLight, GlobalConstant* pReflection)
{
	m_pGlobalConstantData = pGlobal;
	m_pLightConstantData = pLight;
	m_pReflectionConstantData = pReflection;
}

void ResourceManager::SetGlobalTextures(TextureHandles* pHandles)
{
	for (int i = 0; i < MAX_LIGHTS; ++i)
	{
		m_ppLightShadowMaps[i] = pHandles->ppLightShadowMaps[i];
	}
	m_pEnvTexture = pHandles->pEnvTexture;
	m_pIrradianceTexture = pHandles->pIrradianceTexture;
	m_pSpecularTexture = pHandles->pSpecularTexture;
	m_pBRDFTexture = pHandles->pBRDFTetxure;
}

void ResourceManager::SetCommonState(eRenderPSOType psoState)
{
	_ASSERT(m_pRenderer);
	_ASSERT(m_pDevice);
	_ASSERT(m_pSamplerHeap);

	HRESULT hr = S_OK;
	const float BLEND_FECTOR[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
	ID3D12GraphicsCommandList* pCommandList = m_pRenderer->GetCommandList();
	DynamicDescriptorPool* pDynamicDescriptorPool = m_pRenderer->GetDynamicDescriptorPool();
	ConstantBufferManager* pConstantBufferManager = m_pRenderer->GetConstantBufferManager();
	ConstantBufferPool* pGlobalConstantBufferPool = pConstantBufferManager->GetConstantBufferPool(ConstantBufferType_GlobalConstant);
	ConstantBufferPool* pLightConstantBufferPool = pConstantBufferManager->GetConstantBufferPool(ConstantBufferType_LightConstant);
	const UINT TOTAL_COMMON_SRV_COUNT = 9;

	// set dynamic descriptor heap. (for commont resource)
	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuDescriptorTable({ 0xffffffffffffffff, });
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuDescriptorTable({ 0xffffffffffffffff, });

	switch (psoState)
	{
		case RenderPSOType_Default:
		case RenderPSOType_Skinned:
		case RenderPSOType_Skybox:
		case RenderPSOType_MirrorBlend: 
		case RenderPSOType_DepthOnlyCubeDefault:
		case RenderPSOType_DepthOnlyCubeSkinned:
		case RenderPSOType_DepthOnlyCascadeDefault:
		case RenderPSOType_DepthOnlyCascadeSkinned:
		case RenderPSOType_Sampling:
		case RenderPSOType_BloomDown:
		case RenderPSOType_BloomUp:
		case RenderPSOType_Combine:
		case RenderPSOType_Wire:
		{
			hr = pDynamicDescriptorPool->AllocDescriptorTable(&cpuDescriptorTable, &gpuDescriptorTable, 11);
			BREAK_IF_FAILED(hr);


			CBInfo* pGlobalCB = pGlobalConstantBufferPool->AllocCB();
			CBInfo* pLightCB = pLightConstantBufferPool->AllocCB();

			// Upload constant buffer(global, light).
			BYTE* pGlobalConstMem = pGlobalCB->pSystemMemAddr;
			BYTE* pLightConstMem = pLightCB->pSystemMemAddr;
			memcpy(pGlobalConstMem, m_pGlobalConstantData, sizeof(GlobalConstant));
			memcpy(pLightConstMem, m_pLightConstantData, sizeof(LightConstant));


			CD3DX12_CPU_DESCRIPTOR_HANDLE dstHandle(cpuDescriptorTable);
			
			// b0
			m_pDevice->CopyDescriptorsSimple(1, dstHandle, pGlobalCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			// b1
			m_pDevice->CopyDescriptorsSimple(1, dstHandle, pLightCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			// t8 ~ t16
			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_ppLightShadowMaps[1]->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_NullSRVDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_NullSRVDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_ppLightShadowMaps[0]->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_ppLightShadowMaps[2]->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_pEnvTexture->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_pIrradianceTexture->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_pSpecularTexture->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_pBRDFTexture->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);
		}
		break;

		case RenderPSOType_ReflectionDefault:
		case RenderPSOType_ReflectionSkinned:
		case RenderPSOType_ReflectionSkybox:
		{
			hr = pDynamicDescriptorPool->AllocDescriptorTable(&cpuDescriptorTable, &gpuDescriptorTable, 11);
			BREAK_IF_FAILED(hr);


			CBInfo* pReflectionCB = pGlobalConstantBufferPool->AllocCB();
			CBInfo* pLightCB = pLightConstantBufferPool->AllocCB();

			// Upload constant buffer(reflection, light).
			BYTE* pReflectionConstMem = pReflectionCB->pSystemMemAddr;
			BYTE* pLightConstMem = pLightCB->pSystemMemAddr;
			memcpy(pReflectionConstMem, m_pReflectionConstantData, sizeof(GlobalConstant));
			memcpy(pLightConstMem, m_pLightConstantData, sizeof(LightConstant));


			CD3DX12_CPU_DESCRIPTOR_HANDLE dstHandle(cpuDescriptorTable);
			
			// b0
			m_pDevice->CopyDescriptorsSimple(1, dstHandle, pReflectionCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			// b1
			m_pDevice->CopyDescriptorsSimple(1, dstHandle, pLightCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			// t8 ~ t16
			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_ppLightShadowMaps[1]->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_NullSRVDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_NullSRVDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_ppLightShadowMaps[0]->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_ppLightShadowMaps[2]->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_pEnvTexture->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_pIrradianceTexture->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_pSpecularTexture->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_pBRDFTexture->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);
		}
		break;

		case RenderPSOType_StencilMask:
		case RenderPSOType_DepthOnlyDefault:
		case RenderPSOType_DepthOnlySkinned:
		{
			hr = pDynamicDescriptorPool->AllocDescriptorTable(&cpuDescriptorTable, &gpuDescriptorTable, 10);
			BREAK_IF_FAILED(hr);


			CBInfo* pLightCB = pLightConstantBufferPool->AllocCB();

			// Upload constant buffer(global, light).
			BYTE* pLightConstMem = pLightCB->pSystemMemAddr;
			memcpy(pLightConstMem, m_pLightConstantData, sizeof(LightConstant));


			CD3DX12_CPU_DESCRIPTOR_HANDLE dstHandle(cpuDescriptorTable, 0, m_CBVSRVUAVDescriptorSize);
			
			// b1
			m_pDevice->CopyDescriptorsSimple(1, dstHandle, pLightCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			// t8 ~ t16
			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_ppLightShadowMaps[1]->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_NullSRVDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_NullSRVDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_ppLightShadowMaps[0]->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_ppLightShadowMaps[2]->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_pEnvTexture->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_pIrradianceTexture->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_pSpecularTexture->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_pBRDFTexture->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);
		}
		break;

		default:
			break;
	}

	// set pipeline state for each state.
	switch (psoState)
	{
	case RenderPSOType_Default:
		pCommandList->SetGraphicsRootSignature(m_pDefaultRootSignature);
		pCommandList->SetPipelineState(m_pDefaultSolidPSO);
		pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCommandList->OMSetStencilRef(0);
		pCommandList->SetGraphicsRootDescriptorTable(1, gpuDescriptorTable);
		pCommandList->SetGraphicsRootDescriptorTable(2, m_pSamplerHeap->GetGPUDescriptorHandleForHeapStart());
		break;

	case RenderPSOType_Skinned:
		pCommandList->SetGraphicsRootSignature(m_pSkinnedRootSignature);
		pCommandList->SetPipelineState(m_pSkinnedSolidPSO);
		pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCommandList->OMSetStencilRef(0);
		pCommandList->SetGraphicsRootDescriptorTable(1, gpuDescriptorTable);
		pCommandList->SetGraphicsRootDescriptorTable(2, m_pSamplerHeap->GetGPUDescriptorHandleForHeapStart());
		break;

	case RenderPSOType_Skybox:
		pCommandList->SetGraphicsRootSignature(m_pDefaultRootSignature);
		pCommandList->SetPipelineState(m_pSkyboxSolidPSO);
		pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCommandList->OMSetStencilRef(0);
		pCommandList->SetGraphicsRootDescriptorTable(1, gpuDescriptorTable);
		pCommandList->SetGraphicsRootDescriptorTable(2, m_pSamplerHeap->GetGPUDescriptorHandleForHeapStart());
		break;

	case RenderPSOType_StencilMask:
	{
		CBInfo* pGlobalCB = pGlobalConstantBufferPool->AllocCB();
		BYTE* pGlobalConstMem = pGlobalCB->pSystemMemAddr;
		memcpy(pGlobalConstMem, m_pGlobalConstantData, sizeof(GlobalConstant));

		pCommandList->SetGraphicsRootSignature(m_pDepthOnlyRootSignature);
		pCommandList->SetPipelineState(m_pStencilMaskPSO);
		pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCommandList->OMSetStencilRef(1);
		pCommandList->SetGraphicsRootConstantBufferView(1, pGlobalCB->GPUMemAddr);
		pCommandList->SetGraphicsRootDescriptorTable(2, gpuDescriptorTable);
		pCommandList->SetGraphicsRootDescriptorTable(3, m_pSamplerHeap->GetGPUDescriptorHandleForHeapStart());
	}
	break;

	case RenderPSOType_MirrorBlend:
		pCommandList->SetGraphicsRootSignature(m_pDefaultRootSignature);
		pCommandList->SetPipelineState(m_pMirrorBlendSolidPSO);
		pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCommandList->OMSetStencilRef(1);
		pCommandList->OMSetBlendFactor(BLEND_FECTOR);
		pCommandList->SetGraphicsRootDescriptorTable(1, gpuDescriptorTable);
		pCommandList->SetGraphicsRootDescriptorTable(2, m_pSamplerHeap->GetGPUDescriptorHandleForHeapStart());
		break;

	case RenderPSOType_ReflectionDefault:
		pCommandList->SetGraphicsRootSignature(m_pDefaultRootSignature);
		pCommandList->SetPipelineState(m_pReflectDefaultSolidPSO);
		pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCommandList->OMSetStencilRef(1);
		pCommandList->SetGraphicsRootDescriptorTable(1, gpuDescriptorTable);
		pCommandList->SetGraphicsRootDescriptorTable(2, m_pSamplerHeap->GetGPUDescriptorHandleForHeapStart());
		break;

	case RenderPSOType_ReflectionSkinned:
		pCommandList->SetGraphicsRootSignature(m_pSkinnedRootSignature);
		pCommandList->SetPipelineState(m_pReflectSkinnedSolidPSO);
		pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCommandList->OMSetStencilRef(1);
		pCommandList->SetGraphicsRootDescriptorTable(1, gpuDescriptorTable);
		pCommandList->SetGraphicsRootDescriptorTable(2, m_pSamplerHeap->GetGPUDescriptorHandleForHeapStart());
		break;

	case RenderPSOType_ReflectionSkybox:
		pCommandList->SetGraphicsRootSignature(m_pDefaultRootSignature);
		pCommandList->SetPipelineState(m_pReflectSkyboxSolidPSO);
		pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCommandList->OMSetStencilRef(1);
		pCommandList->SetGraphicsRootDescriptorTable(1, gpuDescriptorTable);
		pCommandList->SetGraphicsRootDescriptorTable(2, m_pSamplerHeap->GetGPUDescriptorHandleForHeapStart());
		break;

	case RenderPSOType_DepthOnlyDefault:
		pCommandList->SetGraphicsRootSignature(m_pDepthOnlyRootSignature);
		pCommandList->SetPipelineState(m_pDepthOnlyPSO);
		pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCommandList->OMSetStencilRef(0);
		pCommandList->SetGraphicsRootDescriptorTable(2, gpuDescriptorTable);
		pCommandList->SetGraphicsRootDescriptorTable(3, m_pSamplerHeap->GetGPUDescriptorHandleForHeapStart());
		break;

	case RenderPSOType_DepthOnlySkinned:
		pCommandList->SetGraphicsRootSignature(m_pDepthOnlySkinnedRootSignature);
		pCommandList->SetPipelineState(m_pDepthOnlySkinnedPSO);
		pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCommandList->OMSetStencilRef(0);
		pCommandList->SetGraphicsRootDescriptorTable(2, gpuDescriptorTable);
		pCommandList->SetGraphicsRootDescriptorTable(3, m_pSamplerHeap->GetGPUDescriptorHandleForHeapStart());
		break;

	case RenderPSOType_DepthOnlyCubeDefault:
		pCommandList->SetGraphicsRootSignature(m_pDepthOnlyAroundRootSignature);
		pCommandList->SetPipelineState(m_pDepthOnlyCubePSO);
		pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCommandList->OMSetStencilRef(0);
		pCommandList->SetGraphicsRootDescriptorTable(2, gpuDescriptorTable);
		pCommandList->SetGraphicsRootDescriptorTable(3, m_pSamplerHeap->GetGPUDescriptorHandleForHeapStart());
		break;

	case RenderPSOType_DepthOnlyCubeSkinned:
		pCommandList->SetGraphicsRootSignature(m_pDepthOnlyAroundSkinnedRootSignature);
		pCommandList->SetPipelineState(m_pDepthOnlyCubeSkinnedPSO);
		pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCommandList->OMSetStencilRef(0);
		pCommandList->SetGraphicsRootDescriptorTable(2, gpuDescriptorTable);
		pCommandList->SetGraphicsRootDescriptorTable(3, m_pSamplerHeap->GetGPUDescriptorHandleForHeapStart());
		break;

	case RenderPSOType_DepthOnlyCascadeDefault:
		pCommandList->SetGraphicsRootSignature(m_pDepthOnlyAroundRootSignature);
		pCommandList->SetPipelineState(m_pDepthOnlyCascadePSO);
		pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCommandList->OMSetStencilRef(0);
		pCommandList->SetGraphicsRootDescriptorTable(2, gpuDescriptorTable);
		pCommandList->SetGraphicsRootDescriptorTable(3, m_pSamplerHeap->GetGPUDescriptorHandleForHeapStart());
		break;

	case RenderPSOType_DepthOnlyCascadeSkinned:
		pCommandList->SetGraphicsRootSignature(m_pDepthOnlyAroundSkinnedRootSignature);
		pCommandList->SetPipelineState(m_pDepthOnlyCascadeSkinnedPSO);
		pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCommandList->OMSetStencilRef(0);
		pCommandList->SetGraphicsRootDescriptorTable(2, gpuDescriptorTable);
		pCommandList->SetGraphicsRootDescriptorTable(3, m_pSamplerHeap->GetGPUDescriptorHandleForHeapStart());
		break;

	case RenderPSOType_Sampling:
		pCommandList->SetGraphicsRootSignature(m_pSamplingRootSignature);
		pCommandList->SetPipelineState(m_pSamplingPSO);
		pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCommandList->OMSetStencilRef(0);
		pCommandList->SetGraphicsRootDescriptorTable(1, gpuDescriptorTable);
		pCommandList->SetGraphicsRootDescriptorTable(2, m_pSamplerHeap->GetGPUDescriptorHandleForHeapStart());
		break;

	case RenderPSOType_BloomDown:
		pCommandList->SetGraphicsRootSignature(m_pSamplingRootSignature);
		pCommandList->SetPipelineState(m_pBloomDownPSO);
		pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCommandList->OMSetStencilRef(0);
		pCommandList->SetGraphicsRootDescriptorTable(1, gpuDescriptorTable);
		pCommandList->SetGraphicsRootDescriptorTable(2, m_pSamplerHeap->GetGPUDescriptorHandleForHeapStart());
		break;

	case RenderPSOType_BloomUp:
		pCommandList->SetGraphicsRootSignature(m_pSamplingRootSignature);
		pCommandList->SetPipelineState(m_pBloomUpPSO);
		pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCommandList->OMSetStencilRef(0);
		pCommandList->SetGraphicsRootDescriptorTable(1, gpuDescriptorTable);
		pCommandList->SetGraphicsRootDescriptorTable(2, m_pSamplerHeap->GetGPUDescriptorHandleForHeapStart());
		break;

	case RenderPSOType_Combine:
		pCommandList->SetGraphicsRootSignature(m_pCombineRootSignature);
		pCommandList->SetPipelineState(m_pCombinePSO);
		pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCommandList->OMSetStencilRef(0);
		pCommandList->SetGraphicsRootDescriptorTable(1, gpuDescriptorTable);
		pCommandList->SetGraphicsRootDescriptorTable(2, m_pSamplerHeap->GetGPUDescriptorHandleForHeapStart());
		break;

	case RenderPSOType_Wire:
		pCommandList->SetGraphicsRootSignature(m_pDefaultWireRootSignature);
		pCommandList->SetPipelineState(m_pDefaultWirePSO);
		pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
		pCommandList->OMSetStencilRef(0);
		pCommandList->SetGraphicsRootDescriptorTable(1, gpuDescriptorTable);
		pCommandList->SetGraphicsRootDescriptorTable(2, m_pSamplerHeap->GetGPUDescriptorHandleForHeapStart());
		break;

	default:
		__debugbreak();
		break;
	}
}

void ResourceManager::SetCommonState(UINT threadIndex, ID3D12GraphicsCommandList* pCommandList, DynamicDescriptorPool* pDescriptorPool, ConstantBufferManager* pConstantBufferManager, int psoState)
{
	_ASSERT(pCommandList);
	_ASSERT(pDescriptorPool);
	_ASSERT(pConstantBufferManager);

	HRESULT hr = S_OK;
	const float BLEND_FECTOR[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
	ConstantBufferPool* pGlobalConstantBufferPool = pConstantBufferManager->GetConstantBufferPool(ConstantBufferType_GlobalConstant);
	ConstantBufferPool* pLightConstantBufferPool = pConstantBufferManager->GetConstantBufferPool(ConstantBufferType_LightConstant);
	const UINT TOTAL_COMMON_SRV_COUNT = 9;

	// set dynamic descriptor heap. (for commont resource)
	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuDescriptorTable({ 0xffffffffffffffff, });
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuDescriptorTable({ 0xffffffffffffffff, });

	switch (psoState)
	{
		case RenderPSOType_Default:
		case RenderPSOType_Skinned:
		case RenderPSOType_Skybox:
		case RenderPSOType_MirrorBlend:
		case RenderPSOType_DepthOnlyCubeDefault:
		case RenderPSOType_DepthOnlyCubeSkinned:
		case RenderPSOType_DepthOnlyCascadeDefault:
		case RenderPSOType_DepthOnlyCascadeSkinned:
		case RenderPSOType_Sampling:
		case RenderPSOType_BloomDown:
		case RenderPSOType_BloomUp:
		case RenderPSOType_Combine:
		case RenderPSOType_Wire:
		{
			hr = pDescriptorPool->AllocDescriptorTable(&cpuDescriptorTable, &gpuDescriptorTable, 11);
			BREAK_IF_FAILED(hr);


			CBInfo* pGlobalCB = pGlobalConstantBufferPool->AllocCB();
			CBInfo* pLightCB = pLightConstantBufferPool->AllocCB();

			// Upload constant buffer(global, light).
			BYTE* pGloablConstMem = pGlobalCB->pSystemMemAddr;
			BYTE* pLightConstMem = pLightCB->pSystemMemAddr;
			memcpy(pGloablConstMem, m_pGlobalConstantData, sizeof(GlobalConstant));
			memcpy(pLightConstMem, m_pLightConstantData, sizeof(LightConstant));


			CD3DX12_CPU_DESCRIPTOR_HANDLE dstHandle(cpuDescriptorTable, 0, m_CBVSRVUAVDescriptorSize);
			
			// b0
			m_pDevice->CopyDescriptorsSimple(1, dstHandle, pGlobalCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			// b1
			m_pDevice->CopyDescriptorsSimple(1, dstHandle, pLightCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			// t8 ~ t16
			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_ppLightShadowMaps[1]->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_NullSRVDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_NullSRVDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_ppLightShadowMaps[0]->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_ppLightShadowMaps[2]->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_pEnvTexture->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_pIrradianceTexture->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_pSpecularTexture->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_pBRDFTexture->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);
		}
		break;

		case RenderPSOType_ReflectionDefault:
		case RenderPSOType_ReflectionSkinned:
		case RenderPSOType_ReflectionSkybox:
		{
			hr = pDescriptorPool->AllocDescriptorTable(&cpuDescriptorTable, &gpuDescriptorTable, 11);
			BREAK_IF_FAILED(hr);


			CBInfo* pReflectionCB = pGlobalConstantBufferPool->AllocCB();
			CBInfo* pLightCB = pLightConstantBufferPool->AllocCB();

			// Upload constant buffer(reflection, light).
			BYTE* pReflectionConstMem = pReflectionCB->pSystemMemAddr;
			BYTE* pLightConstMem = pLightCB->pSystemMemAddr;
			memcpy(pReflectionConstMem, m_pReflectionConstantData, sizeof(GlobalConstant));
			memcpy(pLightConstMem, m_pLightConstantData, sizeof(LightConstant));


			CD3DX12_CPU_DESCRIPTOR_HANDLE dstHandle(cpuDescriptorTable, 0, m_CBVSRVUAVDescriptorSize);
			
			// b0
			m_pDevice->CopyDescriptorsSimple(1, dstHandle, pReflectionCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			// b1
			m_pDevice->CopyDescriptorsSimple(1, dstHandle, pReflectionCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			// t8 ~ t16
			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_ppLightShadowMaps[1]->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_NullSRVDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_NullSRVDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_ppLightShadowMaps[0]->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_ppLightShadowMaps[2]->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_pEnvTexture->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_pIrradianceTexture->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_pSpecularTexture->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_pBRDFTexture->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);
		}
		break;

		case RenderPSOType_StencilMask:
		case RenderPSOType_DepthOnlyDefault:
		case RenderPSOType_DepthOnlySkinned:
		{
			hr = pDescriptorPool->AllocDescriptorTable(&cpuDescriptorTable, &gpuDescriptorTable, 10);
			BREAK_IF_FAILED(hr);


			CBInfo* pLightCB = pLightConstantBufferPool->AllocCB();

			// Upload constant buffer(light).
			BYTE* pLightConstMem = pLightCB->pSystemMemAddr;
			memcpy(pLightConstMem, m_pLightConstantData, sizeof(LightConstant));


			CD3DX12_CPU_DESCRIPTOR_HANDLE dstHandle(cpuDescriptorTable, 0, m_CBVSRVUAVDescriptorSize);
			
			// b1
			m_pDevice->CopyDescriptorsSimple(1, dstHandle, pLightCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			// t8 ~ t16
			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_ppLightShadowMaps[1]->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_NullSRVDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_NullSRVDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_ppLightShadowMaps[0]->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_ppLightShadowMaps[2]->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_pEnvTexture->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_pIrradianceTexture->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_pSpecularTexture->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);

			m_pDevice->CopyDescriptorsSimple(1, dstHandle, m_pBRDFTexture->SRVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			dstHandle.Offset(1, m_CBVSRVUAVDescriptorSize);
		}
		break;

		default:
			break;
	}

	// set pipeline state for each state.
	switch (psoState)
	{
		case RenderPSOType_Default:
			pCommandList->SetGraphicsRootSignature(m_pDefaultRootSignature);
			pCommandList->SetPipelineState(m_pDefaultSolidPSO);
			pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			pCommandList->OMSetStencilRef(0);
			pCommandList->SetGraphicsRootDescriptorTable(1, gpuDescriptorTable);
			pCommandList->SetGraphicsRootDescriptorTable(2, m_pSamplerHeap->GetGPUDescriptorHandleForHeapStart());
			break;

		case RenderPSOType_Skinned:
			pCommandList->SetGraphicsRootSignature(m_pSkinnedRootSignature);
			pCommandList->SetPipelineState(m_pSkinnedSolidPSO);
			pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			pCommandList->OMSetStencilRef(0);
			pCommandList->SetGraphicsRootDescriptorTable(1, gpuDescriptorTable);
			pCommandList->SetGraphicsRootDescriptorTable(2, m_pSamplerHeap->GetGPUDescriptorHandleForHeapStart());
			break;

		case RenderPSOType_Skybox:
			pCommandList->SetGraphicsRootSignature(m_pDefaultRootSignature);
			pCommandList->SetPipelineState(m_pSkyboxSolidPSO);
			pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			pCommandList->OMSetStencilRef(0);
			pCommandList->SetGraphicsRootDescriptorTable(1, gpuDescriptorTable);
			pCommandList->SetGraphicsRootDescriptorTable(2, m_pSamplerHeap->GetGPUDescriptorHandleForHeapStart());
			break;

		case RenderPSOType_StencilMask:
		{
			CBInfo* pGlobalCB = pGlobalConstantBufferPool->AllocCB();
			BYTE* pGlobalConstMem = pGlobalCB->pSystemMemAddr;
			memcpy(pGlobalConstMem, m_pGlobalConstantData, sizeof(GlobalConstant));

			pCommandList->SetGraphicsRootSignature(m_pDepthOnlyRootSignature);
			pCommandList->SetPipelineState(m_pStencilMaskPSO);
			pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			pCommandList->OMSetStencilRef(1);
			pCommandList->SetGraphicsRootConstantBufferView(1, pGlobalCB->GPUMemAddr);
			pCommandList->SetGraphicsRootDescriptorTable(2, gpuDescriptorTable);
			pCommandList->SetGraphicsRootDescriptorTable(3, m_pSamplerHeap->GetGPUDescriptorHandleForHeapStart());
		}
		break;

		case RenderPSOType_MirrorBlend:
			pCommandList->SetGraphicsRootSignature(m_pDefaultRootSignature);
			pCommandList->SetPipelineState(m_pMirrorBlendSolidPSO);
			pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			pCommandList->OMSetStencilRef(1);
			pCommandList->OMSetBlendFactor(BLEND_FECTOR);
			pCommandList->SetGraphicsRootDescriptorTable(1, gpuDescriptorTable);
			pCommandList->SetGraphicsRootDescriptorTable(2, m_pSamplerHeap->GetGPUDescriptorHandleForHeapStart());
			break;

		case RenderPSOType_ReflectionDefault:
			pCommandList->SetGraphicsRootSignature(m_pDefaultRootSignature);
			pCommandList->SetPipelineState(m_pReflectDefaultSolidPSO);
			pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			pCommandList->OMSetStencilRef(1);
			pCommandList->SetGraphicsRootDescriptorTable(1, gpuDescriptorTable);
			pCommandList->SetGraphicsRootDescriptorTable(2, m_pSamplerHeap->GetGPUDescriptorHandleForHeapStart());
			break;

		case RenderPSOType_ReflectionSkinned:
			pCommandList->SetGraphicsRootSignature(m_pSkinnedRootSignature);
			pCommandList->SetPipelineState(m_pReflectSkinnedSolidPSO);
			pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			pCommandList->OMSetStencilRef(1);
			pCommandList->SetGraphicsRootDescriptorTable(1, gpuDescriptorTable);
			pCommandList->SetGraphicsRootDescriptorTable(2, m_pSamplerHeap->GetGPUDescriptorHandleForHeapStart());
			break;

		case RenderPSOType_ReflectionSkybox:
			pCommandList->SetGraphicsRootSignature(m_pDefaultRootSignature);
			pCommandList->SetPipelineState(m_pReflectSkyboxSolidPSO);
			pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			pCommandList->OMSetStencilRef(1);
			pCommandList->SetGraphicsRootDescriptorTable(1, gpuDescriptorTable);
			pCommandList->SetGraphicsRootDescriptorTable(2, m_pSamplerHeap->GetGPUDescriptorHandleForHeapStart());
			break;

		case RenderPSOType_DepthOnlyDefault:
			pCommandList->SetGraphicsRootSignature(m_pDepthOnlyRootSignature);
			pCommandList->SetPipelineState(m_pDepthOnlyPSO);
			pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			pCommandList->OMSetStencilRef(0);
			pCommandList->SetGraphicsRootDescriptorTable(2, gpuDescriptorTable);
			pCommandList->SetGraphicsRootDescriptorTable(3, m_pSamplerHeap->GetGPUDescriptorHandleForHeapStart());
			break;

		case RenderPSOType_DepthOnlySkinned:
			pCommandList->SetGraphicsRootSignature(m_pDepthOnlySkinnedRootSignature);
			pCommandList->SetPipelineState(m_pDepthOnlySkinnedPSO);
			pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			pCommandList->OMSetStencilRef(0);
			pCommandList->SetGraphicsRootDescriptorTable(2, gpuDescriptorTable);
			pCommandList->SetGraphicsRootDescriptorTable(3, m_pSamplerHeap->GetGPUDescriptorHandleForHeapStart());
			break;

		case RenderPSOType_DepthOnlyCubeDefault:
			pCommandList->SetGraphicsRootSignature(m_pDepthOnlyAroundRootSignature);
			pCommandList->SetPipelineState(m_pDepthOnlyCubePSO);
			pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			pCommandList->OMSetStencilRef(0);
			pCommandList->SetGraphicsRootDescriptorTable(2, gpuDescriptorTable);
			pCommandList->SetGraphicsRootDescriptorTable(3, m_pSamplerHeap->GetGPUDescriptorHandleForHeapStart());
			break;

		case RenderPSOType_DepthOnlyCubeSkinned:
			pCommandList->SetGraphicsRootSignature(m_pDepthOnlyAroundSkinnedRootSignature);
			pCommandList->SetPipelineState(m_pDepthOnlyCubeSkinnedPSO);
			pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			pCommandList->OMSetStencilRef(0);
			pCommandList->SetGraphicsRootDescriptorTable(2, gpuDescriptorTable);
			pCommandList->SetGraphicsRootDescriptorTable(3, m_pSamplerHeap->GetGPUDescriptorHandleForHeapStart());
			break;

		case RenderPSOType_DepthOnlyCascadeDefault:
			pCommandList->SetGraphicsRootSignature(m_pDepthOnlyAroundRootSignature);
			pCommandList->SetPipelineState(m_pDepthOnlyCascadePSO);
			pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			pCommandList->OMSetStencilRef(0);
			pCommandList->SetGraphicsRootDescriptorTable(2, gpuDescriptorTable);
			pCommandList->SetGraphicsRootDescriptorTable(3, m_pSamplerHeap->GetGPUDescriptorHandleForHeapStart());
			break;

		case RenderPSOType_DepthOnlyCascadeSkinned:
			pCommandList->SetGraphicsRootSignature(m_pDepthOnlyAroundSkinnedRootSignature);
			pCommandList->SetPipelineState(m_pDepthOnlyCascadeSkinnedPSO);
			pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			pCommandList->OMSetStencilRef(0);
			pCommandList->SetGraphicsRootDescriptorTable(2, gpuDescriptorTable);
			pCommandList->SetGraphicsRootDescriptorTable(3, m_pSamplerHeap->GetGPUDescriptorHandleForHeapStart());
			break;

		case RenderPSOType_Sampling:
			pCommandList->SetGraphicsRootSignature(m_pSamplingRootSignature);
			pCommandList->SetPipelineState(m_pSamplingPSO);
			pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			pCommandList->OMSetStencilRef(0);
			pCommandList->SetGraphicsRootDescriptorTable(1, gpuDescriptorTable);
			pCommandList->SetGraphicsRootDescriptorTable(2, m_pSamplerHeap->GetGPUDescriptorHandleForHeapStart());
			break;

		case RenderPSOType_BloomDown:
			pCommandList->SetGraphicsRootSignature(m_pSamplingRootSignature);
			pCommandList->SetPipelineState(m_pBloomDownPSO);
			pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			pCommandList->OMSetStencilRef(0);
			pCommandList->SetGraphicsRootDescriptorTable(1, gpuDescriptorTable);
			pCommandList->SetGraphicsRootDescriptorTable(2, m_pSamplerHeap->GetGPUDescriptorHandleForHeapStart());
			break;

		case RenderPSOType_BloomUp:
			pCommandList->SetGraphicsRootSignature(m_pSamplingRootSignature);
			pCommandList->SetPipelineState(m_pBloomUpPSO);
			pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			pCommandList->OMSetStencilRef(0);
			pCommandList->SetGraphicsRootDescriptorTable(1, gpuDescriptorTable);
			pCommandList->SetGraphicsRootDescriptorTable(2, m_pSamplerHeap->GetGPUDescriptorHandleForHeapStart());
			break;

		case RenderPSOType_Combine:
			pCommandList->SetGraphicsRootSignature(m_pCombineRootSignature);
			pCommandList->SetPipelineState(m_pCombinePSO);
			pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			pCommandList->OMSetStencilRef(0);
			pCommandList->SetGraphicsRootDescriptorTable(1, gpuDescriptorTable);
			pCommandList->SetGraphicsRootDescriptorTable(2, m_pSamplerHeap->GetGPUDescriptorHandleForHeapStart());
			break;

		case RenderPSOType_Wire:
			pCommandList->SetGraphicsRootSignature(m_pDefaultWireRootSignature);
			pCommandList->SetPipelineState(m_pDefaultWirePSO);
			pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
			pCommandList->OMSetStencilRef(0);
			pCommandList->SetGraphicsRootDescriptorTable(1, gpuDescriptorTable);
			pCommandList->SetGraphicsRootDescriptorTable(2, m_pSamplerHeap->GetGPUDescriptorHandleForHeapStart());
			break;
			break;

		default:
			__debugbreak();
			break;
	}
}

void ResourceManager::initSamplers()
{
	HRESULT hr = S_OK;
	
	D3D12_SAMPLER_DESC samplerLinearWrapDesc = {};
	D3D12_SAMPLER_DESC samplerLinearClampDesc = {};
	D3D12_SAMPLER_DESC samplerPointWrapDesc = {};
	D3D12_SAMPLER_DESC samplerPointClampDesc = {};
	D3D12_SAMPLER_DESC samplerShadowPointDesc = {};
	D3D12_SAMPLER_DESC samplerShadowLinearDesc = {};
	D3D12_SAMPLER_DESC samplerShadowCompareDesc = {};
	D3D12_SAMPLER_DESC samplerLinearMirrorDesc = {};

	ZeroMemory(&samplerLinearWrapDesc, sizeof(samplerLinearWrapDesc));
	ZeroMemory(&samplerLinearClampDesc, sizeof(samplerLinearClampDesc));
	ZeroMemory(&samplerPointWrapDesc, sizeof(samplerPointWrapDesc));
	ZeroMemory(&samplerPointClampDesc, sizeof(samplerPointClampDesc));
	ZeroMemory(&samplerShadowPointDesc, sizeof(samplerShadowPointDesc));
	ZeroMemory(&samplerShadowLinearDesc, sizeof(samplerShadowLinearDesc));
	ZeroMemory(&samplerShadowCompareDesc, sizeof(samplerShadowCompareDesc));
	ZeroMemory(&samplerLinearMirrorDesc, sizeof(samplerLinearMirrorDesc));

	samplerLinearWrapDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	samplerLinearWrapDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerLinearWrapDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerLinearWrapDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerLinearWrapDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	samplerLinearWrapDesc.MinLOD = 0.0f;
	samplerLinearWrapDesc.MaxLOD = D3D12_FLOAT32_MAX;

	samplerLinearClampDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	samplerLinearClampDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerLinearClampDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerLinearClampDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerLinearClampDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	samplerLinearClampDesc.MinLOD = 0.0f;
	samplerLinearClampDesc.MaxLOD = D3D12_FLOAT32_MAX;

	samplerPointWrapDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	samplerPointWrapDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerPointWrapDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerPointWrapDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerPointWrapDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	samplerPointWrapDesc.MinLOD = 0.0f;
	samplerPointWrapDesc.MaxLOD = D3D12_FLOAT32_MAX;

	samplerPointClampDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	samplerPointClampDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerPointClampDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerPointClampDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerPointClampDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	samplerPointClampDesc.MinLOD = 0.0f;
	samplerPointClampDesc.MaxLOD = D3D12_FLOAT32_MAX;

	samplerShadowPointDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	samplerShadowPointDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	samplerShadowPointDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	samplerShadowPointDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	samplerShadowPointDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	samplerShadowPointDesc.BorderColor[0] = 1.0f;
	samplerShadowPointDesc.MinLOD = 0.0f;
	samplerShadowPointDesc.MaxLOD = D3D12_FLOAT32_MAX;

	samplerShadowLinearDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	samplerShadowLinearDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	samplerShadowLinearDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	samplerShadowLinearDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	samplerShadowLinearDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	samplerShadowLinearDesc.BorderColor[0] = 100.0f;
	samplerShadowLinearDesc.MinLOD = 0.0f;
	samplerShadowLinearDesc.MaxLOD = D3D12_FLOAT32_MAX;

	samplerShadowCompareDesc.Filter = D3D12_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT;
	samplerShadowCompareDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	samplerShadowCompareDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	samplerShadowCompareDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	samplerShadowCompareDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_EQUAL;
	samplerShadowCompareDesc.BorderColor[0] = 100.0f;
	samplerShadowCompareDesc.MinLOD = 0.0f;
	samplerShadowCompareDesc.MaxLOD = D3D12_FLOAT32_MAX;

	samplerLinearMirrorDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	samplerLinearMirrorDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
	samplerLinearMirrorDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
	samplerLinearMirrorDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
	samplerLinearMirrorDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	samplerLinearMirrorDesc.MinLOD = 0.0f;
	samplerLinearMirrorDesc.MaxLOD = D3D12_FLOAT32_MAX;


	D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc;
	samplerHeapDesc.NumDescriptors = 8;
	samplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
	samplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	samplerHeapDesc.NodeMask = 0;

	hr = m_pDevice->CreateDescriptorHeap(&samplerHeapDesc, IID_PPV_ARGS(&m_pSamplerHeap));
	BREAK_IF_FAILED(hr);
	m_pSamplerHeap->SetName(L"SamplerDescriptorHeap");

	CD3DX12_CPU_DESCRIPTOR_HANDLE samplerStartHandle(m_pSamplerHeap->GetCPUDescriptorHandleForHeapStart());

	m_pDevice->CreateSampler(&samplerLinearWrapDesc, samplerStartHandle);
	samplerStartHandle.Offset(1, m_SamplerDescriptorSize);

	m_pDevice->CreateSampler(&samplerLinearClampDesc, samplerStartHandle);
	samplerStartHandle.Offset(1, m_SamplerDescriptorSize);

	m_pDevice->CreateSampler(&samplerPointWrapDesc, samplerStartHandle);
	samplerStartHandle.Offset(1, m_SamplerDescriptorSize);

	m_pDevice->CreateSampler(&samplerPointClampDesc, samplerStartHandle);
	samplerStartHandle.Offset(1, m_SamplerDescriptorSize);

	m_pDevice->CreateSampler(&samplerShadowPointDesc, samplerStartHandle);
	samplerStartHandle.Offset(1, m_SamplerDescriptorSize);

	m_pDevice->CreateSampler(&samplerShadowLinearDesc, samplerStartHandle);
	samplerStartHandle.Offset(1, m_SamplerDescriptorSize);

	m_pDevice->CreateSampler(&samplerShadowCompareDesc, samplerStartHandle);
	samplerStartHandle.Offset(1, m_SamplerDescriptorSize);

	m_pDevice->CreateSampler(&samplerLinearMirrorDesc, samplerStartHandle);
	samplerStartHandle.Offset(1, m_SamplerDescriptorSize);
}

void ResourceManager::initRasterizerStateDescs()
{
	ZeroMemory(&m_RasterizerSolidDesc, sizeof(m_RasterizerSolidDesc));
	ZeroMemory(&m_RasterizerSolidCcwDesc, sizeof(m_RasterizerSolidCcwDesc));
	ZeroMemory(&m_RasterizerPostProcessDesc, sizeof(m_RasterizerPostProcessDesc));
	ZeroMemory(&m_RasterizerWireDesc, sizeof(m_RasterizerWireDesc));

	m_RasterizerSolidDesc.FillMode = D3D12_FILL_MODE_SOLID;
	m_RasterizerSolidDesc.CullMode = D3D12_CULL_MODE_BACK;
	m_RasterizerSolidDesc.FrontCounterClockwise = FALSE;
	m_RasterizerSolidDesc.DepthClipEnable = TRUE;
	m_RasterizerSolidDesc.MultisampleEnable = FALSE;

	m_RasterizerSolidCcwDesc.FillMode = D3D12_FILL_MODE_SOLID;
	m_RasterizerSolidCcwDesc.CullMode = D3D12_CULL_MODE_BACK;
	m_RasterizerSolidCcwDesc.FrontCounterClockwise = TRUE;
	m_RasterizerSolidCcwDesc.DepthClipEnable = TRUE;
	m_RasterizerSolidCcwDesc.MultisampleEnable = FALSE;

	m_RasterizerPostProcessDesc.FillMode = D3D12_FILL_MODE_SOLID;
	m_RasterizerPostProcessDesc.CullMode = D3D12_CULL_MODE_NONE;
	m_RasterizerPostProcessDesc.FrontCounterClockwise = FALSE;
	m_RasterizerPostProcessDesc.DepthClipEnable = FALSE;
	m_RasterizerPostProcessDesc.MultisampleEnable = FALSE;

	m_RasterizerWireDesc.FillMode = D3D12_FILL_MODE_WIREFRAME;
	m_RasterizerWireDesc.CullMode = D3D12_CULL_MODE_BACK;
	m_RasterizerWireDesc.FrontCounterClockwise = FALSE;
	m_RasterizerWireDesc.DepthClipEnable = TRUE;
	m_RasterizerWireDesc.MultisampleEnable = FALSE;
}

void ResourceManager::initBlendStateDescs()
{
	ZeroMemory(&m_BlendMirrorDesc, sizeof(m_BlendMirrorDesc));
	ZeroMemory(&m_BlendAccumulateDesc, sizeof(m_BlendAccumulateDesc));
	ZeroMemory(&m_BlendAlphaDesc, sizeof(m_BlendAlphaDesc));

	m_BlendMirrorDesc.AlphaToCoverageEnable = TRUE;
	m_BlendMirrorDesc.IndependentBlendEnable = FALSE;
	m_BlendMirrorDesc.RenderTarget[0].BlendEnable = TRUE;
	m_BlendMirrorDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_BLEND_FACTOR;
	m_BlendMirrorDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_BLEND_FACTOR;
	m_BlendMirrorDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	m_BlendMirrorDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	m_BlendMirrorDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
	m_BlendMirrorDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	m_BlendMirrorDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	m_BlendAccumulateDesc.AlphaToCoverageEnable = TRUE;
	m_BlendAccumulateDesc.IndependentBlendEnable = FALSE;
	m_BlendAccumulateDesc.RenderTarget[0].BlendEnable = TRUE;
	m_BlendAccumulateDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_BLEND_FACTOR;
	m_BlendAccumulateDesc.RenderTarget[0].DestBlend = D3D12_BLEND_BLEND_FACTOR;
	m_BlendAccumulateDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	m_BlendAccumulateDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	m_BlendAccumulateDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
	m_BlendAccumulateDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	m_BlendAccumulateDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	m_BlendAlphaDesc.AlphaToCoverageEnable = FALSE;
	m_BlendAlphaDesc.IndependentBlendEnable = FALSE;
	m_BlendAlphaDesc.RenderTarget[0].BlendEnable = TRUE;
	m_BlendAlphaDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	m_BlendAlphaDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	m_BlendAlphaDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	m_BlendAlphaDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	m_BlendAlphaDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
	m_BlendAlphaDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	m_BlendAlphaDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
}

void ResourceManager::initDepthStencilStateDescs()
{
	ZeroMemory(&m_DepthStencilDrawDesc, sizeof(m_DepthStencilDrawDesc));
	ZeroMemory(&m_DepthStencilMaskDesc, sizeof(m_DepthStencilMaskDesc));
	ZeroMemory(&m_DepthStencilDrawMaskedDesc, sizeof(m_DepthStencilDrawMaskedDesc));

	m_DepthStencilDrawDesc.DepthEnable = TRUE;
	m_DepthStencilDrawDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	m_DepthStencilDrawDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	m_DepthStencilDrawDesc.StencilEnable = FALSE;
	m_DepthStencilDrawDesc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	m_DepthStencilDrawDesc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	m_DepthStencilDrawDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	m_DepthStencilDrawDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	m_DepthStencilDrawDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	m_DepthStencilDrawDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	m_DepthStencilDrawDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	m_DepthStencilDrawDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	m_DepthStencilDrawDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
	m_DepthStencilDrawDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	m_DepthStencilMaskDesc.DepthEnable = TRUE;
	m_DepthStencilMaskDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	m_DepthStencilMaskDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	m_DepthStencilMaskDesc.StencilEnable = TRUE;
	m_DepthStencilMaskDesc.StencilReadMask = 0xff;
	m_DepthStencilMaskDesc.StencilWriteMask = 0xff;
	m_DepthStencilMaskDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	m_DepthStencilMaskDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	m_DepthStencilMaskDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
	m_DepthStencilMaskDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	m_DepthStencilMaskDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	m_DepthStencilMaskDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	m_DepthStencilMaskDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
	m_DepthStencilMaskDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	m_DepthStencilDrawMaskedDesc.DepthEnable = TRUE;
	m_DepthStencilDrawMaskedDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	m_DepthStencilDrawMaskedDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	m_DepthStencilDrawMaskedDesc.StencilEnable = TRUE;
	m_DepthStencilDrawMaskedDesc.StencilReadMask = 0xff;
	m_DepthStencilDrawMaskedDesc.StencilWriteMask = 0xff;
	m_DepthStencilDrawMaskedDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	m_DepthStencilDrawMaskedDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	m_DepthStencilDrawMaskedDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	m_DepthStencilDrawMaskedDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;
	m_DepthStencilDrawMaskedDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	m_DepthStencilDrawMaskedDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	m_DepthStencilDrawMaskedDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
	m_DepthStencilDrawMaskedDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
}

void ResourceManager::initPipelineStates()
{
	_ASSERT(m_pDevice);

	HRESULT hr = S_OK;

	CD3DX12_DESCRIPTOR_RANGE commonResourceRanges[5];
	commonResourceRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0); // b0
	commonResourceRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1); // b1
	commonResourceRanges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 8); // t8 ~ t12
	commonResourceRanges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 13); // t13 ~ t16
	commonResourceRanges[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 8, 0); // s0 ~ s7

	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
	ID3DBlob* pSignature = nullptr;
	ID3DBlob* pError = nullptr;

	{
		CD3DX12_DESCRIPTOR_RANGE perDefaultObjectResourceRanges[3];
		perDefaultObjectResourceRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 2, 2); // b2, b3
		perDefaultObjectResourceRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 6, 0); // t0 ~ t5
		perDefaultObjectResourceRanges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 6); // t6

		CD3DX12_ROOT_PARAMETER rootParameters[3];
		rootParameters[0].InitAsDescriptorTable(3, perDefaultObjectResourceRanges, D3D12_SHADER_VISIBILITY_ALL);
		rootParameters[1].InitAsDescriptorTable(4, commonResourceRanges, D3D12_SHADER_VISIBILITY_ALL);
		rootParameters[2].InitAsDescriptorTable(1, &commonResourceRanges[4], D3D12_SHADER_VISIBILITY_ALL);

		rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pSignature, &pError);
		if (FAILED(hr))
		{
			if (pError)
			{
				OutputDebugStringA((char*)pError->GetBufferPointer());
				SAFE_RELEASE(pError);
			}
			__debugbreak();
		}

		hr = m_pDevice->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&m_pDefaultRootSignature));
		BREAK_IF_FAILED(hr);
		m_pDefaultRootSignature->SetName(L"DefaultRootSignature");

		SAFE_RELEASE(pSignature);
		SAFE_RELEASE(pError);
	}

	{
		CD3DX12_DESCRIPTOR_RANGE perSkinnedObjectResourceRanges[4];
		perSkinnedObjectResourceRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 7); // t7
		perSkinnedObjectResourceRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 2, 2); // b2, b3
		perSkinnedObjectResourceRanges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 6, 0); // t0 ~ t5
		perSkinnedObjectResourceRanges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 6); // t6

		CD3DX12_ROOT_PARAMETER rootParameter[3];
		rootParameter[0].InitAsDescriptorTable(4, perSkinnedObjectResourceRanges, D3D12_SHADER_VISIBILITY_ALL);
		rootParameter[1].InitAsDescriptorTable(4, commonResourceRanges, D3D12_SHADER_VISIBILITY_ALL);
		rootParameter[2].InitAsDescriptorTable(1, &commonResourceRanges[4], D3D12_SHADER_VISIBILITY_ALL);

		rootSignatureDesc.Init(_countof(rootParameter), rootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pSignature, &pError);
		if (FAILED(hr))
		{
			if (pError)
			{
				OutputDebugStringA((char*)pError->GetBufferPointer());
				SAFE_RELEASE(pError);
			}
			__debugbreak();
		}

		hr = m_pDevice->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&m_pSkinnedRootSignature));
		BREAK_IF_FAILED(hr);
		m_pSkinnedRootSignature->SetName(L"SkinnedRootSignature");

		SAFE_RELEASE(pSignature);
		SAFE_RELEASE(pError);
	}

	{
		CD3DX12_DESCRIPTOR_RANGE perDefaultObjectResourceRange[1];
		perDefaultObjectResourceRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 2, 2); // b2, b3

		CD3DX12_ROOT_PARAMETER rootParameters[4];
		rootParameters[0].InitAsDescriptorTable(1, perDefaultObjectResourceRange, D3D12_SHADER_VISIBILITY_ALL);
		rootParameters[1].InitAsConstantBufferView(0, D3D12_SHADER_VISIBILITY_ALL);
		rootParameters[2].InitAsDescriptorTable(3, commonResourceRanges + 1, D3D12_SHADER_VISIBILITY_ALL);
		rootParameters[3].InitAsDescriptorTable(1, &commonResourceRanges[4], D3D12_SHADER_VISIBILITY_ALL);

		rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pSignature, &pError);
		if (FAILED(hr))
		{
			if (pError)
			{
				OutputDebugStringA((char*)pError->GetBufferPointer());
				SAFE_RELEASE(pError);
			}
			__debugbreak();
		}

		hr = m_pDevice->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&m_pDepthOnlyRootSignature));
		BREAK_IF_FAILED(hr);
		m_pDepthOnlyRootSignature->SetName(L"DepthOnlyRootSignature");

		SAFE_RELEASE(pSignature);
		SAFE_RELEASE(pError);
	}

	{
		CD3DX12_DESCRIPTOR_RANGE perSkinnedObjectResourceRanges[2];
		perSkinnedObjectResourceRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 7); // t7
		perSkinnedObjectResourceRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 2, 2); // b2, b3
		
		CD3DX12_ROOT_PARAMETER rootParameters[4];
		rootParameters[0].InitAsDescriptorTable(2, perSkinnedObjectResourceRanges, D3D12_SHADER_VISIBILITY_ALL);
		rootParameters[1].InitAsConstantBufferView(0, D3D12_SHADER_VISIBILITY_ALL);
		rootParameters[2].InitAsDescriptorTable(3, commonResourceRanges + 1, D3D12_SHADER_VISIBILITY_ALL);
		rootParameters[3].InitAsDescriptorTable(1, &commonResourceRanges[4], D3D12_SHADER_VISIBILITY_ALL);

		rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pSignature, &pError);
		if (FAILED(hr))
		{
			if (pError)
			{
				OutputDebugStringA((char*)pError->GetBufferPointer());
				SAFE_RELEASE(pError);
			}
			__debugbreak();
		}

		hr = m_pDevice->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&m_pDepthOnlySkinnedRootSignature));
		BREAK_IF_FAILED(hr);
		m_pDepthOnlySkinnedRootSignature->SetName(L"DepthOnlySkinnedRootSignature");

		SAFE_RELEASE(pSignature);
		SAFE_RELEASE(pError);
	}

	{
		CD3DX12_DESCRIPTOR_RANGE perDefaultObjectResourceRange[1];
		perDefaultObjectResourceRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 2, 2); // b2, b3

		CD3DX12_ROOT_PARAMETER rootParameters[4];
		rootParameters[0].InitAsDescriptorTable(1, perDefaultObjectResourceRange, D3D12_SHADER_VISIBILITY_ALL);
		rootParameters[1].InitAsConstantBufferView(4, 0, D3D12_SHADER_VISIBILITY_GEOMETRY);
		rootParameters[2].InitAsDescriptorTable(4, commonResourceRanges, D3D12_SHADER_VISIBILITY_ALL);
		rootParameters[3].InitAsDescriptorTable(1, &commonResourceRanges[4], D3D12_SHADER_VISIBILITY_ALL);

		rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pSignature, &pError);
		if (FAILED(hr))
		{
			if (pError)
			{
				OutputDebugStringA((char*)pError->GetBufferPointer());
				SAFE_RELEASE(pError);
			}
			__debugbreak();
		}

		hr = m_pDevice->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&m_pDepthOnlyAroundRootSignature));
		BREAK_IF_FAILED(hr);
		m_pDepthOnlyAroundRootSignature->SetName(L"DepthOnlyAroundRootSignature");

		SAFE_RELEASE(pSignature);
		SAFE_RELEASE(pError);
	}

	{
		CD3DX12_DESCRIPTOR_RANGE perSkinnedObjectResourceRanges[2];
		perSkinnedObjectResourceRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 7); // t7
		perSkinnedObjectResourceRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 2, 2); // b2, b3

		CD3DX12_ROOT_PARAMETER rootParameters[4];
		rootParameters[0].InitAsDescriptorTable(2, perSkinnedObjectResourceRanges, D3D12_SHADER_VISIBILITY_ALL);
		rootParameters[1].InitAsConstantBufferView(4, 0, D3D12_SHADER_VISIBILITY_GEOMETRY);
		rootParameters[2].InitAsDescriptorTable(4, commonResourceRanges, D3D12_SHADER_VISIBILITY_ALL);
		rootParameters[3].InitAsDescriptorTable(1, &commonResourceRanges[4], D3D12_SHADER_VISIBILITY_ALL);

		rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pSignature, &pError);
		if (FAILED(hr))
		{
			if (pError)
			{
				OutputDebugStringA((char*)pError->GetBufferPointer());
				SAFE_RELEASE(pError);
			}
			__debugbreak();
		}

		hr = m_pDevice->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&m_pDepthOnlyAroundSkinnedRootSignature));
		BREAK_IF_FAILED(hr);
		m_pDepthOnlyAroundSkinnedRootSignature->SetName(L"DepthOnlyAroundSkinnedRootSignature");

		SAFE_RELEASE(pSignature);
		SAFE_RELEASE(pError);
	}


	{
		CD3DX12_DESCRIPTOR_RANGE perPSResourceRange[2];
		perPSResourceRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
		perPSResourceRange[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 4);

		CD3DX12_ROOT_PARAMETER rootParameters[3];
		rootParameters[0].InitAsDescriptorTable(2, perPSResourceRange, D3D12_SHADER_VISIBILITY_ALL);
		rootParameters[1].InitAsDescriptorTable(4, commonResourceRanges, D3D12_SHADER_VISIBILITY_ALL);
		rootParameters[2].InitAsDescriptorTable(1, &commonResourceRanges[4], D3D12_SHADER_VISIBILITY_ALL);

		rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pSignature, &pError);
		if (FAILED(hr))
		{
			if (pError)
			{
				OutputDebugStringA((char*)pError->GetBufferPointer());
				SAFE_RELEASE(pError);
			}
			__debugbreak();
		}

		hr = m_pDevice->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&m_pSamplingRootSignature));
		BREAK_IF_FAILED(hr);
		m_pSamplingRootSignature->SetName(L"BloomRootSignature");

		SAFE_RELEASE(pSignature);
		SAFE_RELEASE(pError);
	}


	{
		CD3DX12_DESCRIPTOR_RANGE perPSResourceRange[2];
		perPSResourceRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0);
		perPSResourceRange[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 4);

		CD3DX12_ROOT_PARAMETER rootParameters[3];
		rootParameters[0].InitAsDescriptorTable(2, perPSResourceRange, D3D12_SHADER_VISIBILITY_ALL);
		rootParameters[1].InitAsDescriptorTable(4, commonResourceRanges, D3D12_SHADER_VISIBILITY_ALL);
		rootParameters[2].InitAsDescriptorTable(1, &commonResourceRanges[4], D3D12_SHADER_VISIBILITY_ALL);

		rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pSignature, &pError);
		if (FAILED(hr))
		{
			if (pError)
			{
				OutputDebugStringA((char*)pError->GetBufferPointer());
				SAFE_RELEASE(pError);
			}
			__debugbreak();
		}

		hr = m_pDevice->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&m_pCombineRootSignature));
		BREAK_IF_FAILED(hr);
		m_pCombineRootSignature->SetName(L"CombineRootSignature");

		SAFE_RELEASE(pSignature);
		SAFE_RELEASE(pError);
	}

	{
		CD3DX12_DESCRIPTOR_RANGE perDefaultObjectResourceRanges[2];
		perDefaultObjectResourceRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 2, 2); // b2, b3
		perDefaultObjectResourceRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 6); // t6. 여기서는 그냥 없는 걸로 놔둠.

		CD3DX12_ROOT_PARAMETER rootParameters[3];
		rootParameters[0].InitAsDescriptorTable(2, perDefaultObjectResourceRanges, D3D12_SHADER_VISIBILITY_ALL);
		rootParameters[1].InitAsDescriptorTable(4, commonResourceRanges, D3D12_SHADER_VISIBILITY_ALL);
		rootParameters[2].InitAsDescriptorTable(1, &commonResourceRanges[4], D3D12_SHADER_VISIBILITY_ALL);

		rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pSignature, &pError);
		if (FAILED(hr))
		{
			if (pError)
			{
				OutputDebugStringA((char*)pError->GetBufferPointer());
				SAFE_RELEASE(pError);
			}
			__debugbreak();
		}

		hr = m_pDevice->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&m_pDefaultWireRootSignature));
		BREAK_IF_FAILED(hr);
		m_pDefaultWireRootSignature->SetName(L"DefaultWireRootSignature");

		SAFE_RELEASE(pSignature);
		SAFE_RELEASE(pError);
	}


	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = { 0, };
	psoDesc.pRootSignature = m_pDefaultRootSignature;
	psoDesc.VS = { (BYTE*)m_pBasicVS->GetBufferPointer(), m_pBasicVS->GetBufferSize() };
	psoDesc.PS = { (BYTE*)m_pBasicPS->GetBufferPointer(), m_pBasicPS->GetBufferSize() };
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.RasterizerState = m_RasterizerSolidDesc;
	psoDesc.DepthStencilState = m_DepthStencilDrawDesc;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.InputLayout = { m_InputLayoutBasicDescs, _countof(m_InputLayoutBasicDescs) };

	hr = m_pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pDefaultSolidPSO));
	BREAK_IF_FAILED(hr);
	m_pDefaultSolidPSO->SetName(L"DefaultSolidPSO");


	psoDesc.pRootSignature = m_pSkinnedRootSignature;
	psoDesc.VS = { (BYTE*)m_pSkinnedVS->GetBufferPointer(), m_pSkinnedVS->GetBufferSize() };
	psoDesc.PS = { (BYTE*)m_pBasicPS->GetBufferPointer(), m_pBasicPS->GetBufferSize() };
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.RasterizerState = m_RasterizerSolidDesc;
	psoDesc.DepthStencilState = m_DepthStencilDrawDesc;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.InputLayout = { m_InputLayoutSkinnedDescs, _countof(m_InputLayoutSkinnedDescs) };

	hr = m_pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pSkinnedSolidPSO));
	BREAK_IF_FAILED(hr);
	m_pSkinnedSolidPSO->SetName(L"SkinnedSolidPSO");


	psoDesc.pRootSignature = m_pDefaultRootSignature;
	psoDesc.VS = { (BYTE*)m_pSkyboxVS->GetBufferPointer(), m_pSkyboxVS->GetBufferSize() };
	psoDesc.PS = { (BYTE*)m_pSkyboxPS->GetBufferPointer(), m_pSkyboxPS->GetBufferSize() };
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.RasterizerState = m_RasterizerSolidDesc;
	psoDesc.DepthStencilState = m_DepthStencilDrawDesc;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.InputLayout = { m_InputLayoutSkyboxDescs, _countof(m_InputLayoutSkyboxDescs) };

	hr = m_pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pSkyboxSolidPSO));
	BREAK_IF_FAILED(hr);
	m_pSkyboxSolidPSO->SetName(L"SkyboxSolidPSO");


	psoDesc.pRootSignature = m_pDepthOnlyRootSignature;
	psoDesc.VS = { (BYTE*)m_pDepthOnlyVS->GetBufferPointer(), m_pDepthOnlyVS->GetBufferSize() };
	psoDesc.PS = { (BYTE*)m_pDepthOnlyPS->GetBufferPointer(), m_pDepthOnlyPS->GetBufferSize() };
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.RasterizerState = m_RasterizerSolidDesc;
	psoDesc.DepthStencilState = m_DepthStencilMaskDesc;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.InputLayout = { m_InputLayoutBasicDescs, _countof(m_InputLayoutBasicDescs) };

	hr = m_pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pStencilMaskPSO));
	BREAK_IF_FAILED(hr);
	m_pStencilMaskPSO->SetName(L"StencilMaskPSO");


	psoDesc.pRootSignature = m_pDefaultRootSignature;
	psoDesc.VS = { (BYTE*)m_pBasicVS->GetBufferPointer(), m_pBasicVS->GetBufferSize() };
	psoDesc.PS = { (BYTE*)m_pBasicPS->GetBufferPointer(), m_pBasicPS->GetBufferSize() };
	psoDesc.BlendState = m_BlendMirrorDesc;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.RasterizerState = m_RasterizerSolidDesc;
	psoDesc.DepthStencilState = m_DepthStencilDrawMaskedDesc;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.InputLayout = { m_InputLayoutBasicDescs, _countof(m_InputLayoutBasicDescs) };

	hr = m_pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pMirrorBlendSolidPSO));
	BREAK_IF_FAILED(hr);
	m_pMirrorBlendSolidPSO->SetName(L"MirrorBlendSolidPSO");


	psoDesc.pRootSignature = m_pDefaultRootSignature;
	psoDesc.VS = { (BYTE*)m_pBasicVS->GetBufferPointer(), m_pBasicVS->GetBufferSize() };
	psoDesc.PS = { (BYTE*)m_pBasicPS->GetBufferPointer(), m_pBasicPS->GetBufferSize() };
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.RasterizerState = m_RasterizerSolidCcwDesc;
	psoDesc.DepthStencilState = m_DepthStencilDrawMaskedDesc;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.InputLayout = { m_InputLayoutBasicDescs, _countof(m_InputLayoutBasicDescs) };

	hr = m_pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pReflectDefaultSolidPSO));
	BREAK_IF_FAILED(hr);
	m_pReflectDefaultSolidPSO->SetName(L"ReflectDefaultSolidPSO");


	psoDesc.pRootSignature = m_pSkinnedRootSignature;
	psoDesc.VS = { (BYTE*)m_pSkinnedVS->GetBufferPointer(), m_pSkinnedVS->GetBufferSize() };
	psoDesc.PS = { (BYTE*)m_pBasicPS->GetBufferPointer(), m_pBasicPS->GetBufferSize() };
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.RasterizerState = m_RasterizerSolidCcwDesc;
	psoDesc.DepthStencilState = m_DepthStencilDrawMaskedDesc;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.InputLayout = { m_InputLayoutSkinnedDescs, _countof(m_InputLayoutSkinnedDescs) };

	hr = m_pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pReflectSkinnedSolidPSO));
	BREAK_IF_FAILED(hr);
	m_pReflectSkinnedSolidPSO->SetName(L"ReflectSkinnedSolidPSO");


	psoDesc.pRootSignature = m_pDefaultRootSignature;
	psoDesc.VS = { (BYTE*)m_pSkyboxVS->GetBufferPointer(), m_pSkyboxVS->GetBufferSize() };
	psoDesc.PS = { (BYTE*)m_pSkyboxPS->GetBufferPointer(), m_pSkyboxPS->GetBufferSize() };
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.RasterizerState = m_RasterizerSolidCcwDesc;
	psoDesc.DepthStencilState = m_DepthStencilDrawMaskedDesc;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.InputLayout = { m_InputLayoutSkyboxDescs, _countof(m_InputLayoutSkyboxDescs) };
	
	hr = m_pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pReflectSkyboxSolidPSO));
	BREAK_IF_FAILED(hr);
	m_pReflectSkyboxSolidPSO->SetName(L"ReflectSkyboxSolidPSO");


	psoDesc.pRootSignature = m_pDepthOnlyRootSignature;
	psoDesc.VS = { (BYTE*)m_pDepthOnlyVS->GetBufferPointer(), m_pDepthOnlyVS->GetBufferSize() };
	psoDesc.PS = { (BYTE*)m_pDepthOnlyPS->GetBufferPointer(), m_pDepthOnlyPS->GetBufferSize() };
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.RasterizerState = m_RasterizerSolidDesc;
	psoDesc.DepthStencilState = m_DepthStencilDrawDesc;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	// psoDesc.RTVFormats[0] = DXGI_FORMAT_R10G10B10A2_UNORM;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.InputLayout = { m_InputLayoutBasicDescs, _countof(m_InputLayoutBasicDescs) };
	
	hr = m_pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pDepthOnlyPSO));
	BREAK_IF_FAILED(hr);
	m_pDepthOnlyPSO->SetName(L"DepthOnlyPSO");


	psoDesc.pRootSignature = m_pDepthOnlySkinnedRootSignature;
	psoDesc.VS = { (BYTE*)m_pDepthOnlySkinnedVS->GetBufferPointer(), m_pDepthOnlySkinnedVS->GetBufferSize() };
	psoDesc.PS = { (BYTE*)m_pDepthOnlyPS->GetBufferPointer(), m_pDepthOnlyPS->GetBufferSize() };
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.RasterizerState = m_RasterizerSolidDesc;
	psoDesc.DepthStencilState = m_DepthStencilDrawDesc;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	// psoDesc.RTVFormats[0] = DXGI_FORMAT_R10G10B10A2_UNORM;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.InputLayout = { m_InputLayoutSkinnedDescs, _countof(m_InputLayoutSkinnedDescs) };

	hr = m_pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pDepthOnlySkinnedPSO));
	BREAK_IF_FAILED(hr);
	m_pDepthOnlySkinnedPSO->SetName(L"DepthOnlySkinnedPSO");


	psoDesc.pRootSignature = m_pDepthOnlyAroundRootSignature;
	psoDesc.VS = { (BYTE*)m_pDepthOnlyCubeVS->GetBufferPointer(), m_pDepthOnlyCubeVS->GetBufferSize() };
	// psoDesc.VS = { (BYTE*)m_pDepthOnlyVS->GetBufferPointer(), m_pDepthOnlyVS->GetBufferSize() };
	psoDesc.PS = { (BYTE*)m_pDepthOnlyCubePS->GetBufferPointer(), m_pDepthOnlyCubePS->GetBufferSize() };
	psoDesc.GS = { (BYTE*)m_pDepthOnlyCubeGS->GetBufferPointer(), m_pDepthOnlyCubeGS->GetBufferSize() };
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.RasterizerState = m_RasterizerSolidDesc;
	psoDesc.DepthStencilState = m_DepthStencilDrawDesc;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	// psoDesc.RTVFormats[0] = DXGI_FORMAT_R10G10B10A2_UNORM;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.InputLayout = { m_InputLayoutBasicDescs, _countof(m_InputLayoutBasicDescs) };

	hr = m_pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pDepthOnlyCubePSO));
	BREAK_IF_FAILED(hr);
	m_pDepthOnlyCubePSO->SetName(L"DepthOnlyCubePSO");


	psoDesc.pRootSignature = m_pDepthOnlyAroundSkinnedRootSignature;
	psoDesc.VS = { (BYTE*)m_pDepthOnlyCubeSkinnedVS->GetBufferPointer(), m_pDepthOnlyCubeSkinnedVS->GetBufferSize() };
	// psoDesc.VS = { (BYTE*)m_pDepthOnlySkinnedVS->GetBufferPointer(), m_pDepthOnlySkinnedVS->GetBufferSize() };
	psoDesc.PS = { (BYTE*)m_pDepthOnlyCubePS->GetBufferPointer(), m_pDepthOnlyCubePS->GetBufferSize() };
	psoDesc.GS = { (BYTE*)m_pDepthOnlyCubeGS->GetBufferPointer(), m_pDepthOnlyCubeGS->GetBufferSize() };
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.RasterizerState = m_RasterizerSolidDesc;
	psoDesc.DepthStencilState = m_DepthStencilDrawDesc;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	// psoDesc.RTVFormats[0] = DXGI_FORMAT_R10G10B10A2_UNORM;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.InputLayout = { m_InputLayoutSkinnedDescs, _countof(m_InputLayoutSkinnedDescs) };

	hr = m_pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pDepthOnlyCubeSkinnedPSO));
	BREAK_IF_FAILED(hr);
	m_pDepthOnlyCubeSkinnedPSO->SetName(L"DepthOnlyCubeSkinnedPSO");


	psoDesc.pRootSignature = m_pDepthOnlyAroundRootSignature;
	psoDesc.VS = { (BYTE*)m_pDepthOnlyCascadeVS->GetBufferPointer(), m_pDepthOnlyCascadeVS->GetBufferSize() };
	// psoDesc.VS = { (BYTE*)m_pDepthOnlyVS->GetBufferPointer(), m_pDepthOnlyVS->GetBufferSize() };
	psoDesc.PS = { (BYTE*)m_pDepthOnlyCascadePS->GetBufferPointer(), m_pDepthOnlyCascadePS->GetBufferSize() };
	psoDesc.GS = { (BYTE*)m_pDepthOnlyCascadeGS->GetBufferPointer(), m_pDepthOnlyCascadeGS->GetBufferSize() };
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.RasterizerState = m_RasterizerSolidDesc;
	psoDesc.DepthStencilState = m_DepthStencilDrawDesc;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	// psoDesc.RTVFormats[0] = DXGI_FORMAT_R10G10B10A2_UNORM;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.InputLayout = { m_InputLayoutBasicDescs, _countof(m_InputLayoutBasicDescs) };

	hr = m_pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pDepthOnlyCascadePSO));
	BREAK_IF_FAILED(hr);
	m_pDepthOnlyCascadePSO->SetName(L"DepthOnlyCascadePSO");


	psoDesc.pRootSignature = m_pDepthOnlyAroundSkinnedRootSignature;
	psoDesc.VS = { (BYTE*)m_pDepthOnlyCascadeSkinnedVS->GetBufferPointer(), m_pDepthOnlyCascadeSkinnedVS->GetBufferSize() };
	// psoDesc.VS = { (BYTE*)m_pDepthOnlySkinnedVS->GetBufferPointer(), m_pDepthOnlySkinnedVS->GetBufferSize() };
	psoDesc.PS = { (BYTE*)m_pDepthOnlyCascadePS->GetBufferPointer(), m_pDepthOnlyCascadePS->GetBufferSize() };
	psoDesc.GS = { (BYTE*)m_pDepthOnlyCascadeGS->GetBufferPointer(), m_pDepthOnlyCascadeGS->GetBufferSize() };
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.RasterizerState = m_RasterizerSolidDesc;
	psoDesc.DepthStencilState = m_DepthStencilDrawDesc;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	// psoDesc.RTVFormats[0] = DXGI_FORMAT_R10G10B10A2_UNORM;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.InputLayout = { m_InputLayoutSkinnedDescs, _countof(m_InputLayoutSkinnedDescs) };

	hr = m_pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pDepthOnlyCascadeSkinnedPSO));
	BREAK_IF_FAILED(hr);
	m_pDepthOnlyCascadeSkinnedPSO->SetName(L"DepthOnlyCascadeSkinnedPSO");


	psoDesc.pRootSignature = m_pSamplingRootSignature;
	psoDesc.VS = { (BYTE*)m_pSamplingVS->GetBufferPointer(), m_pSamplingVS->GetBufferSize() };
	psoDesc.PS = { (BYTE*)m_pSamplingPS->GetBufferPointer(), m_pSamplingPS->GetBufferSize() };
	psoDesc.GS = { 0, };
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.RasterizerState = m_RasterizerPostProcessDesc;
	psoDesc.DepthStencilState.DepthEnable = FALSE;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.InputLayout = { m_InputLayoutSamplingDescs, _countof(m_InputLayoutSamplingDescs) };

	hr = m_pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pSamplingPSO));
	BREAK_IF_FAILED(hr);
	m_pSamplingPSO->SetName(L"SamplingPSO");


	psoDesc.pRootSignature = m_pSamplingRootSignature;
	psoDesc.VS = { (BYTE*)m_pSamplingVS->GetBufferPointer(), m_pSamplingVS->GetBufferSize() };
	psoDesc.PS = { (BYTE*)m_pBloomDownPS->GetBufferPointer(), m_pBloomDownPS->GetBufferSize() };
	psoDesc.GS = { 0, };
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.RasterizerState = m_RasterizerPostProcessDesc;
	psoDesc.DepthStencilState.DepthEnable = FALSE;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.InputLayout = { m_InputLayoutSamplingDescs, _countof(m_InputLayoutSamplingDescs) };

	hr = m_pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pBloomDownPSO));
	BREAK_IF_FAILED(hr);
	m_pBloomDownPSO->SetName(L"BloomDownPSO");


	psoDesc.pRootSignature = m_pSamplingRootSignature;
	psoDesc.VS = { (BYTE*)m_pSamplingVS->GetBufferPointer(), m_pSamplingVS->GetBufferSize() };
	psoDesc.PS = { (BYTE*)m_pBloomUpPS->GetBufferPointer(), m_pBloomUpPS->GetBufferSize() };
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.RasterizerState = m_RasterizerPostProcessDesc;
	psoDesc.DepthStencilState.DepthEnable = FALSE;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.InputLayout = { m_InputLayoutSamplingDescs, _countof(m_InputLayoutSamplingDescs) };

	hr = m_pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pBloomUpPSO));
	BREAK_IF_FAILED(hr);
	m_pBloomUpPSO->SetName(L"BloomUpPSO");


	psoDesc.pRootSignature = m_pCombineRootSignature;
	psoDesc.VS = { (BYTE*)m_pSamplingVS->GetBufferPointer(), m_pSamplingVS->GetBufferSize() };
	psoDesc.PS = { (BYTE*)m_pCombinePS->GetBufferPointer(), m_pCombinePS->GetBufferSize() };
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.RasterizerState = m_RasterizerPostProcessDesc;
	psoDesc.DepthStencilState.DepthEnable = FALSE;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	// psoDesc.RTVFormats[0] = DXGI_FORMAT_R10G10B10A2_UNORM;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.InputLayout = { m_InputLayoutSamplingDescs, _countof(m_InputLayoutSamplingDescs) };

	hr = m_pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pCombinePSO));
	BREAK_IF_FAILED(hr);
	m_pCombinePSO->SetName(L"CombinePSO");


	psoDesc.pRootSignature = m_pDefaultWireRootSignature;
	psoDesc.VS = { (BYTE*)m_pBasicVS->GetBufferPointer(), m_pBasicVS->GetBufferSize() };
	psoDesc.PS = { (BYTE*)m_pColorPS->GetBufferPointer(), m_pColorPS->GetBufferSize() };
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.RasterizerState = m_RasterizerWireDesc;
	psoDesc.DepthStencilState.DepthEnable = FALSE;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.InputLayout = { m_InputLayoutBasicDescs, _countof(m_InputLayoutBasicDescs) };

	hr = m_pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pDefaultWirePSO));
	BREAK_IF_FAILED(hr);
	m_pDefaultWirePSO->SetName(L"DefaultWirePSO");
}

void ResourceManager::initShaders()
{
	HRESULT hr = S_OK;

	D3D12_INPUT_ELEMENT_DESC basicDescs[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
	D3D12_INPUT_ELEMENT_DESC skinncedDescs[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"BLENDWEIGHT", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 60, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"BLENDINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, 76, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"BLENDINDICES", 1, DXGI_FORMAT_R8G8B8A8_UINT, 0, 80, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
	D3D12_INPUT_ELEMENT_DESC skyboxDescs[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
	D3D12_INPUT_ELEMENT_DESC samplingDescs[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
	const D3D_SHADER_MACRO pSKINNED_MACRO[] =
	{
		{"SKINNED", "1"}, { NULL, NULL }
	};
	memcpy(m_InputLayoutBasicDescs, basicDescs, sizeof(basicDescs));
	memcpy(m_InputLayoutSkinnedDescs, skinncedDescs, sizeof(skinncedDescs));
	memcpy(m_InputLayoutSkyboxDescs, skyboxDescs, sizeof(skyboxDescs));
	memcpy(m_InputLayoutSamplingDescs, samplingDescs, sizeof(samplingDescs));

	hr = CompileShader(L"./Shaders/BasicVS.hlsl", "vs_5_1", nullptr, &m_pBasicVS);
	BREAK_IF_FAILED(hr);

	hr = CompileShader(L"./Shaders/BasicVS.hlsl", "vs_5_1", pSKINNED_MACRO, &m_pSkinnedVS);
	BREAK_IF_FAILED(hr);

	hr = CompileShader(L"./Shaders/SkyboxVS.hlsl", "vs_5_1", nullptr, &m_pSkyboxVS);
	BREAK_IF_FAILED(hr);

	hr = CompileShader(L"./Shaders/DepthOnlyVS.hlsl", "vs_5_1", nullptr, &m_pDepthOnlyVS);
	BREAK_IF_FAILED(hr);

	hr = CompileShader(L"./Shaders/DepthOnlyVS.hlsl", "vs_5_1", pSKINNED_MACRO, &m_pDepthOnlySkinnedVS);
	BREAK_IF_FAILED(hr);

	hr = CompileShader(L"./Shaders/DepthOnlyCubeVS.hlsl", "vs_5_1", nullptr, &m_pDepthOnlyCubeVS);
	BREAK_IF_FAILED(hr);

	hr = CompileShader(L"./Shaders/DepthOnlyCubeVS.hlsl", "vs_5_1", pSKINNED_MACRO, &m_pDepthOnlyCubeSkinnedVS);
	BREAK_IF_FAILED(hr);

	hr = CompileShader(L"./Shaders/DepthOnlyCascadeVS.hlsl", "vs_5_1", nullptr, &m_pDepthOnlyCascadeVS);
	BREAK_IF_FAILED(hr);

	hr = CompileShader(L"./Shaders/DepthOnlyCascadeVS.hlsl", "vs_5_1", pSKINNED_MACRO, &m_pDepthOnlyCascadeSkinnedVS);
	BREAK_IF_FAILED(hr);

	hr = CompileShader(L"./Shaders/SamplingVS.hlsl", "vs_5_1", nullptr, &m_pSamplingVS);
	BREAK_IF_FAILED(hr);

	hr = CompileShader(L"./Shaders/BasicPS.hlsl", "ps_5_1", nullptr, &m_pBasicPS);
	BREAK_IF_FAILED(hr);

	hr = CompileShader(L"./Shaders/SkyboxPS.hlsl", "ps_5_1", nullptr, &m_pSkyboxPS);
	BREAK_IF_FAILED(hr);

	hr = CompileShader(L"./Shaders/DepthOnlyPS.hlsl", "ps_5_1", nullptr, &m_pDepthOnlyPS);
	BREAK_IF_FAILED(hr);

	hr = CompileShader(L"./Shaders/DepthOnlyCubePS.hlsl", "ps_5_1", nullptr, &m_pDepthOnlyCubePS);
	BREAK_IF_FAILED(hr);

	hr = CompileShader(L"./Shaders/DepthOnlyCascadePS.hlsl", "ps_5_1", nullptr, &m_pDepthOnlyCascadePS);
	BREAK_IF_FAILED(hr);

	hr = CompileShader(L"./Shaders/SamplingPS.hlsl", "ps_5_1", nullptr, &m_pSamplingPS);
	BREAK_IF_FAILED(hr);

	hr = CompileShader(L"./Shaders/CombinePS.hlsl", "ps_5_1", nullptr, &m_pCombinePS);
	BREAK_IF_FAILED(hr);

	hr = CompileShader(L"./Shaders/BloomDownPS.hlsl", "ps_5_1", nullptr, &m_pBloomDownPS);
	BREAK_IF_FAILED(hr);

	hr = CompileShader(L"./Shaders/BloomUpPS.hlsl", "ps_5_1", nullptr, &m_pBloomUpPS);
	BREAK_IF_FAILED(hr);

	hr = CompileShader(L"./Shaders/ColorPS.hlsl", "ps_5_1", nullptr, &m_pColorPS);
	BREAK_IF_FAILED(hr);

	hr = CompileShader(L"./Shaders/DepthOnlyCubeGS.hlsl", "gs_5_1", nullptr, &m_pDepthOnlyCubeGS);
	BREAK_IF_FAILED(hr);

	hr = CompileShader(L"./Shaders/DepthOnlyCascadeGS.hlsl", "gs_5_1", nullptr, &m_pDepthOnlyCascadeGS);
	BREAK_IF_FAILED(hr);
}

//UINT64 ResourceManager::fence()
//{
//	++(*m_pFenceValue);
//	m_pCommandQueue->Signal(m_pFence, *m_pFenceValue);
//	m_pFenceValues[*m_pFrameIndex] = *m_pFenceValue;
//	return *m_pFenceValue;
//}
//
//void ResourceManager::waitForGPU(UINT64 expectedFenceValue)
//{
//	// Wait until the previous frame is finished.
//	if (m_pFence->GetCompletedValue() < expectedFenceValue)
//	{
//		m_pFence->SetEventOnCompletion(expectedFenceValue, m_hFenceEvent);
//		WaitForSingleObject(m_hFenceEvent, INFINITE);
//	}
//}
