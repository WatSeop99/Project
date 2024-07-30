#pragma once

#include "ImageFilter.h"
#include "../Model/Mesh.h"

class Renderer;

class PostProcessor
{
public:
	struct PostProcessingBuffers
	{
		ID3D12Resource* ppBackBuffers[2] = { nullptr, };
		ID3D12Resource* pFloatBuffer = nullptr;
		ID3D12Resource* pPrevBuffer = nullptr;
		GlobalConstant* pGlobalConstantData = nullptr;
		UINT BackBufferRTV1Offset = 0xffffffff;
		UINT BackBufferRTV2Offset = 0xffffffff;
		UINT FloatBufferSRVOffset = 0xffffffff;
		UINT PrevBufferSRVOffset = 0xffffffff;
	};

public:
	PostProcessor() = default;
	~PostProcessor() { Cleanup(); }

	void Initizlie(Renderer* pRenderer, const PostProcessingBuffers& CONFIG, const int WIDTH, const int HEIGHT, const int BLOOMLEVELS);

	void Update();

	void Render(UINT frameIndex);
	void Render(UINT threadIndex, ID3D12GraphicsCommandList* pCommandList, DynamicDescriptorPool* pDescriptorPool, ConstantBufferManager* pConstantBufferManager, ResourceManager* pManager, UINT frameIndex);

	void Cleanup();

	inline Mesh* GetScreenMeshPtr() { return m_pScreenMesh; }
	inline ImageFilter* GetSamplingFilterPtr() { return &m_BasicSamplingFilter; }
	inline std::vector<ImageFilter>* GetBloomDownFiltersPtr() { return &m_BloomDownFilters; }
	inline std::vector<ImageFilter>* GetBloomUpFiltersPtr() { return &m_BloomUpFilters; }
	inline ImageFilter* GetCombineFilterPtr() { return &m_CombineFilter; }

	void SetViewportsAndScissorRects(ID3D12GraphicsCommandList* pCommandList);

protected:
	void createPostBackBuffers();
	void createImageResources(const int WIDTH, const int HEIGHT, ImageFilter::ImageResource* pImageResource);

	void renderPostProcessing(UINT frameIndex);
	void renderPostProcessing(UINT threadIndex, ID3D12GraphicsCommandList* pCommandList, DynamicDescriptorPool* pDescriptorPool, ConstantBufferManager* pConstantBufferManager, ResourceManager* pResourceManager, UINT frameIndex);
	void renderImageFilter(ImageFilter& imageFilter, eRenderPSOType psoSetting, UINT frameIndex);
	void renderImageFilter(UINT threadIndex, ID3D12GraphicsCommandList* pCommandList, DynamicDescriptorPool* pDescriptorPool, ConstantBufferManager* pConstantBufferManager, ResourceManager* pResourceManager, ImageFilter& imageFilter, int psoSetting, UINT frameIndex);

	void setRenderConfig(const PostProcessingBuffers& CONFIG);

private:
	Renderer* m_pRenderer = nullptr;

	Mesh* m_pScreenMesh = nullptr;
	D3D12_VIEWPORT m_Viewport = { 0, };
	D3D12_RECT m_ScissorRect = { 0, };
	UINT m_ScreenWidth = 0;
	UINT m_ScreenHeight = 0;

	ImageFilter m_BasicSamplingFilter;
	ImageFilter m_CombineFilter;
	std::vector<ImageFilter> m_BloomDownFilters;
	std::vector<ImageFilter> m_BloomUpFilters;
	std::vector<ImageFilter::ImageResource> m_BloomResources;

	ID3D12Resource* m_pResolvedBuffer = nullptr;
	UINT m_ResolvedRTVOffset = 0xffffffff;
	UINT m_ResolvedSRVOffset = 0xffffffff;

	// do not release.
	ID3D12Resource* m_ppBackBuffers[2] = { nullptr, };
	ID3D12Resource* m_pFloatBuffer = nullptr;
	ID3D12Resource* m_pPrevBuffer = nullptr;
	GlobalConstant* m_pGlobalConstantData = nullptr;
	UINT m_BackBufferRTV1Offset = 0xffffffff;
	UINT m_BackBufferRTV2Offset = 0xffffffff;
	UINT m_FloatBufferSRVOffset = 0xffffffff;
	UINT m_PrevBufferSRVOffset = 0xffffffff;
};
