#pragma once

#include "../pch.h"
#include "../Graphics/ConstantBuffer.h"
#include "../Graphics/Texture.h"

struct Material
{
	Texture Albedo;
	Texture Emissive;
	Texture Normal;
	Texture Height;
	Texture AmbientOcclusion;
	Texture Metallic;
	Texture Roughness;
};
struct Mesh
{
	ID3D12Resource* pVertexBuffer = nullptr;
	ID3D12Resource* pIndexBuffer = nullptr;
	Material* pMaterialBuffer = nullptr;

	D3D12_VERTEX_BUFFER_VIEW VertexBufferView;
	D3D12_INDEX_BUFFER_VIEW IndexBufferView;

	UINT VertexCount;
	UINT IndexCount;

	ConstantBuffer MeshConstant;
	ConstantBuffer MaterialConstant;

	bool bSkinnedMesh;
};

#define INIT_MESH							\
	{										\
		nullptr, nullptr, nullptr,			\
		{ 0, }, { 0, },	0, 0,				\
		ConstantBuffer(), ConstantBuffer(),	\
		false								\
	}

#define INIT_MATERIAL																 \
	{																				 \
		Texture(), Texture(), Texture(), Texture(), Texture(), Texture(), Texture(), \
	}

static void ReleaseMesh(Mesh** ppMesh)
{
	_ASSERT(*ppMesh);

	if ((*ppMesh)->pMaterialBuffer)
	{
		delete (*ppMesh)->pMaterialBuffer;
		(*ppMesh)->pMaterialBuffer = nullptr;
	}
	SAFE_RELEASE((*ppMesh)->pVertexBuffer);
	SAFE_RELEASE((*ppMesh)->pIndexBuffer);

	(*ppMesh)->MeshConstant.Clear();
	(*ppMesh)->MaterialConstant.Clear();

	free(*ppMesh);
	*ppMesh = nullptr;
}
