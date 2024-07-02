#pragma once

#include "ConstantBuffer.h"
#include "ImageFilter.h"
#include "../Model/Mesh.h"

class PostProcessor
{
public:
	struct PostProcessingBuffers
	{
		ID3D12Resource* ppBackBuffers[2] = { nullptr, };
		ID3D12Resource* pFloatBuffer = nullptr;
		ID3D12Resource* pPrevBuffer = nullptr;
		ConstantBuffer* pGlobalConstant = nullptr;
		UINT BackBufferRTV1Offset = 0xffffffff;
		UINT BackBufferRTV2Offset = 0xffffffff;
		UINT FloatBufferSRVOffset = 0xffffffff;
		UINT PrevBufferSRVOffset = 0xffffffff;
	};

public:
	PostProcessor() = default;
	~PostProcessor() { Clear(); }

	void Initizlie(ResourceManager* pManager, const PostProcessingBuffers& CONFIG, const int WIDTH, const int HEIGHT, const int BLOOMLEVELS);

	void Update(ResourceManager* pManager);

	void Render(ResourceManager* pManager, UINT frameIndex);
	void Render(ResourceManager* pManager, ID3D12GraphicsCommandList* pCommandList, UINT frameIndex);

	void Clear();

	void SetDescriptorHeap(ResourceManager* pManager);

protected:
	void createPostBackBuffers(ResourceManager* pManager);
	void createImageResources(ResourceManager* pManager, const int WIDTH, const int HEIGHT, ImageFilter::ImageResource* pImageResource);

	void renderPostProcessing(ResourceManager* pManager, UINT frameIndex);
	void renderPostProcessing(ResourceManager* pManager, ID3D12GraphicsCommandList* pCommandList, UINT frameIndex);
	void renderImageFilter(ResourceManager* pManager, ImageFilter& imageFilter, ePipelineStateSetting psoSetting, UINT frameIndex);
	void renderImageFilter(ResourceManager* pManager, ID3D12GraphicsCommandList* pCommandList, ImageFilter& imageFilter, ePipelineStateSetting psoSetting, UINT frameIndex);

	void setRenderConfig(const PostProcessingBuffers& CONFIG);

private:
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
	ConstantBuffer* m_pGlobalConstant = nullptr;
	UINT m_BackBufferRTV1Offset = 0xffffffff;
	UINT m_BackBufferRTV2Offset = 0xffffffff;
	UINT m_FloatBufferSRVOffset = 0xffffffff;
	UINT m_PrevBufferSRVOffset = 0xffffffff;
};
