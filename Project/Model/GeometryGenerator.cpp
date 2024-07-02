#include "../pch.h"
#include "ModelLoader.h"
#include "GeometryGenerator.h"

HRESULT ReadFromFile(std::vector<MeshInfo>& dst, std::wstring& basePath, std::wstring& fileName, bool bRevertNormals)
{
	HRESULT hr = S_OK;

	ModelLoader modelLoader;
	hr = modelLoader.Load(basePath, fileName, bRevertNormals);
	if (FAILED(hr))
	{
		goto LB_RET;
	}

	Normalize(Vector3(0.0f), 1.0f, modelLoader.MeshInfos, modelLoader.AnimData);
	dst = modelLoader.MeshInfos;

LB_RET:
	return hr;
}

HRESULT ReadAnimationFromFile(std::vector<MeshInfo>& meshInfos, AnimationData& animData, std::wstring& basePath, std::wstring& fileName, bool bRevertNormals)
{
	HRESULT hr = S_OK;

	ModelLoader modelLoader;
	hr = modelLoader.Load(basePath, fileName, bRevertNormals);
	if (FAILED(hr))
	{
		goto LB_RET;
	}

	Normalize(Vector3(0.0f), 1.0f, modelLoader.MeshInfos, modelLoader.AnimData);
	meshInfos = modelLoader.MeshInfos;
	animData = modelLoader.AnimData;

LB_RET:
	return hr;
}

void Normalize(const Vector3 CENTER, const float LONGEST_LENGTH, std::vector<MeshInfo>& meshes, AnimationData& animData)
{
	// 모델의 중심을 원점으로 옮기고 크기를 [-1,1]^3으로 스케일 -> 박스 형태로.
	using namespace DirectX;

	// Normalize vertices
	Vector3 vmin(1000, 1000, 1000);
	Vector3 vmax(-1000, -1000, -1000);
	for (size_t i = 0, totalMesh = meshes.size(); i < totalMesh; ++i)
	{
		MeshInfo& curMesh = meshes[i];
		for (size_t j = 0, vertSize = curMesh.Vertices.size(); j < vertSize; ++j)
		{
			Vertex& v = curMesh.Vertices[j];
			vmin.x = XMMin(vmin.x, v.Position.x);
			vmin.y = XMMin(vmin.y, v.Position.y);
			vmin.z = XMMin(vmin.z, v.Position.z);
			vmax.x = XMMax(vmax.x, v.Position.x);
			vmax.y = XMMax(vmax.y, v.Position.y);
			vmax.z = XMMax(vmax.z, v.Position.z);
		}
	}

	float dx = vmax.x - vmin.x, dy = vmax.y - vmin.y, dz = vmax.z - vmin.z;
	float scale = LONGEST_LENGTH / XMMax(XMMax(dx, dy), dz);
	Vector3 translation = -(vmin + vmax) * 0.5f + CENTER;

	for (size_t i = 0, totalMesh = meshes.size(); i < totalMesh; ++i)
	{
		MeshInfo& curMesh = meshes[i];
		for (size_t j = 0, vertSize = curMesh.Vertices.size(); j < vertSize; ++j)
		{
			Vertex& v = curMesh.Vertices[j];
			v.Position = (v.Position + translation) * scale;
		}
		for (size_t j = 0, skinnedVertSize = curMesh.SkinnedVertices.size(); j < skinnedVertSize; ++j)
		{
			SkinnedVertex& v = curMesh.SkinnedVertices[j];
			v.Position = (v.Position + translation) * scale;
		}
	}

	// 애니메이션 데이터 보정에 사용.
	animData.DefaultTransform = Matrix::CreateTranslation(translation) * Matrix::CreateScale(scale);
}

