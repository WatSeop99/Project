#include "../pch.h"
#include "HashTable.h"

void HashTable::Initialize(UINT maxBucketNum, UINT maxKeySize, UINT bucketNum)
{
	_ASSERT(maxBucketNum > 0);
	_ASSERT(maxKeySize > 0);
	_ASSERT(bucketNum > 0);

	m_MaxBucketNum = maxBucketNum;
	m_MaxKeyDataSize = maxKeySize;

	m_pBucketTable = new Bucket[maxBucketNum];
	ZeroMemory(m_pBucketTable, sizeof(Bucket) * maxBucketNum);
}

UINT HashTable::Select(void** ppItemList, UINT maxItemNum, const void* pKeyData, UINT size)
{
	_ASSERT(ppItemList);
	_ASSERT(pKeyData);

	UINT selectedItemNum = 0;
	UINT index = createKey(pKeyData, size, m_MaxBucketNum);
	Bucket* pBucket = m_pBucketTable + index;

	ListElem* pCurBucket = pBucket->pBucketLinkHead;
	BucketItem* pBucketItem;

	while (pCurBucket)
	{
		if (!maxItemNum)
		{
			break;
		}

		pBucketItem = (BucketItem*)pCurBucket->pItem;

		if (pBucketItem->Size != size)
		{
			goto LB_NEXT;
		}
		if (memcmp(pBucketItem->pKeyData, pKeyData, size))
		{
			goto LB_NEXT;
		}

		--maxItemNum;

		ppItemList[selectedItemNum] = (void*)pBucketItem->pItem;
		++selectedItemNum;

	LB_NEXT:
		pCurBucket = pCurBucket->pNext;
	}

	return selectedItemNum;
}

void* HashTable::Insert(const void* pItem, const void* pKeyData, UINT size)
{
	_ASSERT(pItem);
	_ASSERT(pKeyData);

	void* pSearchHandle = nullptr;

	if (size > m_MaxKeyDataSize)
	{
		__debugbreak();
		// goto LB_RETURN;
	}

	UINT bucketMemSize = (UINT)(sizeof(BucketItem) - sizeof(char)) + m_MaxKeyDataSize;
	BucketItem* pBucketItem = (BucketItem*)malloc(bucketMemSize);

	UINT index = createKey(pKeyData, size, m_MaxBucketNum);
	Bucket* pBucket = m_pBucketTable + index;

	pBucketItem->pItem = pItem;
	pBucketItem->Size = size;
	pBucketItem->pBucket = pBucket;
	pBucketItem->SortLink.pPrev = nullptr;
	pBucketItem->SortLink.pNext = nullptr;
	pBucketItem->SortLink.pItem = pBucketItem;
	pBucket->LinkNum++;

	memcpy(pBucketItem->pKeyData, pKeyData, size);

	LinkElemIntoListFIFO(&pBucket->pBucketLinkHead, &pBucket->pBucketLinkTail, &pBucketItem->SortLink);

	++m_ItemNum;
	pSearchHandle = pBucketItem;

	return pSearchHandle;
}

void HashTable::Delete(const void* pSearchHandle)
{
	_ASSERT(pSearchHandle);

	BucketItem* pBucketItem = (BucketItem*)pSearchHandle;
	Bucket* pBucket = pBucketItem->pBucket;

	UnLinkElemFromList(&pBucket->pBucketLinkHead, &pBucket->pBucketLinkTail, &pBucketItem->SortLink);
	--(pBucket->LinkNum);

	free(pBucketItem);
	--m_ItemNum;
}

void HashTable::DeleteAll()
{
	BucketItem* pBucketItem;
	for (UINT i = 0; i < m_MaxBucketNum; ++i)
	{
		while (m_pBucketTable[i].pBucketLinkHead)
		{
			pBucketItem = (BucketItem*)m_pBucketTable[i].pBucketLinkHead->pItem;
			Delete(pBucketItem);
		}
	}
}

void HashTable::Cleanup()
{
	resourceCheck();

	DeleteAll();
	if (m_pBucketTable)
	{
		delete[] m_pBucketTable;
		m_pBucketTable = nullptr;
	}
}

UINT HashTable::createKey(const void* pData, UINT size, UINT bucketNum)
{
	UINT keyData = 0;

	const char* pEntry = (char*)pData;
	if (size & 0x00000001)
	{
		keyData += (DWORD)(*(BYTE*)pEntry);
		++pEntry;
		--size;
	}
	if (!size)
	{
		goto LB_RETURN;
	}


	if (size & 0x00000002)
	{
		keyData += (UINT)(*(USHORT*)pEntry);
		pEntry += 2;
		size -= 2;
	}
	if (!size)
	{
		goto LB_RETURN;
	}

	size = (size >> 2);

	for (UINT i = 0; i < size; ++i)
	{
		keyData += *(UINT*)pEntry;
		pEntry += 4;
	}

LB_RETURN:
	UINT index = keyData % bucketNum;
	return index;
}

void HashTable::resourceCheck()
{
	if (m_ItemNum)
	{
		__debugbreak();
	}
}
