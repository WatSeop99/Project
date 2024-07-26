#include "../pch.h"
#include "IndexCreator.h"

IndexCreator::~IndexCreator()
{
	Check();
	Clear();
}

void IndexCreator::Initialize(ULONG num)
{
	m_pIndexTable = new ULONG[num];
	ZeroMemory(m_pIndexTable, sizeof(ULONG) * num);
	m_MaxNum = num;

	for (ULONG i = 0; i < m_MaxNum; ++i)
	{
		m_pIndexTable[i] = i;
	}
}

ULONG IndexCreator::Alloc()
{
	// 1. m_lAllocatedCount에서 1을 뺀다.
	// 2. m_lAllocatedCount-1위치에 dwIndex를 써넣는다.
	// 이 두가지 액션이 필요한데 1과 2사이에 다른 스레드가 Alloc을 호출하면 이미 할당된 인덱스를 얻어가는 일이 발생한다.
	// 따라서 Alloc과 Free양쪽 다 스핀락으로 막아야한다.

	ULONG result = 0xffff;

	if (m_AllocatedCount >= m_MaxNum)
	{
		goto LB_RET;
	}

	result = m_pIndexTable[m_AllocatedCount];
	++m_AllocatedCount;

LB_RET:
	return result;
}

void IndexCreator::Free(ULONG index)
{
	if (!m_AllocatedCount)
	{
		__debugbreak();
	}
	--m_AllocatedCount;
	m_pIndexTable[m_AllocatedCount] = index;
}

void IndexCreator::Clear()
{
	m_MaxNum = 0;
	m_AllocatedCount = 0;

	if (m_pIndexTable)
	{
		delete[] m_pIndexTable;
		m_pIndexTable = nullptr;
	}
}

void IndexCreator::Check()
{
	if (m_AllocatedCount)
	{
		__debugbreak();
	}
}
