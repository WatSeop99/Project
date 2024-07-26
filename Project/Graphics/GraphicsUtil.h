#pragma once

HRESULT CompileShader(const WCHAR* pszFileName, const char* pszShaderVersion, const D3D_SHADER_MACRO* pSHADER_MACROS, ID3DBlob** ppShader);

HRESULT ReadImage(const WCHAR* pszAlbedoFileName, const WCHAR* pszOpacityFileName, std::vector<UCHAR>& image, int* pWidth, int* pHeight);
HRESULT ReadImage(const WCHAR* pszFileName, std::vector<UCHAR>& image, int* pWidth, int* pHeight);
HRESULT ReadEXRImage(const WCHAR* pszFileName, std::vector<UCHAR>& image, int* pWidth, int* pHeight, DXGI_FORMAT* pPixelFormat);
HRESULT ReadDDSImage(ID3D12Device* pDevice, ID3D12CommandQueue* pCommandQueue, const WCHAR* pszFileName, ID3D12Resource** ppResource);

UINT64 GetPixelSize(const DXGI_FORMAT PIXEL_FORMAT);