#pragma once

#include <vector>
#include <string>
#include "Vertex.h"

struct MeshInfo
{
	std::vector<Vertex> Vertices;
	std::vector<SkinnedVertex> SkinnedVertices;
	std::vector<UINT> Indices;
	std::wstring szAlbedoTextureFileName;
	std::wstring szEmissiveTextureFileName;
	std::wstring szNormalTextureFileName;
	std::wstring szHeightTextureFileName;
	std::wstring szAOTextureFileName; // Ambient Occlusion
	std::wstring szMetallicTextureFileName;
	std::wstring szRoughnessTextureFileName;
	std::wstring szOpacityTextureFileName;
};

#define INIT_MESH_INFO							 \
	{											 \
		{}, {}, {},								 \
		L"",  L"", L"", L"", L"", L"", L"", L"", \
	}

static void ReleaseMeshInfo(MeshInfo** ppMeshInfo)
{
	_ASSERT(*ppMeshInfo);

	(*ppMeshInfo)->Vertices.clear();
	(*ppMeshInfo)->SkinnedVertices.clear();
	(*ppMeshInfo)->Indices.clear();

	free(*ppMeshInfo);
	*ppMeshInfo = nullptr;
}
