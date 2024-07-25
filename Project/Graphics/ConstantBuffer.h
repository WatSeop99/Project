#pragma once

#include "../Renderer/Renderer.h"

class Renderer;

class ConstantBuffer
{
public:
	ConstantBuffer() = default;
	~ConstantBuffer() { Cleanup(); }

	void Initialize(Renderer* pRenderer, UINT64 bufferSize);

	void Upload();

	void Cleanup();

	inline void SetCBVHandle(const D3D12_CPU_DESCRIPTOR_HANDLE HANDLE) { m_CBVHandle = HANDLE; }

	inline ID3D12Resource* GetResource() { return m_pResource; }
	inline void* GetSysMemAddr() { return m_pSystemMemAddr; }
	inline D3D12_GPU_VIRTUAL_ADDRESS GetGPUMemAddr() { return m_GPUMemAddr; }
	inline D3D12_CPU_DESCRIPTOR_HANDLE GetCBVHandle() { return m_CBVHandle; }
	inline UINT64 GetBufferSize() { return m_BufferSize; }

public:
	void* pData = nullptr;

private:
	ID3D12Resource* m_pResource = nullptr;
	void* m_pSystemMemAddr = nullptr;
	D3D12_GPU_VIRTUAL_ADDRESS m_GPUMemAddr = 0xffffffffffffffff;
	D3D12_CPU_DESCRIPTOR_HANDLE m_CBVHandle = { 0xffffffffffffffff, };

	UINT64 m_BufferSize = 0;
	UINT64 m_DataSize = 0;
};
