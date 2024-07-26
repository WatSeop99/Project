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
	pHandle->pLink = nullptr;
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

	m_pHashTable = new HashTable;
	m_pHashTable->Initialize(maxBucketNum, _MAX_PATH, maxFileNum);
}

TextureHandle* TextureManager::CreateTextureFromFile(const WCHAR* pszFileName)
{
	_ASSERT(m_pRenderer);
	_ASSERT(pszFileName);

	ID3D12Device* pDevice = m_pRenderer->GetD3DDevice();
	ResourceManager* pResourceManager = m_pRenderer->GetResourceManager();
	
	return nullptr;
}

TextureHandle* TextureManager::CreateDynamicTexture(UINT width, UINT height)
{
	return nullptr;
}

TextureHandle* TextureManager::CreateImmutableTexture(UINT widht, UINT height, DXGI_FORMAT format, const BYTE* pInitImage)
{
	return nullptr;
}

void TextureManager::DeleteTexture(TextureHandle* pHandle)
{
}

void TextureManager::Cleanup()
{
}

TextureHandle* TextureManager::allocTextureHandle()
{
	return nullptr;
}

UINT TextureManager::freeTextureHandle(TextureHandle* pHandle)
{
	return 0;
}
