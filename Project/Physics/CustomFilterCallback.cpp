#include "../pch.h"
#include "CustomFilterCallback.h"

using namespace physx;

PxQueryHitType::Enum CustomFilterCallback::preFilter(const PxFilterData& filterData, const PxShape* shape, const PxRigidActor* actor, PxHitFlags& queryFlags)
{
	const PxRigidDynamic* pRigidDynamic = actor->is<PxRigidDynamic>();
	if (pRigidDynamic)
	{
		int* pActorType = (int*)(actor->userData);
		if (*pActorType == CollisionGroup_EndEffector)
		{
			return PxQueryHitType::eNONE;
		}
	}

	return PxQueryHitType::eBLOCK;
}

PxQueryHitType::Enum CustomFilterCallback::postFilter(const PxFilterData& filterData, const PxQueryHit& hit, const PxShape* shape, const PxRigidActor* actor)
{
	return PxQueryHitType::eBLOCK;
}
