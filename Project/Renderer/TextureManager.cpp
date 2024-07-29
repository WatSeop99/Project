#include "../pch.h"
#include "TextureManager.h"

void TextureManager::SetInitializeTextureHandle(TextureHandle* pHandle)
{
	_ASSERT(pHandle);

	pHandle->pTextureResource = nullptr;
	pHandle->pUploadBuffer = nullptr;
	pHandle->RTVHandle = { 0xffffffff, };
	pHandle->DSVHandle = { 0xffffffff, };
	pHandle->SRVHandle = { 0xffffffff, };
	pHandle->GPUHandle = { 0xffffffff, };
	pHandle->pSearchHandle = nullptr;
	pHandle->Link = { nullptr, };
	pHandle->RefCount = 0;
	pHandle->bUpdated = false;
	pHandle->bFromFile = false;
}

void TextureManager::Initialize(Renderer* pRenderer, UINT maxBucketNum, UINT maxFileNum)
{
	_ASSERT(pRenderer);
	_ASSERT(maxBucketNum > 0);
	_ASSERT(maxFileNum > 0);

	m_pRenderer = pRenderer;
	m_pResourceManager = pRenderer->GetResourceManager();

	m_pHashTable = new HashTable;
	m_pHashTable->Initialize(maxBucketNum, _MAX_PATH, maxFileNum);
}

TextureHandle* TextureManager::CreateTextureFromFile(const WCHAR* pszFileName, bool bUseSRGB)
{
	_ASSERT(m_pRenderer);
	_ASSERT(m_pResourceManager);
	_ASSERT(pszFileName);

	ID3D12Device* pDevice = m_pRenderer->GetD3DDevice();
	DescriptorAllocator* pSRVDescriptorAllocator = m_pRenderer->GetSRVUAVAllocator();

	ID3D12Resource* pTextureResource = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle;
	D3D12_RESOURCE_DESC desc;
	TextureHandle* pTextureHandle = nullptr;

	UINT fileNameLen = (UINT)wcslen(pszFileName);
	UINT keySize = fileNameLen * sizeof(WCHAR);
	if (m_pHashTable->Select((void**)&pTextureHandle, 1, pszFileName, keySize))
	{
		++pTextureHandle->RefCount;
	}
	else
	{
		HRESULT hr = m_pResourceManager->CreateTextureFromFile(&pTextureResource, &desc, pszFileName, bUseSRGB);
		if (SUCCEEDED(hr))
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Format = desc.Format;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Texture2D.MipLevels = desc.MipLevels;

			if (pSRVDescriptorAllocator->AllocDescriptorHandle(&srvHandle))
			{
				pDevice->CreateShaderResourceView(pTextureResource, &srvDesc, srvHandle);

				pTextureHandle = allocTextureHandle();
				pTextureHandle->pTextureResource = pTextureResource;
				pTextureHandle->bFromFile = true;
				pTextureHandle->SRVHandle = srvHandle;
				pTextureHandle->GPUHandle = pTextureResource->GetGPUVirtualAddress();

				pTextureHandle->pSearchHandle = m_pHashTable->Insert((void*)pTextureHandle, pszFileName, keySize);
				if (!pTextureHandle->pSearchHandle)
				{
					__debugbreak();
				}

			}
			else
			{
				pTextureResource->Release();
				pTextureResource = nullptr;
			}
		}
	}
	
	return pTextureHandle;
}

TextureHandle* TextureManager::CreateTexturFromDDSFile(const WCHAR* pszFileName, bool bIsCube)
{
	_ASSERT(m_pRenderer);
	_ASSERT(m_pResourceManager);
	_ASSERT(pszFileName);

	ID3D12Device* pDevice = m_pRenderer->GetD3DDevice();
	DescriptorAllocator* pSRVDescriptorAllocator = m_pRenderer->GetSRVUAVAllocator();

	ID3D12Resource* pTextureResource = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle;
	D3D12_RESOURCE_DESC desc;
	TextureHandle* pTextureHandle = nullptr;

	UINT fileNameLen = (UINT)wcslen(pszFileName);
	UINT keySize = fileNameLen * sizeof(WCHAR);
	if (m_pHashTable->Select((void**)&pTextureHandle, 1, pszFileName, keySize))
	{
		++pTextureHandle->RefCount;
	}
	else
	{
		HRESULT hr = m_pResourceManager->CreateTextureCubeFromFile(&pTextureResource, &desc, pszFileName);
		if (SUCCEEDED(hr))
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Format = desc.Format;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			if (bIsCube)
			{
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
				srvDesc.TextureCube.MostDetailedMip = 0;
				srvDesc.TextureCube.MipLevels = desc.MipLevels;
			}
			else
			{
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				srvDesc.Texture2D.MostDetailedMip = 0;
				srvDesc.Texture2D.MipLevels = 1;
				srvDesc.Texture2D.PlaneSlice = 0;
			}

			if (pSRVDescriptorAllocator->AllocDescriptorHandle(&srvHandle))
			{
				pDevice->CreateShaderResourceView(pTextureResource, &srvDesc, srvHandle);

				pTextureHandle = allocTextureHandle();
				pTextureHandle->pTextureResource = pTextureResource;
				pTextureHandle->bFromFile = true;
				pTextureHandle->SRVHandle = srvHandle;
				pTextureHandle->GPUHandle = pTextureResource->GetGPUVirtualAddress();

				pTextureHandle->pSearchHandle = m_pHashTable->Insert((void*)pTextureHandle, pszFileName, keySize);
				if (!pTextureHandle->pSearchHandle)
				{
					__debugbreak();
				}

			}
			else
			{
				pTextureResource->Release();
				pTextureResource = nullptr;
			}
		}
	}

	return pTextureHandle;
}

