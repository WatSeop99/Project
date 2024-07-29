#include "../pch.h"
#include "CommandListPool.h"

static UINT s_Count = 0;

void CommandListPool::Initialize(ID3D12Device5* pDevice, D3D12_COMMAND_LIST_TYPE type, UINT maxCommandListNum)
{
	_ASSERT(pDevice);
	_ASSERT(maxCommandListNum > 2);

	m_pDevice = pDevice;
	m_MaxCmdListNum = maxCommandListNum;
	m_CommandListType = type;
	m_PoolIndex = s_Count++;
}

void CommandListPool::Close()
{
	if (!m_pCurCmdList)
	{
		__debugbreak();
	}

	if (m_pCurCmdList->bClosed)
	{
		__debugbreak();
	}

	if (FAILED(m_pCurCmdList->pCommandList->Close()))
	{
		__debugbreak();
	}

	m_pCurCmdList->bClosed = true;
	m_pCurCmdList = nullptr;
}

void CommandListPool::ClosedAndExecute(ID3D12CommandQueue* pCommandQueue)
{
	// 현재 인덱스의 CommandList를 실행
	if (!m_pCurCmdList)
	{
		__debugbreak();
	}

	if (m_pCurCmdList->bClosed)
	{
		__debugbreak();
	}

	if (FAILED(m_pCurCmdList->pCommandList->Close()))
	{
		__debugbreak();
	}

	m_pCurCmdList->bClosed = true;

	pCommandQueue->ExecuteCommandLists(1, (ID3D12CommandList**)&m_pCurCmdList->pCommandList);
	m_pCurCmdList = nullptr;
}

void CommandListPool::Cleanup()
{
	HRESULT hr = S_OK;

	Reset();
	while (m_pAvailableCmdLinkHead)
	{
		CommandList* pCmdList = (CommandList*)m_pAvailableCmdLinkHead->pItem;
		pCmdList->pCommandList->Release();
		pCmdList->pCommandList = nullptr;

		pCmdList->pCommandAllocator->Release();
		pCmdList->pCommandAllocator = nullptr;
		--m_TotalCmdNum;

		UnLinkElemFromList(&m_pAvailableCmdLinkHead, &m_pAvailableCmdLinkTail, &pCmdList->Link);
		--m_AvailableCmdNum;

		delete pCmdList;
	}

	m_pCurCmdList = nullptr;
	m_pAllocatedCmdLinkHead = nullptr;
	m_pAllocatedCmdLinkTail = nullptr;

	m_AllocatedCmdNum = 0;
	m_AvailableCmdNum = 0;
	m_TotalCmdNum = 0;
	m_MaxCmdListNum = 0;
	m_pDevice = nullptr;
}

void CommandListPool::Reset()
{
	HRESULT hr = S_OK;

	while (m_pAllocatedCmdLinkHead)
	{
		CommandList* pCmdList = (CommandList*)m_pAllocatedCmdLinkHead->pItem;

		hr = pCmdList->pCommandAllocator->Reset();
		BREAK_IF_FAILED(hr);

		hr = pCmdList->pCommandList->Reset(pCmdList->pCommandAllocator, nullptr);
		BREAK_IF_FAILED(hr);

		pCmdList->bClosed = false;

		UnLinkElemFromList(&m_pAllocatedCmdLinkHead, &m_pAllocatedCmdLinkTail, &pCmdList->Link);
		--m_AllocatedCmdNum;

		LinkElemIntoListFIFO(&m_pAvailableCmdLinkHead, &m_pAvailableCmdLinkTail, &pCmdList->Link);
		++m_AvailableCmdNum;
	}
}

ID3D12CommandAllocator* CommandListPool::GetCurrentCommandAllocator()
{
	if (!m_pCurCmdList)
	{
		m_pCurCmdList = allocCmdList();
		if (!m_pCurCmdList)
		{
			__debugbreak();
		}
	}

	return m_pCurCmdList->pCommandAllocator;
}

ID3D12GraphicsCommandList* CommandListPool::GetCurrentCommandList()
{
	if (!m_pCurCmdList)
	{
		m_pCurCmdList = allocCmdList();
		if (!m_pCurCmdList)
		{
			__debugbreak();
		}
		char szDebugString[256];
		sprintf_s(szDebugString, 256, "alloc command list\n");
		OutputDebugStringA(szDebugString);
	}

	return m_pCurCmdList->pCommandList;
}

bool CommandListPool::addCmdList()
{
	HRESULT hr = S_OK;
	bool bRet = false;
	CommandList* pCmdList = nullptr;

	ID3D12CommandAllocator* pCommandAllocator = nullptr;
	ID3D12GraphicsCommandList* pCommandList = nullptr;
	WCHAR szDebugName[256] = { 0, };

	if (m_TotalCmdNum >= m_MaxCmdListNum)
	{
#ifdef _DEBUG
		__debugbreak();
#endif
		goto LB_RETURN;
	}

	hr = m_pDevice->CreateCommandAllocator(m_CommandListType, IID_PPV_ARGS(&pCommandAllocator));
	if (FAILED(hr))
	{
#ifdef _DEBUG
		__debugbreak();
#endif
		goto LB_RETURN;
	} // m_PoolIndex
	swprintf_s(szDebugName, 256, L"CommandAllocator%u", m_PoolIndex);
	pCommandAllocator->SetName(szDebugName);

	hr = m_pDevice->CreateCommandList(0, m_CommandListType, pCommandAllocator, nullptr, IID_PPV_ARGS(&pCommandList));
	if (FAILED(hr))
	{
#ifdef _DEBUG
		__debugbreak();
#endif
		pCommandAllocator->Release();
		pCommandAllocator = nullptr;
		goto LB_RETURN;
	}
	swprintf_s(szDebugName, 256, L"CommandList%u", m_PoolIndex);
	pCommandList->SetName(szDebugName);

	pCmdList = new CommandList;
	ZeroMemory(pCmdList, sizeof(CommandList));
	pCmdList->Link.pItem = pCmdList;
	pCmdList->pCommandList = pCommandList;
	pCmdList->pCommandAllocator = pCommandAllocator;
	++m_TotalCmdNum;

	LinkElemIntoListFIFO(&m_pAvailableCmdLinkHead, &m_pAvailableCmdLinkTail, &pCmdList->Link);
	++m_AvailableCmdNum;
	bRet = true;

LB_RETURN:
	return bRet;
}

CommandList* CommandListPool::allocCmdList()
{
	CommandList* pCmdList = nullptr;

	if (!m_pAvailableCmdLinkHead)
	{
		if (!addCmdList())
		{
			goto LB_RETURN;
		}
	}

	pCmdList = (CommandList*)m_pAvailableCmdLinkHead->pItem;

	UnLinkElemFromList(&m_pAvailableCmdLinkHead, &m_pAvailableCmdLinkTail, &pCmdList->Link);
	--m_AvailableCmdNum;

	LinkElemIntoListFIFO(&m_pAllocatedCmdLinkHead, &m_pAllocatedCmdLinkTail, &pCmdList->Link);
	++m_AllocatedCmdNum;

LB_RETURN:
	return pCmdList;
}
