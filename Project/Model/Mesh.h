#pragma once

#include "../pch.h"
#include "../Renderer/TextureManager.h"

// Vertex and Index Info
struct BufferInfo
{
	ID3D12Resource* pBuffer;
	union
	{
		D3D12_VERTEX_BUFFER_VIEW VertexBufferView;
		D3D12_INDEX_BUFFER_VIEW IndexBufferView;
	};
	UINT Count;
};
struct Material
{
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

	void Initialize()
	{
		Vertex.pBuffer = nullptr;
		Vertex.Count = 0;
		Index.pBuffer = nullptr;
		Index.Count = 0;
	}

public:
	BufferInfo Vertex;
	BufferInfo Index;
	Material Material = { nullptr, };

	MeshConstant MeshConstantData;
	MaterialConstant MaterialConstantData;

	bool bSkinnedMesh = false;
};