TextureHandle* TextureManager::CreateDynamicTexture(UINT width, UINT height)
{
	_ASSERT(m_pRenderer);
	_ASSERT(m_pResourceManager);

	ID3D12Device* pDevice = m_pRenderer->GetD3DDevice();
	DescriptorAllocator* pSRVDescriptorAllocator = m_pRenderer->GetSRVUAVAllocator();
	TextureHandle* pTextureHandle = nullptr;

	ID3D12Resource* pTextureResource = nullptr;
	ID3D12Resource* pUploadBuffer = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle;


	DXGI_FORMAT texFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	HRESULT hr = m_pResourceManager->CreateTexturePair(&pTextureResource, &pUploadBuffer, width, height, texFormat);
	if (SUCCEEDED(hr))
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = texFormat;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MipLevels = 1;

		if (pSRVDescriptorAllocator->AllocDescriptorHandle(&srvHandle))
		{
			pDevice->CreateShaderResourceView(pTextureResource, &srvDesc, srvHandle);

			pTextureHandle = allocTextureHandle();
			pTextureHandle->pTextureResource = pTextureResource;
			pTextureHandle->pUploadBuffer = pUploadBuffer;
			pTextureHandle->SRVHandle = srvHandle;
			pTextureHandle->GPUHandle = pTextureResource->GetGPUVirtualAddress();
		}
		else
		{
			pTextureResource->Release();
			pTextureResource = nullptr;

			pUploadBuffer->Release();
			pUploadBuffer = nullptr;
		}
	}

	return pTextureHandle;
}

TextureHandle* TextureManager::CreateImmutableTexture(UINT widht, UINT height, DXGI_FORMAT format, const BYTE* pInitImage)
{
	_ASSERT(m_pRenderer);
	_ASSERT(m_pResourceManager);

	ID3D12Device* pDevice = m_pRenderer->GetD3DDevice();
	DescriptorAllocator* pSRVDescriptorAllocator = m_pRenderer->GetSRVUAVAllocator();
	TextureHandle* pTextureHandle = nullptr;

	ID3D12Resource* pTextureResource = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle;
	
	HRESULT hr = m_pResourceManager->CreateTexture(&pTextureResource, widht, height, format, pInitImage);
	if (SUCCEEDED(hr))
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MipLevels = 1;

		if (pSRVDescriptorAllocator->AllocDescriptorHandle(&srvHandle))
		{
			pDevice->CreateShaderResourceView(pTextureResource, &srvDesc, srvHandle);

			pTextureHandle = allocTextureHandle();
			pTextureHandle->pTextureResource = pTextureResource;
			pTextureHandle->SRVHandle = srvHandle;
			pTextureHandle->GPUHandle = pTextureResource->GetGPUVirtualAddress();
		}
		else
		{
			pTextureResource->Release();
			pTextureResource = nullptr;
		}
	}

	return pTextureHandle;
}

TextureHandle* TextureManager::CreateDepthStencilTexture(const D3D12_RESOURCE_DESC& desc, const D3D12_DEPTH_STENCIL_VIEW_DESC& dsvDesc, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc)
{
	_ASSERT(m_pRenderer);

	ID3D12Device* pDevice = m_pRenderer->GetD3DDevice();
	DescriptorAllocator* pDSVDescriprotAllocator = m_pRenderer->GetDSVAllocator();
	DescriptorAllocator* pSRVDescriptorAllocator = m_pRenderer->GetSRVUAVAllocator();
	TextureHandle* pTextureHandle = nullptr;

	HRESULT hr = S_OK;

	ID3D12Resource* pTextureResource = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = {};
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = {};

	CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = DXGI_FORMAT_D32_FLOAT;
	clearValue.DepthStencil.Depth = 1.0f;
	clearValue.DepthStencil.Stencil = 0;

	hr = pDevice->CreateCommittedResource(&heapProps,
										  D3D12_HEAP_FLAG_NONE,
										  &desc,
										  D3D12_RESOURCE_STATE_GENERIC_READ,
										  &clearValue,
										  IID_PPV_ARGS(&pTextureResource));
	BREAK_IF_FAILED(hr);
	pTextureResource->SetName(L"TextureDepthResource");


	if (pDSVDescriprotAllocator->AllocDescriptorHandle(&dsvHandle))
	{
		pDevice->CreateDepthStencilView(pTextureResource, &dsvDesc, dsvHandle);
	}
	else
	{
		pTextureResource->Release();
		pTextureResource = nullptr;
		goto LB_RETURN;
	}

	if (pSRVDescriptorAllocator->AllocDescriptorHandle(&srvHandle))
	{
		pDevice->CreateShaderResourceView(pTextureResource, &srvDesc, srvHandle);
	}
	else
	{
		pTextureResource->Release();
		pTextureResource = nullptr;
		goto LB_RETURN;
	}

	pTextureHandle = allocTextureHandle();
	pTextureHandle->pTextureResource = pTextureResource;
	pTextureHandle->DSVHandle = dsvHandle;
	pTextureHandle->SRVHandle = srvHandle;
	pTextureHandle->GPUHandle = pTextureResource->GetGPUVirtualAddress();

LB_RETURN:
	return pTextureHandle;
}

