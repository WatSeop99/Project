#pragma once

#include "../Util/LinkedList.h"

struct CommandList
{
	ID3D12CommandAllocator* pCommandAllocator;
	ID3D12GraphicsCommandList* pCommandList;
	ListElem Link;
	bool bClosed;
};

class CommandListPool
{
public:
	CommandListPool() = default;
	~CommandListPool() { Cleanup(); }

	void Initialize(ID3D12Device5* pDevice, D3D12_COMMAND_LIST_TYPE type, UINT maxCommandListNum);

	void Close();
	void ClosedAndExecute(ID3D12CommandQueue* pCommandQueue);

	void Cleanup();

	void Reset();

	ID3D12GraphicsCommandList* GetCurrentCommandList();
	inline UINT GetTotalCmdListNum() const { return m_TotalCmdNum; }
	inline UINT GetAllocatedCmdListNum() const { return m_AllocatedCmdNum; }
	inline UINT GetAvailableCmdListNum() const { return m_AvailableCmdNum; }
	inline ID3D12Device5* GetDevice() { return m_pDevice; }

protected:
	bool addCmdList();
	CommandList* allocCmdList();

private:
	ID3D12Device5* m_pDevice = nullptr;
	D3D12_COMMAND_LIST_TYPE m_CommandListType = D3D12_COMMAND_LIST_TYPE_DIRECT;
	UINT m_AllocatedCmdNum = 0;
	UINT m_AvailableCmdNum = 0;
	UINT m_TotalCmdNum = 0;
	UINT m_MaxCmdListNum = 0;

	CommandList* m_pCurCmdList = nullptr;
	ListElem* m_pAllocatedCmdLinkHead = nullptr;
	ListElem* m_pAllocatedCmdLinkTail = nullptr;
	ListElem* m_pAvailableCmdLinkHead = nullptr;
	ListElem* m_pAvailableCmdLinkTail = nullptr;

	UINT m_PoolIndex = 0;
};

