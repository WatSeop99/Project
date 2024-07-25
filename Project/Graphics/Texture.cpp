#include "../pch.h"
#include "GraphicsUtil.h"
#include "../Util/Utility.h"
#include "Texture.h"

void Texture::Initialize(Renderer* pRenderer, const WCHAR* pszFileName, bool bUseSRGB)
{
	_ASSERT(pRenderer);

	Cleanup();

	HRESULT hr = S_OK;
	ResourceManager* pManager = pRenderer->GetResourceManager();
	ID3D12Device* pDevice = pManager->m_pDevice;
	ID3D12GraphicsCommandList* pCommandList = pManager->GetCommandList();
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


	D3D12_RESOURCE_DESC textureDesc = {};
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

	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProps.CreationNodeMask = 1;
	heapProps.VisibleNodeMask = 1;

	D3D12_RESOURCE_STATES curState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	m_Width = (UINT)textureDesc.Width;
	m_Height = (UINT)textureDesc.Height;
	m_Depth = (UINT)textureDesc.DepthOrArraySize;

	hr = pDevice->CreateCommittedResource(&heapProps,
										  D3D12_HEAP_FLAG_NONE,
										  &textureDesc,
										  curState,
										  nullptr,
										  IID_PPV_ARGS(&m_pResource));
	BREAK_IF_FAILED(hr);
	m_pResource->SetName(L"TextureResource");

	m_GPUMemAddr = m_pResource->GetGPUVirtualAddress();


	ID3D12Resource* pUploadResource = nullptr;
	BYTE* pMappedPtr = nullptr;
	UINT64 pixelSize = GetPixelSize(textureDesc.Format);
	UINT num2DSubresources = textureDesc.DepthOrArraySize * textureDesc.MipLevels;
	UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_pResource, 0, num2DSubresources);

	D3D12_RESOURCE_DESC Desc = m_pResource->GetDesc();
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footPrint;
	UINT rows = 0;
	UINT64 rowSize = 0;
	UINT64 totalBytes = 0;
	CD3DX12_RESOURCE_DESC uploadResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

	pDevice->GetCopyableFootprints(&Desc, 0, 1, 0, &footPrint, &rows, &rowSize, &totalBytes);
	heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
	hr = pDevice->CreateCommittedResource(&heapProps,
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
	for (UINT i = 0; i < m_Width; ++i)
	{
		memcpy(pDest, pSrc, m_Width * pixelSize);
		pSrc += (m_Width * pixelSize);
		pDest += footPrint.Footprint.RowPitch;
	}

	pUploadResource->Unmap(0, nullptr);
	pManager->UpdateTexture(m_pResource, pUploadResource, &curState);

	SAFE_RELEASE(pUploadResource);
}

void Texture::Initialize(Renderer* pRenderer, const D3D12_RESOURCE_DESC& DESC)
{
	_ASSERT(pRenderer);

	HRESULT hr = S_OK;
	ResourceManager* pManager = pRenderer->GetResourceManager();
	ID3D12Device5* pDevice = pManager->m_pDevice;

	CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

	D3D12_CLEAR_VALUE clearValue;
	clearValue.Format = DXGI_FORMAT_D32_FLOAT;
	clearValue.DepthStencil.Depth = 1.0f;
	clearValue.DepthStencil.Stencil = 0;

	hr = pDevice->CreateCommittedResource(&heapProps,
										  D3D12_HEAP_FLAG_NONE,
										  &DESC,
										  D3D12_RESOURCE_STATE_GENERIC_READ,
										  &clearValue,
										  IID_PPV_ARGS(&m_pResource));
	BREAK_IF_FAILED(hr);
	m_pResource->SetName(L"TextureDepthResource");

	m_GPUMemAddr = m_pResource->GetGPUVirtualAddress();
}

void Texture::InitializeWithDDS(Renderer* pRenderer, const WCHAR* pszFileName)
{
	_ASSERT(pRenderer);

	HRESULT hr = S_OK;
	ResourceManager* pManager = pRenderer->GetResourceManager();
	ID3D12Device5* pDevice = pManager->m_pDevice;
	ID3D12CommandQueue* pCommandQueue = pManager->m_pCommandQueue;

	hr = ReadDDSImage(pDevice, pCommandQueue, pszFileName, &m_pResource);
	BREAK_IF_FAILED(hr);
	m_pResource->SetName(L"TextureResource");
}

void Texture::Cleanup()
{
	m_Width = 0;
	m_Height = 0;
	m_Depth = 0;
	m_GPUMemAddr = 0xffffffffffffffff;
	// m_DSVHandle = { 0xffffffffffffffff, };
	// m_SRVHandle = { 0xffffffffffffffff, };
	SAFE_RELEASE(m_pResource);
}


void NonImageTexture::Initialize(Renderer* pRenderer, UINT numElement, UINT elementSize)
{
	_ASSERT(pRenderer);

	Clear();

	HRESULT hr = S_OK;
	ResourceManager* pManager = pRenderer->GetResourceManager();
	ID3D12Device5* pDevice = pManager->m_pDevice;

	ElementCount = numElement;
	ElementSize = elementSize;
	m_BufferSize = ElementSize * ElementCount;
	pData = malloc(m_BufferSize);
	ZeroMemory(pData, m_BufferSize);

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

	CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);

	hr = pDevice->CreateCommittedResource(&heapProps,
										  D3D12_HEAP_FLAG_NONE,
										  &resourceDesc,
										  D3D12_RESOURCE_STATE_GENERIC_READ,
										  nullptr,
										  IID_PPV_ARGS(&m_pResource));
	BREAK_IF_FAILED(hr);
	m_pResource->SetName(L"NonImageTextureResource");

	m_GPUMemAddr = m_pResource->GetGPUVirtualAddress();

	CD3DX12_RANGE writeRange(0, 0);
	m_pResource->Map(0, &writeRange, &m_pSysMemAddr);
}

void NonImageTexture::Upload()
{
	_ASSERT(m_pResource);
	_ASSERT(m_pSysMemAddr);

	memcpy(m_pSysMemAddr, pData, m_BufferSize);
}

void NonImageTexture::Clear()
{
	if (pData)
	{
		free(pData);
		pData = nullptr;
	}

	m_BufferSize = 0;
	ElementCount = 0;
	ElementSize = 0;

	m_GPUMemAddr = 0xffffffffffffffff;
	// m_SRVHandle = { 0xffffffffffffffff, };

	if (m_pResource)
	{
		CD3DX12_RANGE writeRange(0, 0);
		m_pResource->Unmap(0, &writeRange);
		m_pSysMemAddr = nullptr;

		m_pResource->Release();
		m_pResource = nullptr;
	}
}
