#pragma once

#include "AnimationData.h"
#include "../Util/LinkedList.h"
#include "Mesh.h"
#include "MeshInfo.h"
#include "../Renderer/ResourceManager.h"

using DirectX::SimpleMath::Matrix;

class Renderer;

class Model
{
public:
	Model() = default;
	virtual ~Model() { Cleanup(); }

	void Initialize(Renderer* pRenderer, std::wstring& basePath, std::wstring& fileName);
	void Initialize(Renderer* pRenderer, const std::vector<MeshInfo>& MESH_INFOS);
	virtual void Initialize(Renderer* pRenderer) { }
	virtual void InitMeshBuffers(Renderer* pRenderer, const MeshInfo& MESH_INFO, Mesh* pNewMesh);

	void UpdateWorld(const Matrix& WORLD);
	
	virtual void Render(eRenderPSOType psoSetting);
	virtual void Render(UINT threadIndex, ID3D12GraphicsCommandList* pCommandList, DynamicDescriptorPool* pDescriptorPool, ConstantBufferManager* pConstantBufferManager, ResourceManager* pManager, int psoSetting);
	void RenderBoundingBox(eRenderPSOType psoSetting);
	void RenderBoundingSphere(eRenderPSOType psoSetting);

	virtual void Cleanup();

protected:
	void initBoundingBox(const std::vector<MeshInfo>& MESH_INFOS);
	void initBoundingSphere(const std::vector<MeshInfo>& MESH_INFOS);

	DirectX::BoundingBox getBoundingBox(const std::vector<Vertex>& VERTICES);
	void extendBoundingBox(const DirectX::BoundingBox& SRC_BOX, DirectX::BoundingBox* pDestBox);

public:
	Matrix World;
	Matrix InverseWorldTranspose;

	std::vector<Mesh*> Meshes;

	DirectX::BoundingBox BoundingBox;
	DirectX::BoundingSphere BoundingSphere;

	std::string Name;

	eRenderObjectType ModelType = RenderObjectType_DefaultType;

	bool bIsVisible = true;
	bool bCastShadow = true;
	bool bIsPickable = false;

protected:
	Renderer* m_pRenderer = nullptr;

	Mesh* m_pBoundingBoxMesh = nullptr;
	Mesh* m_pBoundingSphereMesh = nullptr;
};
