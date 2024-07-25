#include "../pch.h"
#include "ConstantDataType.h"
#include "ConstantBufferManager.h"

UINT64 g_CBSizes[ConstantBufferType_ConstantTypeCount] =
{
	sizeof(MeshConstant),
	sizeof(MaterialConstant),
	sizeof(LightProperty),
	sizeof(LightConstant),
	sizeof(ShadowConstant),
	sizeof(GlobalConstant),
	sizeof(ImageFilterConstant),
};

void ConstantBufferManager::Initialize(ID3D12Device* pDevice, UINT maxCBVNum)
{
	for (int i = 0; i < ConstantBufferType_ConstantTypeCount; ++i)
	{
		UINT CBSize = (g_CBSizes[i] + 255) & ~(255); // aligned.
		m_ppConstantBufferPool[i] = new ConstantBufferPool;
		m_ppConstantBufferPool[i]->Initialize(pDevice, (eConstantBufferType)i, CBSize, maxCBVNum);
	}
}

void ConstantBufferManager::Reset()
{
	for (int i = 0; i < ConstantBufferType_ConstantTypeCount; ++i)
	{
#ifdef _DEBUG
		if (m_ppConstantBufferPool[i])
		{
			m_ppConstantBufferPool[i]->Reset();
		}
#else
		m_ppConstantBufferPool[i]->Reset();
#endif
	}
}

void ConstantBufferManager::Cleanup()
{
	for (int i = 0; i < ConstantBufferType_ConstantTypeCount; ++i)
	{
		if (m_ppConstantBufferPool[i])
		{
			delete m_ppConstantBufferPool[i];
			m_ppConstantBufferPool[i] = nullptr;
		}
	}
}

ConstantBufferPool* ConstantBufferManager::GetConstantBufferPool(eConstantBufferType type)
{
	_ASSERT(m_ppConstantBufferPool);
	_ASSERT(type < ConstantBufferType_ConstantTypeCount);

	return m_ppConstantBufferPool[type];
}
