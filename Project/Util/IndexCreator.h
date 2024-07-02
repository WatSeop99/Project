#pragma once

class IndexCreator
{
public:
	IndexCreator() = default;
	~IndexCreator();

	void Initialize(ULONG num);

	ULONG Alloc();

	void Free(ULONG index);

	void Clear();

	void Check();

private:
	ULONG* m_pIndexTable = nullptr;
	ULONG m_MaxNum = 0;
	ULONG m_AllocatedCount = 0;
};
