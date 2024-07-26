#pragma once

#include "../Util/HashTable.h"
#include "Renderer.h"
#include "ResourceManager.h"

class Renderer;

struct TextureHandle
{
	ID3D12Resource* pTextureResource;
	ID3D12Resource* pUploadBuffer;
	D3D12_CPU_DESCRIPTOR_HANDLE RTVHandle;
	D3D12_CPU_DESCRIPTOR_HANDLE DSVHandle;
	D3D12_CPU_DESCRIPTOR_HANDLE SRVHandle;
	D3D12_GPU_VIRTUAL_ADDRESS GPUHandle;
	void* pSearchHandle;
	ListElem* pLink;
	UINT RefCount;
	bool bUpdated;
	bool bFromFile;
};
class TextureManager
{
public:
	static void SetInitializeTextureHandle(TextureHandle* pHandle);

	TextureManager() = default;
	~TextureManager() { Cleanup(); }

	void Initialize(Renderer* pRenderer, UINT maxBucketNum, UINT maxFileNum);

	TextureHandle* CreateTextureFromFile(const WCHAR* pszFileName);
	TextureHandle* CreateDynamicTexture(UINT width, UINT height);
	TextureHandle* CreateImmutableTexture(UINT widht, UINT height, DXGI_FORMAT format, const BYTE* pInitImage);

	void DeleteTexture(TextureHandle* pHandle);

	void Cleanup();

protected:
	TextureHandle* allocTextureHandle();
	UINT freeTextureHandle(TextureHandle* pHandle);

private:
	Renderer* m_pRenderer = nullptr;
	ResourceManager* m_pResourceManager = nullptr;

	ListElem* m_pTextureLinkHead = nullptr;
	ListElem* m_pTextureLinkTail = nullptr;

	HashTable* m_pHashTable = nullptr;
};
