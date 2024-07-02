#pragma once

#include "AnimationData.h"
#include "../Util/LinkedList.h"
#include "Mesh.h"
#include "MeshInfo.h"

using DirectX::SimpleMath::Matrix;

class Model
{
public:
	Model() = default;
	Model(ResourceManager* pManager, std::wstring& basePath, std::wstring& fileName);
	Model(ResourceManager* pManager, const std::vector<MeshInfo>& MESH_INFOS);
	virtual ~Model() { Clear(); }

	void Initialize(ResourceManager* pManager, std::wstring& basePath, std::wstring& fileName);
	void Initialize(ResourceManager* pManager, const std::vector<MeshInfo>& MESH_INFOS);
	virtual void Initialize(ResourceManager* pManager) { }
	virtual void InitMeshBuffers(ResourceManager* pManager, const MeshInfo& MESH_INFO, Mesh* pNewMesh);

	void UpdateConstantBuffers();
	void UpdateWorld(const Matrix& WORLD);
	virtual void UpdateAnimation(int clipID, int frame) { }

	virtual void Render(ResourceManager* pManager, ePipelineStateSetting psoSetting);
	virtual void Render(ResourceManager* pManager, ID3D12GraphicsCommandList* pCommandList, ePipelineStateSetting psoSetting);

	virtual void Clear();

	virtual void SetDescriptorHeap(ResourceManager* pManager);

public:
	Matrix World = Matrix();
	Matrix WorldInverseTranspose = Matrix();

	bool bIsVisible = true;
	bool bCastShadow = true;
	bool bIsPickable = false;

	std::vector<Mesh*> Meshes;

	DirectX::BoundingBox BoundingBox;
	DirectX::BoundingSphere BoundingSphere;

	std::string Name;

	ListElem LinkInGame;

protected:
	Mesh* m_pBoundingBoxMesh = nullptr;
	Mesh* m_pBoundingSphereMesh = nullptr;
};
