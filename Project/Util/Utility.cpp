#include "../pch.h"
#include "Utility.h"

typedef BOOL(WINAPI* LPFN_GLPI)(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION, PDWORD);
UINT CountSetBits(ULONG_PTR bitMask)
{
	UINT LSHIFT = sizeof(ULONG_PTR) * 8 - 1;
	UINT bitSetCount = 0;
	ULONG_PTR bitTest = (ULONG_PTR)1 << LSHIFT;

	for (UINT i = 0; i <= LSHIFT; ++i)
	{
		bitSetCount += ((bitMask & bitTest) ? 1 : 0);
		bitTest /= 2;
	}

	return bitSetCount;
}

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

void GetPhysicalCoreCount(UINT* pPhysicalCoreCount, UINT* pLogicalCoreCount)
{
	LPFN_GLPI glpi;

	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION pBuffer = nullptr;
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION ptr = nullptr;
	DWORD returnLength = 0;
	DWORD logicalProcessorCount = 0;
	DWORD numaNodeCount = 0;
	DWORD processorCoreCount = 0;
	DWORD processorL1CacheCount = 0;
	DWORD processorL2CacheCount = 0;
	DWORD processorL3CacheCount = 0;
	DWORD processorPackageCount = 0;
	DWORD byteOffset = 0;
	PCACHE_DESCRIPTOR Cache;

	glpi = (LPFN_GLPI)GetProcAddress(GetModuleHandle(TEXT("kernel32")), "GetLogicalProcessorInformation");
	if (nullptr == glpi)
	{
		SYSTEM_INFO systemInfo;
		GetSystemInfo(&systemInfo);
		*pPhysicalCoreCount = systemInfo.dwNumberOfProcessors;
		*pLogicalCoreCount = systemInfo.dwNumberOfProcessors;

		OutputDebugStringW(L"\nGetLogicalProcessorInformation is not supported.\n");
		return;
	}

	bool bDone = false;
	while (!bDone)
	{
		DWORD rc = glpi(pBuffer, &returnLength);

		if (FALSE == rc)
		{
			if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
			{
				if (pBuffer)
					free(pBuffer);

				pBuffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)malloc(returnLength);
			}
			else
			{
				break;
			}
		}
		else
		{
			bDone = true;
		}
	}

	ptr = pBuffer;

	while (byteOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= returnLength)
	{
		switch (ptr->Relationship)
		{
			case RelationNumaNode:
				// Non-NUMA systems report a single record of this type.
				++numaNodeCount;
				break;

			case RelationProcessorCore:
				++processorCoreCount;

				// A hyperthreaded core supplies more than one logical processor.
				logicalProcessorCount += CountSetBits(ptr->ProcessorMask);
				break;

			case RelationCache:
				// Cache data is in ptr->Cache, one CACHE_DESCRIPTOR structure for each cache. 
				Cache = &ptr->Cache;
				if (Cache->Level == 1)
				{
					++processorL1CacheCount;
				}
				else if (Cache->Level == 2)
				{
					++processorL2CacheCount;
				}
				else if (Cache->Level == 3)
				{
					++processorL3CacheCount;
				}
				break;

			case RelationProcessorPackage:
				// Logical processors share a physical package.
				++processorPackageCount;
				break;

			default:
				OutputDebugStringW(L"\nError: Unsupported LOGICAL_PROCESSOR_RELATIONSHIP value.\n");
				break;
		}
		byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
		ptr++;
	}
	*pPhysicalCoreCount = processorCoreCount;
	*pLogicalCoreCount = logicalProcessorCount;
	//numaNodeCount;
	//processorPackageCount;
	//processorCoreCount;
	//logicalProcessorCount;
	//processorL1CacheCount;
	//processorL2CacheCount;
	//processorL3CacheCount

	free(pBuffer);
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

UINT64 GetAllocMemSize(UINT64 size)
{
	return size + sizeof(UINT64) * 2;
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

Vector3 Min(const Vector3& V1, const Vector3& V2)
{
	DirectX::XMVECTOR v1 = DirectX::XMLoadFloat3(&V1);
	DirectX::XMVECTOR v2 = DirectX::XMLoadFloat3(&V2);
	DirectX::XMVECTOR ret = DirectX::XMVectorMin(v1, v2);

	return Vector3(ret);
}

Vector3 Max(const Vector3& V1, const Vector3& V2)
{
	DirectX::XMVECTOR v1 = DirectX::XMLoadFloat3(&V1);
	DirectX::XMVECTOR v2 = DirectX::XMLoadFloat3(&V2);
	DirectX::XMVECTOR ret = DirectX::XMVectorMax(v1, v2);

	return Vector3(ret);
}

float Clamp(float x, float upper, float lower)
{
	return Max(lower, Min(x, upper));
}
