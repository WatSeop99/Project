#pragma once

struct ListElem
{
	ListElem* pPrev;
	ListElem* pNext;
	void* pItem;
};

void LinkElemIntoList();
void UnLinkElemFromList();