void MakeSquare(MeshInfo* pDst, const float SCALE, const Vector2 TEX_SCALE)
{
	_ASSERT(pDst);

	pDst->Vertices.resize(4);

	pDst->Vertices[0].Position = Vector3(-1.0f, 1.0f, 0.0f) * SCALE;
	pDst->Vertices[0].Normal = Vector3(0.0f, 0.0f, -1.0f);
	pDst->Vertices[0].Texcoord = Vector2(0.0f, 0.0f) * TEX_SCALE;
	pDst->Vertices[0].Tangent = Vector3(1.0f, 0.0f, 0.0f);

	pDst->Vertices[1].Position = Vector3(1.0f, 1.0f, 0.0f) * SCALE;
	pDst->Vertices[1].Normal = Vector3(0.0f, 0.0f, -1.0f);
	pDst->Vertices[1].Texcoord = Vector2(1.0f, 0.0f) * TEX_SCALE;
	pDst->Vertices[1].Tangent = Vector3(1.0f, 0.0f, 0.0f);

	pDst->Vertices[2].Position = Vector3(1.0f, -1.0f, 0.0f) * SCALE;
	pDst->Vertices[2].Normal = Vector3(0.0f, 0.0f, -1.0f);
	pDst->Vertices[2].Texcoord = Vector2(1.0f, 1.0f) * TEX_SCALE;
	pDst->Vertices[2].Tangent = Vector3(1.0f, 0.0f, 0.0f);

	pDst->Vertices[3].Position = Vector3(-1.0f, -1.0f, 0.0f) * SCALE;
	pDst->Vertices[3].Normal = Vector3(0.0f, 0.0f, -1.0f);
	pDst->Vertices[3].Texcoord = Vector2(0.0f, 1.0f) * TEX_SCALE;
	pDst->Vertices[3].Tangent = Vector3(1.0f, 0.0f, 0.0f);

	pDst->Indices = { 0, 1, 2, 0, 2, 3, };
}

void MakeSquareGrid(MeshInfo* pDst, const int NUM_SLICES, const int NUM_STACKS, const float SCALE, const Vector2 TEX_SCALE)
{
	_ASSERT(pDst);

	pDst->Vertices.resize((NUM_STACKS + 1) * (NUM_SLICES + 1));
	pDst->Indices.reserve(NUM_STACKS * NUM_SLICES * 6);

	float dx = 2.0f / NUM_SLICES;
	float dy = 2.0f / NUM_STACKS;

	float y = 1.0f;
	for (int j = 0; j < NUM_STACKS + 1; ++j)
	{
		float x = -1.0f;
		for (int i = 0; i < NUM_SLICES + 1; ++i)
		{
			Vertex& v = pDst->Vertices[j * (NUM_SLICES + 1) + i];
			v.Position = Vector3(x, y, 0.0f) * SCALE;
			v.Normal = Vector3(0.0f, 0.0f, -1.0f);
			v.Texcoord = Vector2(x + 1.0f, y + 1.0f) * 0.5f * TEX_SCALE;
			v.Tangent = Vector3(1.0f, 0.0f, 0.0f);

			x += dx;
		}
		y -= dy;
	}

	for (int j = 0; j < NUM_STACKS; ++j)
	{
		for (int i = 0; i < NUM_SLICES; ++i)
		{
			pDst->Indices.push_back((NUM_SLICES + 1) * j + i);
			pDst->Indices.push_back((NUM_SLICES + 1) * j + i + 1);
			pDst->Indices.push_back((NUM_SLICES + 1) * (j + 1) + i);

			pDst->Indices.push_back((NUM_SLICES + 1) * (j + 1) + i);
			pDst->Indices.push_back((NUM_SLICES + 1) * j + i + 1);
			pDst->Indices.push_back((NUM_SLICES + 1) * (j + 1) + i + 1);
		}
	}
}

void MakeGrass(MeshInfo* pDst)
{
	_ASSERT(pDst);

	MakeSquareGrid(pDst, 1, 4);

	for (size_t i = 0, size = pDst->Vertices.size(); i < size; ++i)
	{
		Vertex& v = pDst->Vertices[i];

		// 적당히 가늘게 조절.
		v.Position.x *= 0.02f;

		// Y범위를 0~1로 조절.
		v.Position.y = v.Position.y * 0.5f + 0.5f;
	}

	// 맨 위를 뾰족하게 만들기 위해 삼각형 하나와 정점 하나 삭제.
	pDst->Indices.erase(pDst->Indices.begin(), pDst->Indices.begin() + 3);
	for (size_t i = 0, size = pDst->Indices.size(); i < size; ++i)
	{
		uint32_t& index = pDst->Indices[i];
		index -= 1;
	}
	pDst->Vertices.erase(pDst->Vertices.begin());
	pDst->Vertices[0].Position.x = 0.0f;
	pDst->Vertices[0].Texcoord.x = 0.5f;
}

