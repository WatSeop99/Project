#pragma once

#include "../pch.h"
#include "../Graphics/Texture.h"
#include "../Renderer/TextureManager.h"

// Vertex and Index Info
struct BufferInfo
{
	ID3D12Resource* pBuffer = nullptr;
	union
	{
		D3D12_VERTEX_BUFFER_VIEW VertexBufferView;
		D3D12_INDEX_BUFFER_VIEW IndexBufferView;
	};
	UINT Count;
};
struct Material
{
	Texture Albedo;
	Texture Emissive;
	Texture Normal;
	Texture Height;
	Texture AmbientOcclusion;
	Texture Metallic;
	Texture Roughness;

	TextureHandle* pAlbedo;
	TextureHandle* pEmissive;
	TextureHandle* pNormal;
	TextureHandle* pHeight;
	TextureHandle* pAmbientOcclusion;
	TextureHandle* pMetallic;
	TextureHandle* pRoughness;
};
class Mesh
{
public:
	Mesh() = default;
	~Mesh()
	{
		if (Vertex.pBuffer)
		{
			Vertex.pBuffer->Release();
			Vertex.pBuffer = nullptr;
		}
		if (Index.pBuffer)
		{
			Index.pBuffer->Release();
			Index.pBuffer = nullptr;
		}
	}

public:
	BufferInfo Vertex;
	BufferInfo Index;
	Material Material;

	MeshConstant MeshConstantData;
	MaterialConstant MaterialConstantData;

	bool bSkinnedMesh = false;
};
