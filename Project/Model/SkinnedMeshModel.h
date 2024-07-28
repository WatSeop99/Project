#pragma once

#include "Model.h"
#include "../Graphics/Texture.h"

class SkinnedMeshModel final : public Model
{
public:
	enum eJointPart
	{
		JointPart_RightArm = 0,
		JointPart_LeftArm,
		JointPart_RightLeg,
		JointPart_LeftLeg,
		JointPart_TotalJointParts
	};
	struct JointUpdateInfo
	{
		Vector3 EndEffectorTargetPoses[JointPart_TotalJointParts];
		bool bUpdatedJointParts[JointPart_TotalJointParts];
	};

public:
	SkinnedMeshModel(Renderer* pRenderer, const std::vector<MeshInfo>& MESHES, const AnimationData& ANIM_DATA);
	~SkinnedMeshModel() { Cleanup(); }

	void Initialize(Renderer* pRenderer, const std::vector<MeshInfo>& MESH_INFOS, const AnimationData& ANIM_DATA);
	void InitMeshBuffers(Renderer* pRenderer, const MeshInfo& MESH_INFO, Mesh* pNewMesh) override;
	void InitMeshBuffers(Renderer* pRenderer, const MeshInfo& MESH_INFO, Mesh** ppNewMesh);
	void InitAnimationData(Renderer* pRenderer, const AnimationData& ANIM_DATA);

	void UpdateAnimation(int clipID, int frame, const float DELTA_TIME, JointUpdateInfo* pUpdateInfo);
	void UpdateCharacterIK(Vector3& target, int chainPart, int clipID, int frame, const float DELTA_TIME);

	void Render(eRenderPSOType psoSetting) override;
	void Render(UINT threadIndex, ID3D12GraphicsCommandList* pCommandList, DynamicDescriptorPool* pDescriptorPool, ConstantBufferManager* pConstantBufferManager, ResourceManager* pManager, int psoSetting) override;
	void RenderBoundingCapsule(eRenderPSOType psoSetting);
	void RenderJointSphere(eRenderPSOType psoSetting);

	void Cleanup();

	inline Mesh** GetRightArmsMesh() { return m_ppRightArm; }
	inline Mesh** GetLeftArmsMesh() { return m_ppLeftArm; }
	inline Mesh** GetRightLegsMesh() { return m_ppRightLeg; }
	inline Mesh** GetLeftLegsMesh() { return m_ppLeftLeg; }

	void SetDescriptorHeap(Renderer* pRenderer) override;

protected:
	void initBoundingCapsule();
	void initJointSpheres();
	void initChain();

	void updateChainPosition();
	void updateJointSpheres(int clipID, int frame);

	void solveCharacterIK(int clipID, int frame, const float DELTA_TIME, JointUpdateInfo* pUpdateInfo);

public:
	NonImageTexture BoneTransforms;
	TextureHandle* pBoneTransform = nullptr;
	AnimationData CharacterAnimationData;
	// CharacterMoveInfo MoveInfo;

	DirectX::BoundingSphere RightHandMiddle;
	DirectX::BoundingSphere LeftHandMiddle;
	DirectX::BoundingSphere RightToe;
	DirectX::BoundingSphere LeftToe;

	Chain RightArm;
	Chain LeftArm;
	Chain RightLeg;
	Chain LeftLeg;

	physx::PxController* pController = nullptr;

private:
	Mesh* m_ppRightArm[4] = { nullptr, }; // right arm - right fore arm - right hand - right hand middle.
	Mesh* m_ppLeftArm[4] = { nullptr, }; // left arm - left fore arm - left hand - left hand middle.
	Mesh* m_ppRightLeg[4] = { nullptr, }; // right up leg - right leg - right foot - right toe.
	Mesh* m_ppLeftLeg[4] = { nullptr, }; // left up leg - left leg - left foot - left toe.
	Mesh* m_pBoundingCapsuleMesh = nullptr;

	const int TOTAL_JOINT_PART = 0;
};
