#include "../pch.h"
#include "PhysicsManager.h"

using namespace physx;

#define PVD_HOST "127.0.0.1"

void PhysicsManager::Initialize(UINT numThreads)
{
	_ASSERT(numThreads >= 1);

	m_pFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, m_Allocator, m_ErrorCallback);
	if (!m_pFoundation)
	{
		__debugbreak();
	}
	
#ifdef _DEBUG
	m_pPVD = PxCreatePvd(*m_pFoundation);
	if (!m_pPVD)
	{
		__debugbreak();
	}

	PxPvdTransport* pTransport = PxDefaultPvdSocketTransportCreate(PVD_HOST, 5425, 10);
	m_pPVD->connect(*pTransport, PxPvdInstrumentationFlag::eALL);
#endif

	m_pPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *m_pFoundation, PxTolerancesScale(), true, m_pPVD);
	if (!m_pPhysics)
	{
		__debugbreak();
	}

	PxSceneDesc sceneDesc(m_pPhysics->getTolerancesScale());
	sceneDesc.gravity = m_GRAVITY;

	m_pDispatcher = PxDefaultCpuDispatcherCreate(numThreads);
	if (!m_pDispatcher)
	{
		__debugbreak();
	}
	sceneDesc.cpuDispatcher = m_pDispatcher;
	sceneDesc.filterShader = PxDefaultSimulationFilterShader;

	m_pScene = m_pPhysics->createScene(sceneDesc);
	if (!m_pScene)
	{
		__debugbreak();
	}

#ifdef _DEBUG
	PxPvdSceneClient* pPVDClient = m_pScene->getScenePvdClient();
	if (pPVDClient)
	{
		pPVDClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, false);
		pPVDClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONTACTS, false);
		pPVDClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, false);
	}
	else
	{
		__debugbreak();
	}
#endif

	m_pTaskManager = PxTaskManager::createTaskManager(m_pFoundation->getErrorCallback(), m_pDispatcher);
	if (!m_pTaskManager)
	{
		__debugbreak();
	}

	m_pControllerManager = PxCreateControllerManager(*m_pScene);
	if (!m_pControllerManager)
	{
		__debugbreak();
	}

	pCommonMaterial = m_pPhysics->createMaterial(0.5f, 0.5f, 0.6f); // (¸¶Âû·Â, dynamic ¸¶Âû·Â, Åº¼º·Â)

	// left-hand coord <==> model coord.
	PxVec4 col0(0.0f, 1.0f, 0.0f, 0.0f);
	PxVec4 col1(-1.0f, 0.0f, 0.0f, 0.0f);
	PxVec4 col2(0.0f, 0.0f, 1.0f, 0.0f);
	PxVec4 col3(0.0f, 0.0f, 0.0f, 1.0f);
	WorldToModel = PxTransform(PxMat44(col0, col1, col2, col3));
	ModelToWorld = WorldToModel.getInverse();
}

void PhysicsManager::Update(const float DELTA_TIME)
{
	_ASSERT(m_pScene);

	// m_pScene->simulate(1.0f / 60.0f);
	m_pScene->simulate(DELTA_TIME);
	m_pScene->fetchResults(true);
}

void PhysicsManager::CookingStaticTriangleMesh(const std::vector<Vertex>* pVERTICES, const std::vector<UINT32>* pINDICES, const Matrix& WORLD)
{
	_ASSERT(m_pPhysics);
	_ASSERT(m_pScene);

	const UINT64 TOTAL_VERTEX = pVERTICES->size();
	const UINT64 TOTAL_INDEX = pINDICES->size();
	const Vector3 POSITION = WORLD.Translation();
	const PxVec3 POSITION_PX(POSITION.x, POSITION.y, POSITION.z);
	PxMat44 world;
	std::vector<PxVec3> vertices(TOTAL_VERTEX);
	std::vector<PxU32> indices(TOTAL_INDEX);

	for (int i = 0; i < 4; ++i)
	{
		for (int j = 0; j < 4; ++j)
		{
			world(i, j) = WORLD.m[i][j];
		}
	}
	for (UINT64 i = 0; i < TOTAL_VERTEX; ++i)
	{
		PxVec3& v = vertices[i];
		const Vertex& originalV = (*pVERTICES)[i];

		v.x = originalV.Position.x;
		v.y = originalV.Position.y;
		v.z = -originalV.Position.z;
	}
	indices = *pINDICES;


	PxTriangleMeshDesc meshDesc;
	meshDesc.points.count = (PxU32)TOTAL_VERTEX;
	meshDesc.points.stride = sizeof(PxVec3);
	meshDesc.points.data = vertices.data();
	meshDesc.triangles.count = (PxU32)(TOTAL_INDEX / 3);
	meshDesc.triangles.stride = 3 * sizeof(PxU32);
	meshDesc.triangles.data = indices.data();

	PxTolerancesScale scale;
	PxCookingParams params(scale);

	PxDefaultMemoryOutputStream writeBuffer;
	PxTriangleMeshCookingResult::Enum result;

	bool status = PxCookTriangleMesh(params, meshDesc, writeBuffer, &result);
	if (!status)
	{
		__debugbreak();
	}

	PxDefaultMemoryInputData readBuffer(writeBuffer.getData(), writeBuffer.getSize());
	PxTriangleMesh* pMesh = m_pPhysics->createTriangleMesh(readBuffer);
	if (!pMesh)
	{
		__debugbreak();
	}

	PxRigidStatic* pPawn = m_pPhysics->createRigidStatic(PxTransform(world));
	if (!pPawn)
	{
		__debugbreak();
	}
	
	PxTriangleMeshGeometry geom(pMesh);
	PxShape* pShape = m_pPhysics->createShape(geom, *pCommonMaterial);
	if (!pShape)
	{
		__debugbreak();
	}

	pPawn->attachShape(*pShape);
	m_pScene->addActor(*pPawn);
}

void PhysicsManager::AddActor(physx::PxRigidActor* pActor)
{
	_ASSERT(m_pScene);
	_ASSERT(pActor);

	m_pScene->addActor(*pActor);
}

void PhysicsManager::Cleanup()
{
	if (m_pControllerManager)
	{
		m_pControllerManager->purgeControllers();
		m_pControllerManager->release();
		m_pControllerManager = nullptr;
	}
	PX_RELEASE(m_pTaskManager);
	PX_RELEASE(m_pScene);
	PX_RELEASE(m_pDispatcher);
	PX_RELEASE(m_pPhysics);
	if (m_pPVD)
	{
		PxPvdTransport* pTransport = m_pPVD->getTransport();
		m_pPVD->release();
		m_pPVD = nullptr;
		pTransport->release();
		pTransport = nullptr;
	}
	PX_RELEASE(m_pFoundation);
}
