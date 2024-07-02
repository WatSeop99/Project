#pragma once

HRESULT CompileShader(const wchar_t* pszFileName, const char* pszShaderVersion, const D3D_SHADER_MACRO* pSHADER_MACROS, ID3DBlob** ppShader);

HRESULT ReadImage(const wchar_t* pszAlbedoFileName, const wchar_t* pszOpacityFileName, std::vector<UCHAR>& image, int* pWidth, int* pHeight);
HRESULT ReadImage(const wchar_t* pszFileName, std::vector<UCHAR>& image, int* pWidth, int* pHeight);
HRESULT ReadEXRImage(const wchar_t* pszFileName, std::vector<UCHAR>& image, int* pWidth, int* pHeight, DXGI_FORMAT* pPixelFormat);
HRESULT ReadDDSImage(ID3D12Device* pDevice, ID3D12CommandQueue* pCommandQueue, const wchar_t* pszFileName, ID3D12Resource** ppResource);

UINT64 GetPixelSize(const DXGI_FORMAT PIXEL_FORMAT);