void MakeBox(MeshInfo* pDst, const float SCALE)
{
	_ASSERT(pDst);

	pDst->Vertices.resize(24);

	// 윗면
	pDst->Vertices[0].Position = Vector3(-1.0f, 1.0f, -1.0f) * SCALE;
	pDst->Vertices[0].Normal = Vector3(0.0f, 1.0f, 0.0f);
	pDst->Vertices[0].Texcoord = Vector2(0.0f, 0.0f);

	pDst->Vertices[1].Position = Vector3(-1.0f, 1.0f, 1.0f) * SCALE;
	pDst->Vertices[1].Normal = Vector3(0.0f, 1.0f, 0.0f);
	pDst->Vertices[1].Texcoord = Vector2(1.0f, 0.0f);

	pDst->Vertices[2].Position = Vector3(1.0f, 1.0f, 1.0f) * SCALE;
	pDst->Vertices[2].Normal = Vector3(0.0f, 1.0f, 0.0f);
	pDst->Vertices[2].Texcoord = Vector2(1.0f, 1.0f);

	pDst->Vertices[3].Position = Vector3(1.0f, 1.0f, -1.0f) * SCALE;
	pDst->Vertices[3].Normal = Vector3(0.0f, 1.0f, 0.0f);
	pDst->Vertices[3].Texcoord = Vector2(0.0f, 1.0f);

	// 아랫면
	pDst->Vertices[4].Position = Vector3(-1.0f, -1.0f, -1.0f) * SCALE;
	pDst->Vertices[4].Normal = Vector3(0.0f, -1.0f, 0.0f);
	pDst->Vertices[4].Texcoord = Vector2(0.0f, 0.0f);

	pDst->Vertices[5].Position = Vector3(1.0f, -1.0f, -1.0f) * SCALE;
	pDst->Vertices[5].Normal = Vector3(0.0f, -1.0f, 0.0f);
	pDst->Vertices[5].Texcoord = Vector2(1.0f, 0.0f);

	pDst->Vertices[6].Position = Vector3(1.0f, -1.0f, 1.0f) * SCALE;
	pDst->Vertices[6].Normal = Vector3(0.0f, -1.0f, 0.0f);
	pDst->Vertices[6].Texcoord = Vector2(1.0f, 1.0f);

	pDst->Vertices[7].Position = Vector3(-1.0f, -1.0f, 1.0f) * SCALE;
	pDst->Vertices[7].Normal = Vector3(0.0f, -1.0f, 0.0f);
	pDst->Vertices[7].Texcoord = Vector2(0.0f, 1.0f);

	// 앞면
	pDst->Vertices[8].Position = Vector3(-1.0f, -1.0f, -1.0f) * SCALE;
	pDst->Vertices[8].Normal = Vector3(0.0f, 0.0f, -1.0f);
	pDst->Vertices[8].Texcoord = Vector2(0.0f, 0.0f);

	pDst->Vertices[9].Position = Vector3(-1.0f, 1.0f, -1.0f) * SCALE;
	pDst->Vertices[9].Normal = Vector3(0.0f, 0.0f, -1.0f);
	pDst->Vertices[9].Texcoord = Vector2(1.0f, 0.0f);

	pDst->Vertices[10].Position = Vector3(1.0f, 1.0f, -1.0f) * SCALE;
	pDst->Vertices[10].Normal = Vector3(0.0f, 0.0f, -1.0f);
	pDst->Vertices[10].Texcoord = Vector2(1.0f, 1.0f);

	pDst->Vertices[11].Position = Vector3(1.0f, -1.0f, -1.0f) * SCALE;
	pDst->Vertices[11].Normal = Vector3(0.0f, 0.0f, -1.0f);
	pDst->Vertices[11].Texcoord = Vector2(0.0f, 1.0f);

	// 뒷면
	pDst->Vertices[12].Position = Vector3(-1.0f, -1.0f, 1.0f) * SCALE;
	pDst->Vertices[12].Normal = Vector3(0.0f, 0.0f, 1.0f);
	pDst->Vertices[12].Texcoord = Vector2(0.0f, 0.0f);

	pDst->Vertices[13].Position = Vector3(1.0f, -1.0f, 1.0f) * SCALE;
	pDst->Vertices[13].Normal = Vector3(0.0f, 0.0f, 1.0f);
	pDst->Vertices[13].Texcoord = Vector2(1.0f, 0.0f);

	pDst->Vertices[14].Position = Vector3(1.0f, 1.0f, 1.0f) * SCALE;
	pDst->Vertices[14].Normal = Vector3(0.0f, 0.0f, 1.0f);
	pDst->Vertices[14].Texcoord = Vector2(1.0f, 1.0f);

	pDst->Vertices[15].Position = Vector3(-1.0f, 1.0f, 1.0f) * SCALE;
	pDst->Vertices[15].Normal = Vector3(0.0f, 0.0f, 1.0f);
	pDst->Vertices[15].Texcoord = Vector2(0.0f, 1.0f);

	// 왼쪽
	pDst->Vertices[16].Position = Vector3(-1.0f, -1.0f, 1.0f) * SCALE;
	pDst->Vertices[16].Normal = Vector3(-1.0f, 0.0f, 0.0f);
	pDst->Vertices[16].Texcoord = Vector2(0.0f, 0.0f);

	pDst->Vertices[17].Position = Vector3(-1.0f, 1.0f, 1.0f) * SCALE;
	pDst->Vertices[17].Normal = Vector3(-1.0f, 0.0f, 0.0f);
	pDst->Vertices[17].Texcoord = Vector2(1.0f, 0.0f);

	pDst->Vertices[18].Position = Vector3(-1.0f, 1.0f, -1.0f) * SCALE;
	pDst->Vertices[18].Normal = Vector3(-1.0f, 0.0f, 0.0f);
	pDst->Vertices[18].Texcoord = Vector2(1.0f, 1.0f);

	pDst->Vertices[19].Position = Vector3(-1.0f, -1.0f, -1.0f) * SCALE;
	pDst->Vertices[19].Normal = Vector3(-1.0f, 0.0f, 0.0f);
	pDst->Vertices[19].Texcoord = Vector2(0.0f, 1.0f);

	// 오른쪽
	pDst->Vertices[20].Position = Vector3(1.0f, -1.0f, 1.0f) * SCALE;
	pDst->Vertices[20].Normal = Vector3(1.0f, 0.0f, 0.0f);
	pDst->Vertices[20].Texcoord = Vector2(0.0f, 0.0f);

	pDst->Vertices[21].Position = Vector3(1.0f, -1.0f, -1.0f) * SCALE;
	pDst->Vertices[21].Normal = Vector3(1.0f, 0.0f, 0.0f);
	pDst->Vertices[21].Texcoord = Vector2(1.0f, 0.0f);

	pDst->Vertices[22].Position = Vector3(1.0f, 1.0f, -1.0f) * SCALE;
	pDst->Vertices[22].Normal = Vector3(1.0f, 0.0f, 0.0f);
	pDst->Vertices[22].Texcoord = Vector2(1.0f, 1.0f);

	pDst->Vertices[23].Position = Vector3(1.0f, 1.0f, 1.0f) * SCALE;
	pDst->Vertices[23].Normal = Vector3(1.0f, 0.0f, 0.0f);
	pDst->Vertices[23].Texcoord = Vector2(0.0f, 1.0f);

	pDst->Indices =
	{
		0,  1,  2,  0,  2,  3,  // 윗면
		4,  5,  6,  4,  6,  7,  // 아랫면
		8,  9,  10, 8,  10, 11, // 앞면
		12, 13, 14, 12, 14, 15, // 뒷면
		16, 17, 18, 16, 18, 19, // 왼쪽
		20, 21, 22, 20, 22, 23  // 오른쪽
	};
}

