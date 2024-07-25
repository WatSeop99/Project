#pragma once

#include "ConstantBufferPool.h"

class ConstantBufferManager
{
public:
	ConstantBufferManager() = default;
	~ConstantBufferManager() { Cleanup(); }

	void Initialize(ID3D12Device* pDevice, UINT maxCBVNum);

	void Reset();

	void Cleanup();

	ConstantBufferPool* GetConstantBufferPool(eConstantBufferType type);

private:
	ConstantBufferPool* m_ppConstantBufferPool[ConstantBufferType_ConstantTypeCount] = { nullptr, };
};
