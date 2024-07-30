#pragma once

#include <physx/cooking/PxCooking.h>
#include "CollisionGroup.h"

class PhysicsManager
{
public:
	static physx::PxFilterFlags IgnoreCharacterControllerAndEndEffector(physx::PxFilterObjectAttributes attributes0,
																		physx::PxFilterData filterData0,
																		physx::PxFilterObjectAttributes attributes1,
																		physx::PxFilterData filterData1,
																		physx::PxPairFlags& pairFlags,
																		const void* constantBlock,
																		physx::PxU32 constantBlockSize);

	PhysicsManager() = default;
	~PhysicsManager() { Cleanup(); };

	void Initialize(UINT numThreads = 1);

	void Update(const float DELTA_TIME);

	void CookingStaticTriangleMesh(const std::vector<Vertex>* pVERTICES, const std::vector<UINT32>* pINDICES, const Matrix& WORLD);
	void AddActor(physx::PxRigidActor* pActor);

	void Cleanup();

	inline physx::PxPhysics* GetPhysics() { return m_pPhysics; }
	inline physx::PxScene* GetScene() { return m_pScene; }
	inline physx::PxControllerManager* GetControllerManager() { return m_pControllerManager; }

public:
	physx::PxMaterial* pCommonMaterial = nullptr;

private:
	const physx::PxVec3 m_GRAVITY = physx::PxVec3(0.0f, -9.81f, 0.0f);

	physx::PxDefaultAllocator m_Allocator;
	physx::PxDefaultErrorCallback m_ErrorCallback;
	physx::PxFoundation* m_pFoundation = nullptr;
	physx::PxPhysics* m_pPhysics = nullptr;
	physx::PxScene* m_pScene = nullptr;
	physx::PxPvd* m_pPVD = nullptr;
	physx::PxTaskManager* m_pTaskManager = nullptr;
	physx::PxDefaultCpuDispatcher* m_pDispatcher = nullptr;
	physx::PxControllerManager* m_pControllerManager = nullptr;
};