void MakeWireBox(MeshInfo* pDst, const Vector3 CENTER, const Vector3 EXTENTS)
{
	// 상자를 와이어 프레임으로 그리는 용도.

	_ASSERT(pDst);

	pDst->Vertices.resize(8);

	// 앞면
	pDst->Vertices[0].Position = CENTER + Vector3(-1.0f, -1.0f, -1.0f) * EXTENTS;
	pDst->Vertices[0].Normal = pDst->Vertices[0].Position - CENTER;
	pDst->Vertices[0].Normal.Normalize();
	pDst->Vertices[0].Texcoord = Vector2(0.0f);

	pDst->Vertices[1].Position = CENTER + Vector3(-1.0f, 1.0f, -1.0f) * EXTENTS;
	pDst->Vertices[1].Normal = pDst->Vertices[1].Position - CENTER;
	pDst->Vertices[1].Normal.Normalize();
	pDst->Vertices[1].Texcoord = Vector2(0.0f);

	pDst->Vertices[2].Position = CENTER + Vector3(1.0f, 1.0f, -1.0f) * EXTENTS;
	pDst->Vertices[2].Normal = pDst->Vertices[2].Position - CENTER;
	pDst->Vertices[2].Normal.Normalize();
	pDst->Vertices[2].Normal.Normalize();

	pDst->Vertices[3].Position = CENTER + Vector3(1.0f, -1.0f, -1.0f) * EXTENTS;
	pDst->Vertices[3].Normal = pDst->Vertices[3].Position - CENTER;
	pDst->Vertices[3].Normal.Normalize();
	pDst->Vertices[3].Texcoord = Vector2(0.0f);

	// 뒷면
	pDst->Vertices[4].Position = CENTER + Vector3(-1.0f, -1.0f, 1.0f) * EXTENTS;
	pDst->Vertices[4].Normal = pDst->Vertices[4].Position - CENTER;
	pDst->Vertices[4].Normal.Normalize();
	pDst->Vertices[4].Texcoord = Vector2(0.0f);

	pDst->Vertices[5].Position = CENTER + Vector3(-1.0f, 1.0f, 1.0f) * EXTENTS;
	pDst->Vertices[5].Normal = pDst->Vertices[5].Position - CENTER;
	pDst->Vertices[5].Normal.Normalize();
	pDst->Vertices[5].Texcoord = Vector2(0.0f);

	pDst->Vertices[6].Position = CENTER + Vector3(1.0f, 1.0f, 1.0f) * EXTENTS;
	pDst->Vertices[6].Normal = pDst->Vertices[6].Position - CENTER;
	pDst->Vertices[6].Normal.Normalize();
	pDst->Vertices[6].Texcoord = Vector2(0.0f);

	pDst->Vertices[7].Position = CENTER + Vector3(1.0f, -1.0f, 1.0f) * EXTENTS;
	pDst->Vertices[7].Normal = pDst->Vertices[7].Position - CENTER;
	pDst->Vertices[7].Normal.Normalize();
	pDst->Vertices[7].Texcoord = Vector2(0.0f);

	// Line list.
	pDst->Indices =
	{
		0, 1, 1, 2, 2, 3, 3, 0, // 앞면
		4, 5, 5, 6, 6, 7, 7, 4, // 뒷면
		0, 4, 1, 5, 2, 6, 3, 7  // 옆면
	};
}

