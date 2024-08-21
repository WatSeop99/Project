#pragma once

struct Container
{
	UINT64 MemSize;
	UINT64 ElemCount;
	BYTE Data[1];
};

void GetHardwareAdapter(IDXGIFactory2* pFactory, IDXGIAdapter1** ppAdapter);
void GetSoftwareAdapter(IDXGIFactory2* pFactory, IDXGIAdapter1** ppAdapter);
void SetDebugLayerInfo(ID3D12Device* pD3DDevice);

void GetPhysicalCoreCount(UINT* pPhysicalCoreCount, UINT* pLogicalCoreCount);

std::string RemoveBasePath(const std::string& szFilePath);
std::wstring RemoveBasePath(const std::wstring& szFilePath);
std::wstring GetFileExtension(const std::wstring& szFilepath);

UINT64 GetAllocMemSize(UINT64 size);

int Min(int x, int y);
int Max(int x, int y);
float Min(float x, float y);
float Max(float x, float y);
Vector3 Min(const Vector3& V1, const Vector3& V2);
Vector3 Max(const Vector3& V1, const Vector3& V2);
float Clamp(const float VAL, const float LOWER, const float UPPER);
