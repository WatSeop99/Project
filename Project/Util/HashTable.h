#pragma once

#include "LinkedList.h"

struct Bucket
{
	ListElem* pBucketLinkHead;
	ListElem* pBucketLinkTail;
	UINT LinkNum;
};
struct BucketItem
{
	const void* pItem;
	Bucket* pBucket;
	ListElem SortLink;
	UINT Size;
	char pKeyData[1];
};
class HashTable
{
public:
	HashTable() = default;
	~HashTable() { Cleanup(); };

	void Initialize(UINT maxBucketNum, UINT maxKeySize, UINT bucketNum);

	UINT Select(void** ppItemList, UINT maxItemNum, const void* pKeyData, UINT size);
	void* Insert(const void* pItem, const void* pKeyData, UINT size);
	void Delete(const void* pSearchHandle);
	void DeleteAll();

	void Cleanup();

protected:
	UINT createKey(const void* pData, UINT size, UINT bucketNum);

	void resourceCheck();

private:
	Bucket* m_pBucketTable = nullptr;
	UINT m_MaxBucketNum = 0;
	UINT m_MaxKeyDataSize = 0;
	UINT m_ItemNum = 0;
};
