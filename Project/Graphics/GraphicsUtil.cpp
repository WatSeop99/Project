#include <d3dx12/d3dx12.h>
#include <DirectXTex.h>
#include <DirectXTexEXR.h>
#include <directxtk12/DDSTextureLoader.h>
#include <directxtk12/ResourceUploadBatch.h>
#include <wrl/client.h>
#include "../pch.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

#include "GraphicsUtil.h"

HRESULT CompileShader(const WCHAR* pszFileName, const char* pszShaderVersion, const D3D_SHADER_MACRO* pSHADER_MACROS, ID3DBlob** ppShader)
{
	_ASSERT(*ppShader == nullptr);

	HRESULT hr = S_OK;

	ID3DBlob* pErrorBlob = nullptr;
	UINT compileFlags = 0;

#ifdef _DEBUG
	compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	hr = D3DCompileFromFile(pszFileName, pSHADER_MACROS, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", pszShaderVersion, compileFlags, 0, ppShader, &pErrorBlob);
	if (FAILED(hr) && pErrorBlob)
	{
		OutputDebugStringA((const char*)(pErrorBlob->GetBufferPointer()));
		SAFE_RELEASE(pErrorBlob);
	}

	return hr;
}

HRESULT ReadImage(const WCHAR* pszAlbedoFileName, const WCHAR* pszOpacityFileName, std::vector<UCHAR>& image, int* pWidth, int* pHeight)
{
	std::vector<UCHAR> opacityImage;
	HRESULT hr = ReadImage(pszAlbedoFileName, image, pWidth, pHeight);
	if (FAILED(hr))
	{
		goto LB_RET;
	}

	{
		int opaWidth = 0;
		int opaHeight = 0;

		hr = ReadImage(pszOpacityFileName, opacityImage, &opaWidth, &opaHeight);
		if (FAILED(hr))
		{
			goto LB_RET;
		}

		_ASSERT(*pWidth == opaWidth && *pHeight == opaHeight);
	}

	for (int j = 0; j < *pHeight; ++j)
	{
		for (int i = 0; i < *pWidth; ++i)
		{
			image[3 + 4 * i + 4 * (*pWidth) * j] = opacityImage[4 * i + 4 * (*pWidth) * j]; // 알파채널 복사.
		}
	}

LB_RET:
	return hr;
}

HRESULT ReadImage(const WCHAR* pszFileName, std::vector<UCHAR>& image, int* pWidth, int* pHeight)
{
	HRESULT hr = S_OK;
	int channels = 0;

	char pFileName[MAX_PATH];
	if (!WideCharToMultiByte(CP_ACP, 0, pszFileName, -1, pFileName, MAX_PATH, nullptr, nullptr))
	{
		pFileName[0] = '\0';
	}

	unsigned char* pImg = stbi_load(pFileName, pWidth, pHeight, &channels, 0);

	// 4채널로 만들어 복사.
	image.resize((*pWidth) * (*pHeight) * 4);
	switch (channels)
	{
		case 1:
		{
			for (int i = 0, size = (*pWidth) * (*pHeight); i < size; ++i)
			{
				UCHAR g = pImg[i * channels];
				for (int c = 0; c < 4; ++c)
				{
					image[4 * i + c] = g;
				}
			}
		}
		break;

		case 2:
		{
			for (int i = 0, size = (*pWidth) * (*pHeight); i < size; ++i)
			{
				for (int c = 0; c < 2; ++c)
				{
					image[4 * i + c] = pImg[i * channels + c];
				}
				image[4 * i + 2] = 255;
				image[4 * i + 3] = 255;
			}
		}
		break;

		case 3:
		{
			for (int i = 0, size = (*pWidth) * (*pHeight); i < size; ++i)
			{
				for (int c = 0; c < 3; ++c)
				{
					image[4 * i + c] = pImg[i * channels + c];
				}
				image[4 * i + 3] = 255;
			}
		}
		break;

		case 4:
		{
			for (int i = 0, size = (*pWidth) * (*pHeight); i < size; ++i)
			{
				for (int c = 0; c < 4; ++c)
				{
					image[4 * i + c] = pImg[i * channels + c];
				}
			}
		}
		break;

		default:
			hr = E_FAIL;
			break;
	}

	free(pImg);

	return hr;
}

HRESULT ReadEXRImage(const WCHAR* pszFileName, std::vector<UCHAR>& image, int* pWidth, int* pHeight, DXGI_FORMAT* pPixelFormat)
{
	HRESULT hr = S_OK;

	DirectX::TexMetadata metaData;
	DirectX::ScratchImage scratchImage;

	hr = GetMetadataFromEXRFile(pszFileName, metaData);
	if (FAILED(hr))
	{
		goto LB_RET;
	}

	hr = LoadFromEXRFile(pszFileName, nullptr, scratchImage);
	if (FAILED(hr))
	{
		goto LB_RET;
	}

	*pWidth = (int)(metaData.width);
	*pHeight = (int)(metaData.height);
	*pPixelFormat = metaData.format;

	image.resize(scratchImage.GetPixelsSize());
	memcpy(image.data(), scratchImage.GetPixels(), image.size());

LB_RET:
	return hr;
}

HRESULT ReadDDSImage(ID3D12Device* pDevice, ID3D12CommandQueue* pCommandQueue, const WCHAR* pszFileName, ID3D12Resource** ppResource)
{
	_ASSERT(pDevice);
	_ASSERT(pCommandQueue);
	_ASSERT(*ppResource == nullptr);

	HRESULT hr = S_OK;
	Microsoft::WRL::ComPtr<ID3D12Resource> pResource;
	DirectX::ResourceUploadBatch resourceUpload(pDevice);

	resourceUpload.Begin();

	hr = DirectX::CreateDDSTextureFromFile(pDevice, resourceUpload, pszFileName, pResource.ReleaseAndGetAddressOf());
	BREAK_IF_FAILED(hr);

	// Upload the resources to the GPU.
	auto uploadResourcesFinished = resourceUpload.End(pCommandQueue);

	// Wait for the upload thread to terminate
	uploadResourcesFinished.wait();

	pResource->SetName(L"TextureResource");

	hr = pResource.CopyTo(ppResource);
	BREAK_IF_FAILED(hr);

	return hr;
}

UINT64 GetPixelSize(const DXGI_FORMAT PIXEL_FORMAT)
{
	switch (PIXEL_FORMAT)
	{
		case DXGI_FORMAT_R16G16B16A16_FLOAT:
			return sizeof(USHORT) * 4;

		case DXGI_FORMAT_R32G32B32A32_FLOAT:
			return sizeof(UINT) * 4;

		case DXGI_FORMAT_R32_FLOAT:
			return sizeof(UINT);

		case DXGI_FORMAT_R8G8B8A8_UNORM:
			return sizeof(UCHAR) * 4;

		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
			return sizeof(UCHAR) * 4;

		case DXGI_FORMAT_R32_SINT:
			return sizeof(int);

		case DXGI_FORMAT_R16_FLOAT:
			return sizeof(USHORT);

		default:
			break;
	}

	char debugString[256];
	sprintf_s(debugString, 256, "PixelFormat not implemented %d\n", PIXEL_FORMAT);
	OutputDebugStringA(debugString);

	return sizeof(UCHAR) * 4;
}