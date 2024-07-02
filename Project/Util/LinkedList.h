#pragma once

struct ListElem
{
	ListElem* pPrev;
	ListElem* pNext;
	void* pItem;
};

void LinkElemIntoList(ListElem** ppHead, ListElem** ppTail, ListElem* pNew);
void LinkElemIntoListFIFO(ListElem** ppHead, ListElem** ppTail, ListElem* pNew);
void UnLinkElemFromList(ListElem** ppHead, ListElem** ppTail, ListElem* pDel);