TextureHandle* TextureManager::CreateNonImageTexture(UINT numElement, UINT elementSize)
{
	_ASSERT(m_pRenderer);
	_ASSERT(m_pResourceManager);

	ID3D12Device* pDevice = m_pRenderer->GetD3DDevice();
	DescriptorAllocator* pSRVDescriptroAllocator = m_pRenderer->GetSRVUAVAllocator();
	TextureHandle* pTextureHandle = nullptr;

	ID3D12Resource* pTextureResource = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle;

	HRESULT hr = m_pResourceManager->CreateNonImageUploadTexture(&pTextureResource, numElement, elementSize);
	if (SUCCEEDED(hr))
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = numElement;
		srvDesc.Buffer.StructureByteStride = elementSize;
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

		if (pSRVDescriptroAllocator->AllocDescriptorHandle(&srvHandle))
		{
			pDevice->CreateShaderResourceView(pTextureResource, &srvDesc, srvHandle);

			pTextureHandle = allocTextureHandle();
			pTextureHandle->pTextureResource = pTextureResource;
			pTextureHandle->SRVHandle = srvHandle;
			pTextureHandle->GPUHandle = pTextureResource->GetGPUVirtualAddress();
		}
		else
		{
			pTextureResource->Release();
			pTextureResource = nullptr;
		}
	}
	
	return pTextureHandle;
}

void TextureManager::DeleteTexture(TextureHandle* pHandle)
{
	freeTextureHandle(pHandle);
}

void TextureManager::Cleanup()
{
	// 텍스쳐들은 모델에서 사용 후 정리하면서 texture manager에서 ref count를 줄여감.
	// 따라서, ref count가 남아있는 텍스쳐가 있다면 이는 누수임.
	if (m_pTextureLinkHead)
	{
		__debugbreak();
	}
	if (m_pHashTable)
	{
		delete m_pHashTable;
		m_pHashTable = nullptr;
	}
}

TextureHandle* TextureManager::allocTextureHandle()
{
	TextureHandle* pTextureHandle = new TextureHandle;
	ZeroMemory(pTextureHandle, sizeof(TextureHandle));

	pTextureHandle->Link.pItem = pTextureHandle;
	LinkElemIntoListFIFO(&m_pTextureLinkHead, &m_pTextureLinkTail, &pTextureHandle->Link);
	pTextureHandle->RefCount = 1;
	
	return pTextureHandle;
}

UINT TextureManager::freeTextureHandle(TextureHandle* pHandle)
{
	ID3D12Device* pDevice = m_pRenderer->GetD3DDevice();
	DescriptorAllocator* pRTVAllocator = m_pRenderer->GetRTVAllocator();
	DescriptorAllocator* pDSVAllocator = m_pRenderer->GetDSVAllocator();
	DescriptorAllocator* pSRVAllocator = m_pRenderer->GetSRVUAVAllocator();

	if (pHandle->RefCount == 0)
	{
		__debugbreak();
	}

	UINT refCount = --pHandle->RefCount;
	if (refCount == 0)
	{
		if (pHandle->pTextureResource)
		{
			pHandle->pTextureResource->Release();
			pHandle->pTextureResource = nullptr;
		}
		if (pHandle->pUploadBuffer)
		{
			pHandle->pUploadBuffer->Release();
			pHandle->pUploadBuffer = nullptr;
		}
		if (pHandle->RTVHandle.ptr)
		{
			pRTVAllocator->FreeDescriptorHandle(pHandle->RTVHandle);
			pHandle->RTVHandle = {};
		}
		if (pHandle->DSVHandle.ptr)
		{
			pDSVAllocator->FreeDescriptorHandle(pHandle->DSVHandle);
			pHandle->DSVHandle = {};
		}
		if (pHandle->SRVHandle.ptr)
		{
			pSRVAllocator->FreeDescriptorHandle(pHandle->SRVHandle);
			pHandle->SRVHandle = {};
		}
		if (pHandle->GPUHandle)
		{
			pHandle->GPUHandle = {};
		}
		if (pHandle->pSearchHandle)
		{
			m_pHashTable->Delete(pHandle->pSearchHandle);
			pHandle->pSearchHandle = nullptr;
		}
		UnLinkElemFromList(&m_pTextureLinkHead, &m_pTextureLinkTail, &pHandle->Link);

		delete pHandle;
	}

	return refCount;
}
