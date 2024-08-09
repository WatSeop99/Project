#pragma once

#include "Model.h"

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
	SkinnedMeshModel();
	~SkinnedMeshModel() { Cleanup(); }

	void Initialize(Renderer* pRenderer, const std::vector<MeshInfo>& MESH_INFOS, const AnimationData& ANIM_DATA);
	void InitMeshBuffers(Renderer* pRenderer, const MeshInfo& MESH_INFO, Mesh* pNewMesh) override;
	void InitMeshBuffers(Renderer* pRenderer, const MeshInfo& MESH_INFO, Mesh** ppNewMesh);
	void InitAnimationData(Renderer* pRenderer, const AnimationData& ANIM_DATA);

	void UpdateWorld(const Matrix& WORLD) override;
	void UpdateAnimation(const int CLIP_ID, const int FRAME, const float DELTA_TIME, JointUpdateInfo* pUpdateInfo);
	
	void Render(eRenderPSOType psoSetting) override;
	void Render(UINT threadIndex, ID3D12GraphicsCommandList* pCommandList, DynamicDescriptorPool* pDescriptorPool, ConstantBufferManager* pConstantBufferManager, ResourceManager* pManager, int psoSetting) override;
	void RenderBoundingCapsule(eRenderPSOType psoSetting);
	void RenderJointSphere(eRenderPSOType psoSetting);

	void Cleanup();

	inline Mesh** GetRightArmsMesh() { return m_ppRightArm; }
	inline Mesh** GetLeftArmsMesh() { return m_ppLeftArm; }
	inline Mesh** GetRightLegsMesh() { return m_ppRightLeg; }
	inline Mesh** GetLeftLegsMesh() { return m_ppLeftLeg; }

protected:
	void initBoundingCapsule();
	void initJointSpheres();
	void initChain();

	void updateChainPosition(const int CLIP_ID, const int FRAME);
	void updateJointSpheres(const int CLIP_ID, const int FRAME);

	void solveCharacterIK(const int CLIP_ID, const int FRAME, const float DELTA_TIME, JointUpdateInfo* pUpdateInfo);

public:
	TextureHandle* pBoneTransform = nullptr;
	AnimationData CharacterAnimationData;

	DirectX::BoundingSphere RightHandMiddle;
	DirectX::BoundingSphere LeftHandMiddle;
	DirectX::BoundingSphere RightToe;
	DirectX::BoundingSphere LeftToe;

	Chain RightArm;
	Chain LeftArm;
	Chain RightLeg;
	Chain LeftLeg;

	physx::PxCapsuleController* pController = nullptr;
	physx::PxRigidDynamic* pRightFoot = nullptr;
	physx::PxRigidDynamic* pLeftFoot = nullptr;

private:
	Mesh* m_ppRightArm[4] = { nullptr, }; // right arm - right fore arm - right hand - right hand middle.
	Mesh* m_ppLeftArm[4] = { nullptr, }; // left arm - left fore arm - left hand - left hand middle.
	Mesh* m_ppRightLeg[4] = { nullptr, }; // right up leg - right leg - right foot - right toe.
	Mesh* m_ppLeftLeg[4] = { nullptr, }; // left up leg - left leg - left foot - left toe.
	Mesh* m_pBoundingCapsuleMesh = nullptr;
	Mesh* m_pTargetPos1 = nullptr; // for right foot.
	Mesh* m_pTargetPos2 = nullptr; // for left foot.
};
