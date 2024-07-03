#include "../pch.h"
#include "Utility.h"

void GetHardwareAdapter(IDXGIFactory2* pFactory, IDXGIAdapter1** ppAdapter)
{
	IDXGIAdapter1* adapter = nullptr;
	*ppAdapter = nullptr;

	for (UINT adapterIndex = 0; pFactory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND; ++adapterIndex)
	{
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			// Don't select the Basic Render Driver adapter.
			continue;
		}

		// Check to see if the adapter supports Direct3D 12, but don't create the actual device yet.
		if (SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
		{
			break;
		}
	}

	*ppAdapter = adapter;
}

void GetSoftwareAdapter(IDXGIFactory2* pFactory, IDXGIAdapter1** ppAdapter)
{
	IDXGIAdapter1* adapter = nullptr;
	*ppAdapter = nullptr;

	for (UINT adapterIndex = 0; pFactory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND; ++adapterIndex)
	{
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			// Check to see if the adapter supports Direct3D 12, but don't create the actual device yet.
			if (SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_1, _uuidof(ID3D12Device), nullptr)))
			{
				*ppAdapter = adapter;
				break;
			}
		}
	}
}

void SetDebugLayerInfo(ID3D12Device* pD3DDevice)
{
	ID3D12InfoQueue* pInfoQueue = nullptr;

	pD3DDevice->QueryInterface(IID_PPV_ARGS(&pInfoQueue));
	if (pInfoQueue)
	{
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);

		D3D12_MESSAGE_ID hide[] =
		{
			D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
			D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
			// Workarounds for debug layer issues on hybrid-graphics systems
			D3D12_MESSAGE_ID_EXECUTECOMMANDLISTS_WRONGSWAPCHAINBUFFERREFERENCE,
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
		};
		D3D12_INFO_QUEUE_FILTER filter = {};
		filter.DenyList.NumIDs = (UINT)_countof(hide);
		filter.DenyList.pIDList = hide;
		pInfoQueue->AddStorageFilterEntries(&filter);

		SAFE_RELEASE(pInfoQueue);
	}
}

std::string RemoveBasePath(const std::string& szFilePath)
{
	size_t lastSlash;
	if ((lastSlash = szFilePath.rfind('/')) != std::string::npos)
	{
		return szFilePath.substr(lastSlash + 1, std::string::npos);
	}
	else if ((lastSlash = szFilePath.rfind('\\')) != std::string::npos)
	{
		return szFilePath.substr(lastSlash + 1, std::string::npos);
	}
	else
	{
		return szFilePath;
	}
}

std::wstring RemoveBasePath(const std::wstring& szFilePath)
{
	size_t lastSlash;
	if ((lastSlash = szFilePath.rfind(L'/')) != std::string::npos)
	{
		return szFilePath.substr(lastSlash + 1, std::string::npos);
	}
	else if ((lastSlash = szFilePath.rfind(L'\\')) != std::string::npos)
	{
		return szFilePath.substr(lastSlash + 1, std::string::npos);
	}
	else
	{
		return szFilePath;
	}
}

std::wstring GetFileExtension(const std::wstring& szFilePath)
{
	std::wstring fileName = RemoveBasePath(szFilePath);
	size_t extOffset = fileName.rfind(L'.');
	if (extOffset == std::wstring::npos)
	{
		return L"";
	}

	return fileName.substr(extOffset + 1);
}

ULONGLONG GetAllocMemorySize(ULONGLONG size)
{
	return (size + sizeof(ULONGLONG) * 2);
}

int Min(int x, int y)
{
	return (x < y ? x : y);
}

int Max(int x, int y)
{
	return (x > y ? x : y);
}

float Min(float x, float y)
{
	return (x < y ? x : y);
}

float Max(float x, float y)
{
	return (x > y ? x : y);
}

float Clamp(float x, float upper, float lower)
{
	return Max(lower, Min(x, upper));
}