void MakeWireSphere(MeshInfo* pDst, const Vector3 CENTER, const float RADIUS)
{
	_ASSERT(pDst);

	std::vector<Vertex>& vertices = pDst->Vertices;
	std::vector<UINT>& indices = pDst->Indices;

	const int NUM_POINT = 30;
	const float D_THETA = DirectX::XM_2PI / (float)NUM_POINT;

	// XY plane
	{
		int offset = (int)(vertices.size());
		Vector3 start(1.0f, 0.0f, 0.0f);
		for (int i = 0; i < NUM_POINT; ++i)
		{
			Vertex v;
			v.Position = CENTER + Vector3::Transform(start, Matrix::CreateRotationZ(D_THETA * (float)i)) * RADIUS;
			vertices.push_back(v);
			indices.push_back(i + offset);
			if (i != 0)
			{
				indices.push_back(i + offset);
			}
		}
		indices.push_back(offset);
	}

	// YZ
	{
		int offset = (int)(vertices.size());
		Vector3 start(0.0f, 1.0f, 0.0f);
		for (int i = 0; i < NUM_POINT; ++i)
		{
			Vertex v;
			v.Position = CENTER + Vector3::Transform(start, Matrix::CreateRotationX(D_THETA * (float)i)) * RADIUS;
			vertices.push_back(v);
			indices.push_back(i + offset);
			if (i != 0)
			{
				indices.push_back(i + offset);
			}
		}
		indices.push_back(offset);
	}

	// XZ
	{
		int offset = (int)(vertices.size());
		Vector3 start(1.0f, 0.0f, 0.0f);
		for (int i = 0; i < NUM_POINT; ++i)
		{
			Vertex v;
			v.Position = CENTER + Vector3::Transform(start, Matrix::CreateRotationY(D_THETA * (float)i)) * RADIUS;
			vertices.push_back(v);
			indices.push_back(i + offset);
			if (i != 0)
			{
				indices.push_back(i + offset);
			}
		}
		indices.push_back(offset);
	}
}

