#pragma once

class Texture
{
public:
	Texture() = default;
	~Texture() { Cleanup(); }

	void Initialize(Renderer* pRenderer, const WCHAR* pszFileName, bool bUseSRGB);
	void Initialize(Renderer* pRenderer, const D3D12_RESOURCE_DESC& DESC);
	void InitializeWithDDS(Renderer* pRenderer, const WCHAR* pszFileName);

	void Cleanup();

	inline ID3D12Resource* GetResource() { return m_pResource; }
	inline D3D12_GPU_VIRTUAL_ADDRESS GetGPUMemAddr() { return m_GPUMemAddr; }
	inline D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() { return m_DSVHandle; }
	inline D3D12_CPU_DESCRIPTOR_HANDLE GetSRVHandle() { return m_SRVHandle; }

	inline void SetDSVHandle(const D3D12_CPU_DESCRIPTOR_HANDLE HANDLE) { m_DSVHandle = HANDLE; }
	inline void SetSRVHandle(const D3D12_CPU_DESCRIPTOR_HANDLE HANDLE) { m_SRVHandle = HANDLE; }

private:
	ID3D12Resource* m_pResource = nullptr;
	D3D12_GPU_VIRTUAL_ADDRESS m_GPUMemAddr = 0xffffffffffffffff;
	D3D12_CPU_DESCRIPTOR_HANDLE m_DSVHandle = { 0xffffffffffffffff, };
	D3D12_CPU_DESCRIPTOR_HANDLE m_SRVHandle = { 0xffffffffffffffff, };

	UINT m_Width = 0;
	UINT m_Height = 0;
	UINT m_Depth = 0;
};

class NonImageTexture
{
public:
	NonImageTexture() = default;
	~NonImageTexture() { Clear(); }

	void Initialize(Renderer* pRenderer, UINT numElement, UINT elementSize);

	void Upload();

	void Clear();

	inline void SetSRVHandle(const D3D12_CPU_DESCRIPTOR_HANDLE HANDLE) { m_SRVHandle = HANDLE; }

	inline ID3D12Resource* GetResource() { return m_pResource; }
	inline void* GetSysMemAddr() { return m_pSysMemAddr; }
	inline D3D12_GPU_VIRTUAL_ADDRESS GetGPUMemAddr() { return m_GPUMemAddr; }
	inline D3D12_CPU_DESCRIPTOR_HANDLE GetSRVHandle() { return m_SRVHandle; }

public:
	void* pData = nullptr;
	UINT64 ElementSize = 0;
	UINT64 ElementCount = 0;

private:
	ID3D12Resource* m_pResource = nullptr;
	void* m_pSysMemAddr = nullptr;
	D3D12_GPU_VIRTUAL_ADDRESS m_GPUMemAddr = 0xffffffffffffffff;
	D3D12_CPU_DESCRIPTOR_HANDLE m_SRVHandle = { 0xffffffffffffffff, };

	UINT64 m_BufferSize = 0;
};
