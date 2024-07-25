#pragma once

#include "AppBase.h"

#include "physx/PxPhysicsAPI.h"

// 기본 사용법은 SnippetHelloWorld.cpp
// 렌더링 관련은 SnippetHelloWorldRender.cpp

#define PX_RELEASE(x)                                                          \
    if (x) {                                                                   \
        x->release();                                                          \
        x = NULL;                                                              \
    }

#define PVD_HOST "127.0.0.1"
#define MAX_NUM_ACTOR_SHAPES 128

namespace hlab {

	using namespace physx;

	class Ex1901_PhysX : public AppBase {
	public:
		Ex1901_PhysX();

		~Ex1901_PhysX() {
			PX_RELEASE(gScene);
			PX_RELEASE(gDispatcher);
			PX_RELEASE(gPhysics);
			if (gPvd) {
				PxPvdTransport* transport = gPvd->getTransport();
				gPvd->release();
				gPvd = NULL;
				PX_RELEASE(transport);
			}
			PX_RELEASE(gFoundation);
		}

		// 동적으로 움직이는 물체 생성.
		PxRigidDynamic* createDynamic(const PxTransform& t,
									  const PxGeometry& geometry,
									  const PxVec3& velocity = PxVec3(0)) {
			PxRigidDynamic* dynamic =
				PxCreateDynamic(*gPhysics, t, geometry, *gMaterial, 10.0f);
			dynamic->setAngularDamping(0.5f); // 각속도를 줄여줌. 마찰로 생각하면 됨.
			dynamic->setLinearVelocity(velocity); // 속도 설정. 
			// Rigid body는 이동에 대해 3자유도, 회전에 대해 3자유도 총 6자유도를 가짐.
			// 따라서 6개의 숫자를 통해 속도 설정할 수 있음. 여기서는 이동에 대한 속도만 설정.
			gScene->addActor(*dynamic);
			return dynamic;
		}

		void createStack(const PxTransform& t, PxU32 size, PxReal halfExtent) {

			vector<MeshData> box = { GeometryGenerator::MakeBox(halfExtent) };

			PxShape* shape = gPhysics->createShape(
				PxBoxGeometry(halfExtent, halfExtent, halfExtent), *gMaterial);

			for (PxU32 i = 2; i < size; i++) {
				for (PxU32 j = 0; j < size - i; j++) {
					PxTransform localTm(PxVec3(PxReal(j * 2) - PxReal(size - i),
											   PxReal(i * 2 + 1), 0) *
										halfExtent); // 1 쿼터니언, 1 위치 이동벡터
					PxRigidDynamic* body =
						gPhysics->createRigidDynamic(t.transform(localTm));
					body->attachShape(*shape); // 박스 형태의 rigid body volume 생성.
					PxRigidBodyExt::updateMassAndInertia(*body, 10.0f); // 질량 생성.
					gScene->addActor(*body);

					auto m_newObj =
						std::make_shared<Model>(m_device, m_context, box);
					m_newObj->m_materialConsts.GetCpu().albedoFactor =
						Vector3(0.5f);
					AppBase::m_basicList.push_back(m_newObj);
					this->m_objects.push_back(m_newObj);
				}
			}
			shape->release();
		}

		void InitPhysics(bool interactive);

		bool InitScene() override;

		void UpdateLights(float dt) override;
		void UpdateGUI() override;
		void Update(float dt) override;
		void Render() override;

	public:
		PxDefaultAllocator gAllocator;
		PxDefaultErrorCallback gErrorCallback;
		PxFoundation* gFoundation = NULL;
		PxPhysics* gPhysics = NULL;
		PxDefaultCpuDispatcher* gDispatcher = NULL;
		PxScene* gScene = NULL;
		PxMaterial* gMaterial = NULL;
		PxPvd* gPvd = NULL;
		PxReal stackZ = 10.0f;

		vector<shared_ptr<Model>> m_objects;
	};

} // namespace hlab