void MakeCylinder(MeshInfo* pDst, const float BOTTOM_RADIUS, const float TOP_RADIUS, float height, int numSlices)
{
	_ASSERT(pDst);

	// Texture 좌표계때문에 (numSlices + 1) x 2 개의 버텍스 사용.

	const float D_THETA = -DirectX::XM_2PI / (float)numSlices;

	std::vector<Vertex>& vertices = pDst->Vertices;
	std::vector<uint32_t>& indices = pDst->Indices;
	vertices.resize(numSlices * numSlices);
	indices.reserve(numSlices * 6);

	// 옆면의 바닥 버텍스들 (인덱스 0 이상 numSlices 미만).
	for (int i = 0; i <= numSlices; ++i)
	{
		Vertex& v = vertices[i];
		v.Position = Vector3::Transform(Vector3(BOTTOM_RADIUS, -0.5f * height, 0.0f), Matrix::CreateRotationY(D_THETA * (float)i));
		v.Normal = v.Position - Vector3(0.0f, -0.5f * height, 0.0f);
		v.Normal.Normalize();
		v.Texcoord = Vector2(float(i) / numSlices, 1.0f);
	}

	// 옆면의 맨 위 버텍스들 (인덱스 numSlices 이상 2 * numSlices 미만).
	for (int i = 0; i <= numSlices; ++i)
	{
		Vertex& v = vertices[numSlices + i];
		v.Position = Vector3::Transform(Vector3(TOP_RADIUS, 0.5f * height, 0.0f), Matrix::CreateRotationY(D_THETA * (float)i));
		v.Normal = v.Position - Vector3(0.0f, 0.5f * height, 0.0f);
		v.Normal.Normalize();
		v.Texcoord = Vector2((float)i / numSlices, 0.0f);
	}

	for (int i = 0; i < numSlices; ++i)
	{
		indices.push_back(i);
		indices.push_back(i + numSlices + 1);
		indices.push_back(i + 1 + numSlices + 1);

		indices.push_back(i);
		indices.push_back(i + 1 + numSlices + 1);
		indices.push_back(i + 1);
	}
}

