#pragma once

#include "ConstantBuffer.h"

class ImageFilter
{
public:
	struct ImageResource
	{
		ID3D12Resource* pResource = nullptr;
		UINT RTVOffset = 0xffffffff;
		UINT SRVOffset = 0xffffffff;
	};
	struct Handle
	{
		ID3D12Resource* pResource = nullptr;
		D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle;
	};

public:
	ImageFilter() = default;
	~ImageFilter() { Clear(); }

	void Initialize(ResourceManager* pManager, const int WIDTH, const int HEIGHT);

	void UpdateConstantBuffers();

	void BeforeRender(ResourceManager* pManager, ePipelineStateSetting psoSetting, UINT frameIndex);
	void BeforeRender(ResourceManager* pManager, ID3D12GraphicsCommandList* pCommandList, ePipelineStateSetting psoSetting, UINT frameIndex);
	void AfterRender(ResourceManager* pManager, ePipelineStateSetting psoSetting, UINT frameIndex);
	void AfterRender(ResourceManager* pManager, ID3D12GraphicsCommandList* pCommandList, ePipelineStateSetting psoSetting, UINT frameIndex);

	void Clear();

	inline ConstantBuffer* GetConstantPtr() { return &m_ConstantBuffer; }

	void SetSRVOffsets(ResourceManager* pManager, const std::vector<ImageResource>& SRVs);
	void SetRTVOffsets(ResourceManager* pManager, const std::vector<ImageResource>& RTVs);
	void SetDescriptorHeap(ResourceManager* pManager);

private:
	ConstantBuffer m_ConstantBuffer;

	std::vector<Handle> m_SRVHandles;
	std::vector<Handle> m_RTVHandles;
};
