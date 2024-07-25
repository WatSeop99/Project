#pragma once

#include "AnimationData.h"
#include "../Util/LinkedList.h"
#include "Mesh.h"
#include "MeshInfo.h"
#include "../Renderer/ResourceManager.h"

using DirectX::SimpleMath::Matrix;

class Model
{
public:
	Model() = default;
	Model(Renderer* pRenderer, std::wstring& basePath, std::wstring& fileName);
	Model(Renderer* pRenderer, const std::vector<MeshInfo>& MESH_INFOS);
	virtual ~Model() { Cleanup(); }

	void Initialize(Renderer* pRenderer, std::wstring& basePath, std::wstring& fileName);
	void Initialize(Renderer* pRenderer, const std::vector<MeshInfo>& MESH_INFOS);
	virtual void Initialize(Renderer* pRenderer) { }
	virtual void InitMeshBuffers(Renderer* pRenderer, const MeshInfo& MESH_INFO, Mesh* pNewMesh);

	virtual void UpdateConstantBuffers();
	void UpdateWorld(const Matrix& WORLD);
	virtual void UpdateAnimation(int clipID, int frame, const float DELTA_TIME) { }

	virtual void Render(Renderer* pRenderer, eRenderPSOType psoSetting);
	virtual void Render(UINT threadIndex, ID3D12GraphicsCommandList* pCommandList, DynamicDescriptorPool* pDescriptorPool, ResourceManager* pManager, int psoSetting);
	void RenderBoundingBox(Renderer* pRenderer, eRenderPSOType psoSetting);
	void RenderBoundingSphere(Renderer* pRenderer, eRenderPSOType psoSetting);

	virtual void Cleanup();

	virtual void SetDescriptorHeap(Renderer* pRenderer);

protected:
	void initBoundingBox(Renderer* pRenderer, const std::vector<MeshInfo>& MESH_INFOS);
	void initBoundingSphere(Renderer* pRenderer, const std::vector<MeshInfo>& MESH_INFOS);

	DirectX::BoundingBox getBoundingBox(const std::vector<Vertex>& VERTICES);
	void extendBoundingBox(const DirectX::BoundingBox& SRC_BOX, DirectX::BoundingBox* pDestBox);

public:
	Matrix World;
	Matrix WorldInverseTranspose;

	std::vector<Mesh*> Meshes;

	DirectX::BoundingBox BoundingBox;
	DirectX::BoundingSphere BoundingSphere;

	std::string Name;

	eRenderObjectType ModelType = RenderObjectType_DefaultType;

	bool bIsVisible = true;
	bool bCastShadow = true;
	bool bIsPickable = false;

	// ListElem LinkToList = { nullptr, nullptr, this };

protected:
	Mesh* m_pBoundingBoxMesh = nullptr;
	Mesh* m_pBoundingSphereMesh = nullptr;
};