void MakeSphere(MeshInfo* pDst, const float RADIUS, const int NUM_SLICES, const int NUM_STACKS, const Vector2 TEX_SCALE)
{
	_ASSERT(pDst);

	const float D_THETA = -DirectX::XM_2PI / (float)NUM_SLICES;
	const float D_PHI = -DirectX::XM_PI / (float)NUM_STACKS;

	std::vector<Vertex>& vertices = pDst->Vertices;
	std::vector<uint32_t>& indices = pDst->Indices;
	vertices.resize((NUM_STACKS + 1) * (NUM_SLICES + 1));
	indices.reserve(NUM_SLICES * NUM_STACKS * 6);

	for (int j = 0; j <= NUM_STACKS; ++j)
	{
		// 스택에 쌓일 수록 시작점을 x-y 평면에서 회전 시켜서 위로 올리는 구조
		Vector3 stackStartPoint = Vector3::Transform(Vector3(0.0f, -RADIUS, 0.0f), Matrix::CreateRotationZ(D_PHI * j));

		for (int i = 0; i <= NUM_SLICES; ++i)
		{
			Vertex& v = vertices[j * (NUM_SLICES + 1) + i];

			// 시작점을 x-z 평면에서 회전시키면서 원을 만드는 구조.
			v.Position = Vector3::Transform(stackStartPoint, Matrix::CreateRotationY(D_THETA * float(i)));

			v.Normal = v.Position; // 원점이 구의 중심.
			v.Normal.Normalize();

			v.Texcoord = Vector2(float(i) / NUM_SLICES, 1.0f - float(j) / NUM_STACKS) * TEX_SCALE;

			// Texcoord가 위로 갈수록 증가.
			Vector3 biTangent = Vector3(0.0f, 1.0f, 0.0f);
			Vector3 normalOrth = v.Normal - biTangent.Dot(v.Normal) * v.Normal;
			normalOrth.Normalize();

			v.Tangent = biTangent.Cross(normalOrth);
			v.Tangent.Normalize();
		}
	}

	for (int j = 0; j < NUM_STACKS; ++j)
	{
		const int OFFSET = (NUM_SLICES + 1) * j;

		for (int i = 0; i < NUM_SLICES; ++i)
		{
			indices.push_back(OFFSET + i);
			indices.push_back(OFFSET + i + NUM_SLICES + 1);
			indices.push_back(OFFSET + i + 1 + NUM_SLICES + 1);

			indices.push_back(OFFSET + i);
			indices.push_back(OFFSET + i + 1 + NUM_SLICES + 1);
			indices.push_back(OFFSET + i + 1);
		}
	}
}

void MakeTetrahedron(MeshInfo* pDst)
{
	_ASSERT(pDst);

	pDst->Vertices.resize(4);

	const float A = 1.0f;
	const float X = sqrt(3.0f) / 3.0f * A;
	const float D = sqrt(3.0f) / 6.0f * A; // = x / 2
	const float H = sqrt(6.0f) / 3.0f * A;

	Vector3 points[] =
	{
		{ 0.0f, X, 0.0f },
		{ -0.5f * A, -D, 0.0f },
		{ 0.5f * A, -D, 0.0f },
		{ 0.0f, 0.0f, H }
	};
	Vector3 center = Vector3(0.0f);

	for (int i = 0; i < 4; ++i)
	{
		center += points[i];
	}
	center /= 4.0f;
	for (int i = 0; i < 4; ++i)
	{
		points[i] -= center;
	}

	for (int i = 0; i < 4; ++i)
	{
		Vertex& v = pDst->Vertices[i];
		v.Position = points[i];
		v.Normal = v.Position; // 중심이 원점.
		v.Normal.Normalize();
	}

	pDst->Indices = { 0, 1, 2, 3, 2, 1, 0, 3, 1, 0, 2, 3 };
}

void MakeIcosahedron(MeshInfo* pDst)
{
	_ASSERT(pDst);

	const float X = 0.525731f;
	const float Z = 0.850651f;

	pDst->Vertices.resize(12);

	Vector3 pos[] =
	{
		Vector3(-X, 0.0f, Z), Vector3(X, 0.0f, Z),   Vector3(-X, 0.0f, -Z),
		Vector3(X, 0.0f, -Z), Vector3(0.0f, Z, X),   Vector3(0.0f, Z, -X),
		Vector3(0.0f, -Z, X), Vector3(0.0f, -Z, -X), Vector3(Z, X, 0.0f),
		Vector3(-Z, X, 0.0f), Vector3(Z, -X, 0.0f),  Vector3(-Z, -X, 0.0f)
	};
	for (int i = 0; i < 12; ++i)
	{
		Vertex& v = pDst->Vertices[i];
		v.Position = pos[i];
		v.Normal = v.Position;
		v.Normal.Normalize();
	}

	pDst->Indices =
	{
		1,  4,  0, 4,  9, 0, 4, 5,  9, 8, 5, 4,  1,  8, 4,
		1,  10, 8, 10, 3, 8, 8, 3,  5, 3, 2, 5,  3,  7, 2,
		3,  10, 7, 10, 6, 7, 6, 11, 7, 6, 0, 11, 6,  1, 0,
		10, 1,  6, 11, 0, 9, 2, 11, 9, 5, 2, 9,  11, 2, 7
	};
}

