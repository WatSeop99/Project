#include <PxPhysicsAPI.h>

using namespace physx;

class CustomFilterCallback : public PxQueryFilterCallback
{
public:
	PxQueryHitType::Enum preFilter(
		const PxFilterData& filterData,
		const PxShape* shape,
		const PxRigidActor* actor,
		const PxRigidActor* actor,
		PxHitFlags& queryFlags) override
	{
		// Check if the actor is of type PxRigidDynamic and has a specific type
		const PxRigidDynamic* rigidDynamic = actor->is<PxRigidDynamic>();
		if (rigidDynamic)
		{
			// Assuming we use the userData to store the type information
			int actorType = reinterpret_cast<int>(actor->userData);
			if (actorType == SPECIFIC_TYPE)
			{
				// Ignore this actor
				return PxQueryHitType::eNONE;
			}
		}
		// Default behavior
		return PxQueryHitType::eBLOCK;
	}

	PxQueryHitType::Enum postFilter(
		const PxFilterData& filterData,
		const PxQueryHit& hit) override
	{
		// Default behavior
		return PxQueryHitType::eBLOCK;
	}
};

class CharacterControllerExample
{
private:
	PxControllerManager* controllerManager;
	PxController* characterController;

public:
	void initialize(PxScene* scene, PxPhysics* physics)
	{
		controllerManager = PxCreateControllerManager(*scene);

		PxCapsuleControllerDesc desc;
		desc.height = 1.8f;
		desc.radius = 0.5f;
		desc.position = PxExtendedVec3(0, 0, 0);
		desc.upDirection = PxVec3(0, 1, 0);
		desc.slopeLimit = 0.5f;
		desc.stepOffset = 0.5f;
		desc.contactOffset = 0.1f;
		desc.material = physics->createMaterial(0.5f, 0.5f, 0.5f);

		characterController = controllerManager->createController(desc);
	}

	void moveCharacter(const PxVec3& displacement, float deltaTime)
	{
		// Define custom filters
		PxFilterData filterData;
		filterData.word0 = 1; // Custom filter data

		CustomFilterCallback filterCallback;

		PxControllerFilters filters;
		filters.mFilterData = &filterData;
		filters.mFilterCallback = &filterCallback;

		PxControllerCollisionFlags flags = characterController->move(displacement, 0.001f, deltaTime, filters);
		// Handle movement flags (e.g., collision detection)
	}
};

int main()
{
	// Initialize PhysX and create scene, physics, etc.
	// ...

	CharacterControllerExample example;
	example.initialize(scene, physics);

	// Move character
	example.moveCharacter(PxVec3(1.0f, 0.0f, 0.0f), 0.016f);

	// Cleanup PhysX
	// ...

	return 0;
}
