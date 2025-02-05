#pragma once

class Renderer;

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
	~ImageFilter() { Cleanup(); }

	void Initialize(Renderer* pRenderer, const int WIDTH, const int HEIGHT);

	void UpdateConstantBuffers();

	void BeforeRender(Renderer* pRenderer, eRenderPSOType psoSetting, UINT frameIndex);
	void BeforeRender(UINT threadIndex, ID3D12GraphicsCommandList* pCommandList, DynamicDescriptorPool* pDescriptorPool, ConstantBufferManager* pConstantBufferManager, ResourceManager* pResourceManager, int psoSetting, UINT frameIndex);
	void AfterRender(Renderer* pRenderer, eRenderPSOType psoSetting, UINT frameIndex);
	void AfterRender(ID3D12GraphicsCommandList* pCommandList, int psoSetting);

	void Cleanup();

	inline ImageFilterConstant* GetConstantDataPtr() { return &m_ConstantBufferData; }

	void SetSRVOffsets(Renderer* pRenderer, const std::vector<ImageResource>& SRVs);
	void SetRTVOffsets(Renderer* pRenderer, const std::vector<ImageResource>& RTVs);

private:
	Renderer* m_pRenderer = nullptr;

	ImageFilterConstant m_ConstantBufferData;

	std::vector<Handle> m_SRVHandles;
	std::vector<Handle> m_RTVHandles;
};
