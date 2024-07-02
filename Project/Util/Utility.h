#pragma once

struct Container
{
	ULONGLONG MemSize;
	ULONGLONG ElemCount;
	BYTE Data[1];
};

void GetHardwareAdapter(IDXGIFactory2* pFactory, IDXGIAdapter1** ppAdapter);
void GetSoftwareAdapter(IDXGIFactory2* pFactory, IDXGIAdapter1** ppAdapter);
void SetDebugLayerInfo(ID3D12Device* pD3DDevice);

std::string RemoveBasePath(const std::string& szFilePath);
std::wstring RemoveBasePath(const std::wstring& szFilePath);
std::wstring GetFileExtension(const std::wstring& szFilepath);

int Min(int x, int y);
int Max(int x, int y);
float Min(float x, float y);
float Max(float x, float y);
float Clamp(float x, float upper, float lower);