void SubdivideToSphere(MeshInfo* pDst, const float RADIUS, MeshInfo& meshData)
{
	using namespace DirectX;
	using DirectX::SimpleMath::Matrix;
	using DirectX::SimpleMath::Vector3;

	_ASSERT(pDst);

	// 원점이 중심이라고 가정.
	for (auto& v : meshData.Vertices)
	{
		v.Position = v.Normal * RADIUS;
	}

	// 구의 표면으로 옮기고 노멀과 texture 좌표 계산.
	auto ProjectVertex = [&](Vertex& v)
		{
			v.Normal = v.Position;
			v.Normal.Normalize();
			v.Position = v.Normal * RADIUS;
		};

	auto UpdateFaceNormal = [](Vertex& v0, Vertex& v1, Vertex& v2)
		{
			Vector3 faceNormal = (v1.Position - v0.Position).Cross(v2.Position - v0.Position);
			faceNormal.Normalize();
			v0.Normal = faceNormal;
			v1.Normal = faceNormal;
			v2.Normal = faceNormal;
		};

	// 버텍스가 중복되는 구조로 구현.
	const size_t TOTAL_INDICES = meshData.Indices.size();
	uint32_t count = 0;
	pDst->Vertices.reserve(12 * TOTAL_INDICES);
	pDst->Indices.reserve(12 * TOTAL_INDICES);
	for (size_t i = 0; i < TOTAL_INDICES; i += 3)
	{
		size_t i0 = meshData.Indices[i];
		size_t i1 = meshData.Indices[i + 1];
		size_t i2 = meshData.Indices[i + 2];

		Vertex v0 = meshData.Vertices[i0];
		Vertex v1 = meshData.Vertices[i1];
		Vertex v2 = meshData.Vertices[i2];

		Vertex v3;
		v3.Position = (v0.Position + v2.Position) * 0.5f;
		v3.Texcoord = (v0.Texcoord + v2.Texcoord) * 0.5f;
		ProjectVertex(v3);

		Vertex v4;
		v4.Position = (v0.Position + v1.Position) * 0.5f;
		v4.Texcoord = (v0.Texcoord + v1.Texcoord) * 0.5f;
		ProjectVertex(v4);

		Vertex v5;
		v5.Position = (v1.Position + v2.Position) * 0.5f;
		v5.Texcoord = (v1.Texcoord + v2.Texcoord) * 0.5f;
		ProjectVertex(v5);

		// UpdateFaceNormal(v4, v1, v5);
		// UpdateFaceNormal(v0, v4, v3);
		// UpdateFaceNormal(v3, v4, v5);
		// UpdateFaceNormal(v3, v5, v2);

		pDst->Vertices.push_back(v4);
		pDst->Vertices.push_back(v1);
		pDst->Vertices.push_back(v5);

		pDst->Vertices.push_back(v0);
		pDst->Vertices.push_back(v4);
		pDst->Vertices.push_back(v3);

		pDst->Vertices.push_back(v3);
		pDst->Vertices.push_back(v4);
		pDst->Vertices.push_back(v5);

		pDst->Vertices.push_back(v3);
		pDst->Vertices.push_back(v5);
		pDst->Vertices.push_back(v2);

		for (uint32_t j = 0; j < 12; ++j)
		{
			pDst->Indices.push_back(j + count);
		}
		count += 12;
	}
}