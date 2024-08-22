// #include <Eigen/Dense>
#include "../pch.h"
#include "../Util/Utility.h"
#include "AnimationData.h"

Matrix AnimationClip::Key::GetTransform()
{
	return (Matrix::CreateScale(Scale) * Matrix::CreateFromQuaternion(Rotation) * Matrix::CreateTranslation(Position));
}

void AnimationData::Update(const int CLIP_ID, const int FRAME, const float DELTA_TIME)
{
	TimeSinceLoaded += DELTA_TIME;

	AnimationClip& clip = Clips[CLIP_ID];
	float timeInTicks = (float)(TimeSinceLoaded * clip.TicksPerSec);
	float animationTimeTicks = fmod(timeInTicks, (float)clip.Duration);

	// root bone id은 0(아닐 수 있음).
	{
		const int ROOT_BONE_ID = 0;
		
		Vector3 interpolatedPos;
		Vector3 interpolatedScale;
		Quaternion interpolatedRot;
		InterpolateKeyData(&interpolatedPos, &interpolatedRot, &interpolatedScale, &clip, 0, animationTimeTicks);

		if (CLIP_ID == 0)
		{
			interpolatedPos.y = 0.0f;
			BoneTransforms[ROOT_BONE_ID] = Matrix::CreateScale(interpolatedScale) * Matrix::CreateFromQuaternion(interpolatedRot) * Matrix::CreateTranslation(interpolatedPos);
		}
		else
		{
			BoneTransforms[ROOT_BONE_ID] = Matrix::CreateScale(interpolatedScale) * Matrix::CreateFromQuaternion(interpolatedRot);
		}
	}

	// 나머지 bone transform 업데이트.
	// bone id가 부모->자식 순으로 선형적으로 저장되어 있기에 가능함.
	for (UINT64 boneID = 1, totalBone = BoneTransforms.size(); boneID < totalBone; ++boneID)
	{
		const int PARENT_ID = BoneParents[boneID];

		Vector3 interpolatedPos;
		Vector3 interpolatedScale;
		Quaternion interpolatedRot;
		InterpolateKeyData(&interpolatedPos, &interpolatedRot, &interpolatedScale, &clip, (const int)boneID, animationTimeTicks);

		BoneTransforms[boneID] = Matrix::CreateScale(interpolatedScale) * Matrix::CreateFromQuaternion(interpolatedRot) * Matrix::CreateTranslation(interpolatedPos) * BoneTransforms[PARENT_ID];
	}
}

void AnimationData::UpdateForIK(const int CLIP_ID, const int FRAME)
{
	AnimationClip& clip = Clips[CLIP_ID];
	float timeInTicks = (float)(TimeSinceLoaded * clip.TicksPerSec);
	float animationTimeTicks = fmod(timeInTicks, (float)clip.Duration);

	{
		const int ROOT_BONE_ID = 0;

		Vector3 interpolatedPos;
		Vector3 interpolatedScale;
		Quaternion interpolatedRot;
		InterpolateKeyData(&interpolatedPos, &interpolatedRot, &interpolatedScale, &clip, 0, animationTimeTicks);

		Quaternion newRot = Quaternion::Concatenate(interpolatedRot, clip.IKRotations[ROOT_BONE_ID]);
		//BoneTransforms[ROOT_BONE_ID] = Matrix::CreateScale(interpolatedScale) * Matrix::CreateFromQuaternion(newRot);
		if (CLIP_ID == 0)
		{
			interpolatedPos.y = 0.0f;
			BoneTransforms[ROOT_BONE_ID] = Matrix::CreateScale(interpolatedScale) * Matrix::CreateFromQuaternion(newRot) * Matrix::CreateTranslation(interpolatedPos);
		}
		else
		{
			BoneTransforms[ROOT_BONE_ID] = Matrix::CreateScale(interpolatedScale) * Matrix::CreateFromQuaternion(newRot);
		}
	}

	for (UINT64 boneID = 1, totalBone = BoneTransforms.size(); boneID < totalBone; ++boneID)
	{
		const int PARENT_ID = BoneParents[boneID];

		Vector3 interpolatedPos;
		Vector3 interpolatedScale;
		Quaternion interpolatedRot;
		InterpolateKeyData(&interpolatedPos, &interpolatedRot, &interpolatedScale, &clip, (const int)boneID, animationTimeTicks);

		Quaternion newRot = Quaternion::Concatenate(interpolatedRot, clip.IKRotations[boneID]);
		BoneTransforms[boneID] = Matrix::CreateScale(interpolatedScale) * Matrix::CreateFromQuaternion(newRot) * Matrix::CreateTranslation(interpolatedPos) * BoneTransforms[PARENT_ID];
	}
}

void AnimationData::UpdateVelocity(const int CLIP_ID, const int FRAME)
{
	AnimationClip& clip = Clips[CLIP_ID];

	const int ROOT_BONE_ID = 0;
	std::vector<AnimationClip::Key>& keys = clip.Keys[ROOT_BONE_ID];
	const UINT64 KEY_SIZE = keys.size();
	AnimationClip::Key& key = keys[FRAME % KEY_SIZE];

	if (FRAME != 0)
	{
		Vector3 posDelta = key.Position - PrevKeyPos;
		posDelta = Vector3::Transform(posDelta, DefaultTransform);
		Velocity = posDelta.Length();
	}
	else
	{
		Velocity = 0.0f;
	}
	PrevKeyPos = key.Position;
}

void AnimationData::ResetAllIKRotations(const int CLIP_ID)
{
	AnimationClip& clip = Clips[CLIP_ID];
	for (UINT64 i = 0, end = clip.IKRotations.size(); i < end; ++i)
	{
		clip.IKRotations[i] = Quaternion();
	}
}

void AnimationData::InterpolateKeyData(Vector3* pOutPosition, Quaternion* pOutRotation, Vector3* pOutScale, AnimationClip* pClip, const int BONE_ID, const float ANIMATION_TIME_TICK)
{
	_ASSERT(pOutScale);
	_ASSERT(pClip);

	std::vector<AnimationClip::Key>& keys = pClip->Keys[BONE_ID];
	const UINT64 KEY_SIZE = keys.size();

	if (KEY_SIZE == 1)
	{
		AnimationClip::Key& key = keys[0];
		*pOutPosition = key.Position;
		*pOutRotation = key.Rotation;
		*pOutScale = key.Scale;

		return;
	}

	// Find key index at this time.
	UINT positionIndex = findIndex(pClip, BONE_ID, ANIMATION_TIME_TICK);
	UINT nextPositionIndex = positionIndex + 1;
	_ASSERT(nextPositionIndex < KEY_SIZE);

	AnimationClip::Key& curKey = keys[positionIndex];
	AnimationClip::Key& nextKey = keys[nextPositionIndex];

	// Calculate interpolated time.
	float t1 = (float)curKey.Time;
	float t2 = (float)nextKey.Time;
	float deltaTime = t2 - t1;
	float factor = (ANIMATION_TIME_TICK - t1) / deltaTime;

	// Calculate position data.
	const Vector3& START_POS = curKey.Position;
	const Vector3& END_POS = nextKey.Position;
	Vector3 deltaPos = END_POS - START_POS;
	*pOutPosition = START_POS + factor * deltaPos;

	// Calculate rotation data.
	const Quaternion& START_ROT = curKey.Rotation;
	const Quaternion& END_ROT = nextKey.Rotation;
	Quaternion interporlated = DirectX::XMQuaternionSlerp(START_ROT, END_ROT, factor);
	interporlated.Normalize();
	*pOutRotation = interporlated;

	// Calculate scale data.
	const Vector3& START_SCALE = curKey.Scale;
	const Vector3& END_SCALE = nextKey.Scale;
	Vector3 deltaScale = END_SCALE - START_SCALE;
	*pOutScale = START_SCALE + factor * deltaScale;
}

Matrix AnimationData::Get(const int CLIP_ID, const int FRAME, const int BONE_ID)
{
	return (InverseDefaultTransform * OffsetMatrices[BONE_ID] * BoneTransforms[BONE_ID] * InverseOffsetMatrices[0] * DefaultTransform);
}

Matrix AnimationData::GetRootBoneTransformWithoutLocalRot(const int CLIP_ID, const int FRAME)
{
	AnimationClip& clip = Clips[CLIP_ID];

	// root bone id은 0(아닐 수 있음).
	const int ROOT_BONE_ID = 0;
	std::vector<AnimationClip::Key>& keys = clip.Keys[ROOT_BONE_ID];
	const UINT64 KEY_SIZE = keys.size();

	const int PARENT_ID = BoneParents[ROOT_BONE_ID];
	AnimationClip::Key& key = keys[FRAME % KEY_SIZE];

	Matrix ret = Matrix::CreateScale(key.Scale) * Matrix::CreateTranslation(key.Position);

	return (InverseDefaultTransform * OffsetMatrices[0] * ret * InverseOffsetMatrices[0] * DefaultTransform);
}

Matrix AnimationData::GetGlobalBonePositionMatix(const int CLIP_ID, const int FRAME, const int BONE_ID)
{
	return (InverseDefaultTransform * OffsetMatrices[0] * BoneTransforms[BONE_ID] * InverseOffsetMatrices[0] * DefaultTransform);
}

UINT AnimationData::findIndex(AnimationClip* pClip, const int BONE_ID, const float ANIMATION_TIME_TICK)
{
	_ASSERT(pClip);
	_ASSERT(BONE_ID >= 0);

	UINT ret = 0;

	std::vector<AnimationClip::Key>& keys = pClip->Keys[BONE_ID];
	for (UINT64 i = 0, end = keys.size() - 1; i < end; ++i)
	{
		float t = (float)keys[i + 1].Time;
		if (ANIMATION_TIME_TICK < t)
		{
			ret = (UINT)i;
			break;
		}
	}

	return ret;
}

Joint::Joint()
{
	// 초기 제한 값은 최대로 해놓음.
	for (int i = 0; i < JointAxis_AxisCount; ++i)
	{
		AngleLimitation[i] = Vector2(-FLT_MAX, FLT_MAX);
	}
}

void Joint::ApplyJacobian(float deltaThetaX, float deltaThetaY, float deltaThetaZ, AnimationData* pAnimationData, int clipID, int frame, Matrix& characerWorld)
{
	_ASSERT(pAnimationData);
	_ASSERT(BoneID != 0xffffffff);

	// 변환 각도.
	
	/*Matrix offset = characerWorld.Invert() * pAnimationData->InverseDefaultTransform * pAnimationData->OffsetMatrices[BoneID];
	Vector3 xAxis = Vector3::Transform(Vector3::UnitX, offset);
	Vector3 yAxis = Vector3::Transform(Vector3::UnitY, offset);
	Vector3 zAxis = Vector3::Transform(Vector3::UnitZ, offset);

	Quaternion deltaRot = Quaternion::CreateFromAxisAngle(xAxis, deltaThetaX);
	deltaRot = Quaternion::Concatenate(deltaRot, Quaternion::CreateFromAxisAngle(yAxis, deltaThetaY));
	deltaRot = Quaternion::Concatenate(deltaRot, Quaternion::CreateFromAxisAngle(zAxis, deltaThetaZ));*/

	Quaternion deltaRot = Quaternion::CreateFromYawPitchRoll(deltaThetaY, deltaThetaX, deltaThetaZ);
	/*Vector3 xAxis = Vector3(1.0f, 0.0f, 0.0f) * deltaThetaX;
	Vector3 yAxis = Vector3(0.0f, 1.0f, 0.0f) * deltaThetaY;
	Vector3 zAxis = Vector3(0.0f, 0.0f, 1.0f) * deltaThetaZ;
	Quaternion xAxisRot;
	Quaternion yAxisRot;
	Quaternion zAxisRot;
	const float EPSILON = 1.192092896e-7f;
	{
		const float xAxisAngle = xAxis.Length();
		if (xAxisAngle < EPSILON)
		{
			xAxisRot = Quaternion(xAxis * sinf(xAxisAngle), cosf(xAxisAngle));
		}
		else
		{
			xAxisRot = Quaternion((xAxis / xAxisAngle) * sin(xAxisAngle), cosf(xAxisAngle));
		}
	}
	{
		const float yAxisAngle = yAxis.Length();
		if (yAxisAngle < EPSILON)
		{
			yAxisRot = Quaternion(yAxis * sinf(yAxisAngle), cosf(yAxisAngle));
		}
		else
		{
			yAxisRot = Quaternion((yAxis / yAxisAngle) * sinf(yAxisAngle), cosf(yAxisAngle));
		}
	}
	{
		const float zAxisAngle = zAxis.Length();
		if (zAxisAngle < EPSILON)
		{
			zAxisRot = Quaternion(zAxis * sinf(zAxisAngle), cosf(zAxisAngle));
		}
		else
		{
			zAxisRot = Quaternion((zAxis / zAxisAngle) * sinf(zAxisAngle), cosf(zAxisAngle));
		}
	}
	Quaternion deltaRot = Quaternion::Concatenate(xAxisRot, yAxisRot);
	deltaRot = Quaternion::Concatenate(deltaRot, zAxisRot);*/

	// 원래 키 데이터에서의 변환 각도.
	AnimationClip& clip = pAnimationData->Clips[clipID];
	float timeInTicks = (float)(pAnimationData->TimeSinceLoaded * clip.TicksPerSec);
	float animationTimeTicks = fmod(timeInTicks, (float)clip.Duration);

	Vector3 interpolatedPos;
	Vector3 interpolatedScale;
	Quaternion interpolatedRot;
	pAnimationData->InterpolateKeyData(&interpolatedPos, &interpolatedRot, &interpolatedScale, &clip, (const int)BoneID, animationTimeTicks);

	Quaternion originRot = interpolatedRot;
	Quaternion prevUpdateRot = clip.IKRotations[BoneID];

	// 원래 각도에서 변환 각도 적용.
	Quaternion newUpdateRot = Quaternion::Concatenate(originRot, prevUpdateRot);
	newUpdateRot = Quaternion::Concatenate(newUpdateRot, deltaRot);

	//// 적용 전, joint 전체 제한 값 테스트.
	//Matrix newUpdateRotMat = Matrix::CreateFromQuaternion(newUpdateRot);
	//float pitch = asin(-newUpdateRotMat._23);
	//float yaw = atan2(newUpdateRotMat._13, newUpdateRotMat._33);
	//float roll = atan2(newUpdateRotMat._21, newUpdateRotMat._22);
	//bool bUpdateFlag = false;

	//// Restrict x-axis value.
	//if (pitch < AngleLimitation[JointAxis_X].x || pitch > AngleLimitation[JointAxis_X].y)
	//{
	//	deltaThetaX = 0.0f;
	//	bUpdateFlag = true;
	//}

	//// Restrict y-axis value.
	//if (yaw < AngleLimitation[JointAxis_Y].x || yaw > AngleLimitation[JointAxis_Y].y)
	//{
	//	deltaThetaY = 0.0f;
	//	bUpdateFlag = true;
	//}

	//// Restrict z-axis value.
	//if (roll < AngleLimitation[JointAxis_Z].x || roll > AngleLimitation[JointAxis_Z].y)
	//{
	//	deltaThetaZ = 0.0f;
	//	bUpdateFlag = true;
	//}

	//if (bUpdateFlag)
	//{
	//	deltaRot = Quaternion::CreateFromYawPitchRoll(deltaThetaY, deltaThetaX, deltaThetaZ);
	//	newUpdateRot = Quaternion::Concatenate(prevUpdateRot, deltaRot);
	//}


	//clip.IKRotations[BoneID] = newUpdateRot;
	clip.IKRotations[BoneID] = Quaternion::Concatenate(prevUpdateRot, deltaRot);
}

void Chain::Initialize(const int BODY_CHAIN_SIZE)
{
	BodyChain.resize(BODY_CHAIN_SIZE);

	m_JacobianMatrix = Eigen::MatrixXf(3, 3 * BODY_CHAIN_SIZE);
	m_DeltaPos = Eigen::VectorXf(3);
	m_DeltaTheta = Eigen::VectorXf(3 * BODY_CHAIN_SIZE);
}

bool Chain::SolveIK(AnimationData* pAnimationData, Vector3& targetPos, float* pDeltaThetas, const int CLIP_ID, const int FRAME, const float DELTA_TIME, Matrix& characterWorld)
{
	_ASSERT(pAnimationData);
	_ASSERT(pDeltaThetas);
	_ASSERT(BodyChain.size() > 0);

//	bool bRetContinue = true;
//
//	const UINT64 TOTAL_JOINT = BodyChain.size();
//	Joint* pEndEffector = &BodyChain[TOTAL_JOINT - 1];
//	ZeroMemory(pDeltaThetas, sizeof(float) * 3 * TOTAL_JOINT); // hip 제외.
//
//	Vector3 deltaPos = targetPos - pEndEffector->Position;
//	float deltaPosLength = deltaPos.Length();
//	{
//		/*char szDebugString[256];
//		sprintf_s(szDebugString, 256, "length: %f\n", deltaPosLength);
//		OutputDebugStringA(szDebugString);*/
//	}
//	/*if (deltaPosLength < 0.01f || deltaPosLength > 0.8f)
//	{
//		bRetContinue = false;
//		goto LB_RET;
//	}*/
//
//	{
//		Vector3 startToTarget = targetPos - BodyChain[0].Position;
//		const float START_TO_TARGET_LENGTH = startToTarget.Length();
//		const float LENGTH = (BodyChain[TOTAL_JOINT - 1].Position - BodyChain[0].Position).Length();
//		float length = 0.0f;
//		for (UINT64 i = 0; i < TOTAL_JOINT; ++i)
//		{
//			length += BodyChain[i].Length;
//		}
//		//if (length < START_TO_TARGET_LENGTH)
//		//{
//		//	/*char szDebugString[256];
//		//	sprintf_s(szDebugString, 256, "length: %f targetLength: %f\n", LENGTH, START_TO_TARGET_LENGTH);
//		//	OutputDebugStringA(szDebugString);*/
//
//		//	bRetContinue = false;
//		//	goto LB_RET;
//		//}
//	}
//
//	//m_DeltaPos << deltaPos.x, deltaPos.y, deltaPos.z;
//	m_DeltaPos[0] = deltaPos.x;
//	m_DeltaPos[1] = deltaPos.y;
//	m_DeltaPos[2] = deltaPos.z;
//
//	{
//		int columnIndex = 0;
//		Matrix inverseWorld = characterWorld.Invert();
//		for (UINT64 i = 0, end = TOTAL_JOINT; i < end; ++i)
//		{
//			Joint* pJoint = &BodyChain[i];
//			Vector3 diff = pEndEffector->Position - pJoint->Position;
//			diff.Normalize();
//
//			Matrix offset = pAnimationData->InverseOffsetMatrices[pJoint->BoneID] * pAnimationData->DefaultTransform * characterWorld;
//			Vector3 xAxis = Vector3::Transform(Vector3::UnitX, offset);
//			Vector3 yAxis = Vector3::Transform(Vector3::UnitY, offset);
//			Vector3 zAxis = Vector3::Transform(Vector3::UnitZ, offset);
//
//			Vector3 partialX = xAxis.Cross(diff);
//			Vector3 partialY = yAxis.Cross(diff);
//			Vector3 partialZ = zAxis.Cross(diff);
//			/*Vector3 partialX = Vector3::UnitX.Cross(diff);
//			Vector3 partialY = Vector3::UnitY.Cross(diff);
//			Vector3 partialZ = Vector3::UnitZ.Cross(diff);*/
//
//			// 3-dof.
//			/*m_JacobianMatrix.col(columnIndex) << partialX.x, partialX.y, partialX.z;
//			m_JacobianMatrix.col(columnIndex + 1) << partialY.x, partialY.y, partialY.z;
//			m_JacobianMatrix.col(columnIndex + 2) << partialZ.x, partialZ.y, partialZ.z;*/
//			m_JacobianMatrix(0, columnIndex) = partialX.x;
//			m_JacobianMatrix(1, columnIndex) = partialX.y;
//			m_JacobianMatrix(2, columnIndex) = partialX.z;
//			m_JacobianMatrix(0, columnIndex + 1) = partialY.x;
//			m_JacobianMatrix(1, columnIndex + 1) = partialY.y;
//			m_JacobianMatrix(2, columnIndex + 1) = partialY.z;
//			m_JacobianMatrix(0, columnIndex + 2) = partialZ.x;
//			m_JacobianMatrix(1, columnIndex + 2) = partialZ.y;
//			m_JacobianMatrix(2, columnIndex + 2) = partialZ.z;
//
//			columnIndex += 3;
//		}
//	}
//
//	m_DeltaTheta = m_JacobianMatrix.bdcSvd(Eigen::ComputeFullU | Eigen::ComputeThinV).solve(m_DeltaPos);
//	m_DeltaTheta *= DELTA_TIME;
//	{
//		char szDebugString[256];
//		sprintf_s(szDebugString, 256, "Theta 1: %f, %f, %f  Theta 2: %f, %f, %f  Theta 3: %f, %f, %f  Theta 4: %f, %f, %f\n",
//				  m_DeltaTheta[0], m_DeltaTheta[1], m_DeltaTheta[2],
//				  m_DeltaTheta[3], m_DeltaTheta[4], m_DeltaTheta[5],
//				  m_DeltaTheta[6], m_DeltaTheta[7], m_DeltaTheta[8],
//				  m_DeltaTheta[9], m_DeltaTheta[10], m_DeltaTheta[11]);
//		OutputDebugStringA(szDebugString);
//	}
//
//	for (UINT64 i = 0, end = 3 * TOTAL_JOINT; i < end; ++i)
//	{
//		pDeltaThetas[i] = m_DeltaTheta[i];
//	}
//
//LB_RET:
//	return bRetContinue;

	// https://www.ryanjuckett.com/analytic-two-bone-ik-in-2d/
	// leg 기준.

	// bool bReached = ((BodyChain[0].Position - targetPos).Length() <= legLength);
	
	AnimationClip& clip = pAnimationData->Clips[CLIP_ID];
	float timeInTicks = (float)(pAnimationData->TimeSinceLoaded * clip.TicksPerSec);
	float animationTimeTicks = fmod(timeInTicks, (float)clip.Duration);

	Joint& startJoint = BodyChain[0];
	Joint& midJoint = BodyChain[1];
	Joint& endJoint = BodyChain[2];
	Matrix inverseWorld = characterWorld.Invert();
	
	Vector3 interpolatedPos;
	Vector3 interpolatedScale;
	Quaternion interpolatedRot;
	Matrix transform;
	pAnimationData->InterpolateKeyData(&interpolatedPos, &interpolatedRot, &interpolatedScale, &clip, startJoint.BoneID, animationTimeTicks);
	transform = Matrix::CreateScale(interpolatedScale) * Matrix::CreateFromQuaternion(interpolatedRot) * Matrix::CreateTranslation(interpolatedPos);

	Quaternion midIKRot;
	Quaternion startIKRot;
	float midJointAngle = 0.0f;
	// mid joint angle.
	{
		Matrix offset = inverseWorld * pAnimationData->InverseDefaultTransform * pAnimationData->OffsetMatrices[startJoint.BoneID];
		Vector3 targetPosInStartJointSpace = Vector3::Transform(targetPos, offset);
		Vector3 startJointPos = Vector3::Transform(startJoint.Position, offset);
		Vector3 midJointPos = Vector3::Transform(midJoint.Position, offset);
		Vector3 endJointPos = Vector3::Transform(endJoint.Position, offset);

		Vector3 startToTarget = targetPosInStartJointSpace - startJointPos;
		Vector3 upLeg = midJointPos - startJointPos;
		Vector3 leg = endJointPos - midJointPos;
		startToTarget.x = 0.0f;
		upLeg.x = 0.0f;
		leg.x = 0.0f;

		const float UPLEG_TO_LEG_LENGTH = upLeg.Length();
		const float LEG_TO_FOOT_LENGTH = leg.Length();
		const float UPLEG_TO_TARGET = startToTarget.Length();

		// rotate in x-axis(yx).
		float cosTheta = ((UPLEG_TO_TARGET * UPLEG_TO_TARGET) - (UPLEG_TO_LEG_LENGTH * UPLEG_TO_LEG_LENGTH) - (LEG_TO_FOOT_LENGTH * LEG_TO_FOOT_LENGTH)) / (2.0f * UPLEG_TO_LEG_LENGTH * LEG_TO_FOOT_LENGTH);
		cosTheta = Clamp(cosTheta, -1.0f, 1.0f);
		midJointAngle = acosf(cosTheta);
		midJointAngle = Clamp(midJointAngle, 0.0f, DirectX::XM_PI);
		{
			char szDebugString[256];
			sprintf_s(szDebugString, 256, "midJointAngle: %f\n", midJointAngle);
			OutputDebugStringA(szDebugString);
		}

		midIKRot = Quaternion::CreateFromAxisAngle(Vector3::UnitX, midJointAngle);
	}

	// start joint angle.
	{
		// xy(z-axis), yz(x-axis), zx(y-axis) 세 plane에서 바라본 target pos와의 각도를 구해야함.(in start joint space)
		// 여기서 mid joint는 yz 평면에서만 회전하므로, 이 평면만 이론대로 계산하고, 나머지 두 평면은 간단히 arctan으로 계산.

		// yx
		Quaternion startIKRotInZ;
		{
			Matrix offset = inverseWorld * pAnimationData->InverseDefaultTransform * pAnimationData->OffsetMatrices[startJoint.BoneID];
			Vector3 targetPosInStartJointSpace = Vector3::Transform(targetPos, offset);
			Vector3 startJointPos = Vector3::Transform(startJoint.Position, offset);
			Vector3 midJointPos = Vector3::Transform(midJoint.Position, offset);
			Vector3 endJointPos = Vector3::Transform(endJoint.Position, offset);

			Vector3 startJointToEndJoint = endJointPos - startJointPos;
			Vector3 startJointToTarget = targetPosInStartJointSpace - startJointPos;

			startJointToEndJoint.z = 0.0f;
			startJointToEndJoint.Normalize();
			startJointToTarget.z = 0.0f;
			startJointToTarget.Normalize();

			float dot = startJointToEndJoint.Dot(startJointToTarget);
			float angle = acosf(dot);
			startIKRotInZ = Quaternion::CreateFromAxisAngle(Vector3::UnitZ, angle);
		}

		// zy
		Quaternion startIKRotInX;
		{
			Matrix offset = inverseWorld * pAnimationData->InverseDefaultTransform * pAnimationData->OffsetMatrices[startJoint.BoneID];
			Vector3 targetPosInStartJointSpace = Vector3::Transform(targetPos, offset);
			Vector3 startJointPos = Vector3::Transform(startJoint.Position, offset);
			Vector3 midJointPos = Vector3::Transform(midJoint.Position, offset);
			Vector3 endJointPos = Vector3::Transform(endJoint.Position, offset);

			Vector3 startJointToTarget = targetPosInStartJointSpace - startJointPos;
			Vector3 upLeg = midJointPos - startJointPos;
			Vector3 leg = endJointPos - midJointPos;
			startJointToTarget.x = 0.0f;
			upLeg.x = 0.0f;
			leg.x = 0.0f;

			const float UPLEG_TO_LEG_LENGTH = upLeg.Length();
			const float LEG_TO_FOOT_LENGTH = leg.Length();
			const float UPLEG_TO_TARGET = startJointToTarget.Length();
			const float FIRST = UPLEG_TO_LEG_LENGTH + LEG_TO_FOOT_LENGTH * cosf(midJointAngle);
			const float SECOND = LEG_TO_FOOT_LENGTH * sinf(midJointAngle);
			float angle = atan2(startJointToTarget.z * FIRST + startJointToTarget.y * SECOND,
								startJointToTarget.y * FIRST - startJointToTarget.z * SECOND);
			startIKRotInX = Quaternion::CreateFromAxisAngle(Vector3::UnitX, angle);
		}

		// xz
		Quaternion startIKRotInY;
		{
			Matrix offset = inverseWorld * pAnimationData->InverseDefaultTransform * pAnimationData->OffsetMatrices[startJoint.BoneID];
			Vector3 targetPosInStartJointSpace = Vector3::Transform(targetPos, offset);
			Vector3 startJointPos = Vector3::Transform(startJoint.Position, offset);
			Vector3 midJointPos = Vector3::Transform(midJoint.Position, offset);
			Vector3 endJointPos = Vector3::Transform(endJoint.Position, offset);

			Vector3 startJointToEndJoint = endJointPos - startJointPos;
			Vector3 startJointToTarget = targetPosInStartJointSpace - startJointPos;

			startJointToEndJoint.y = 0.0f;
			startJointToEndJoint.Normalize();
			startJointToTarget.y = 0.0f;
			startJointToTarget.Normalize();

			float dot = startJointToEndJoint.Dot(startJointToTarget);
			float angle = acosf(dot);
			startIKRotInY = Quaternion::CreateFromAxisAngle(Vector3::UnitY, angle);
		}

		startIKRot = Quaternion::Concatenate(startIKRotInX, startIKRotInY);
		startIKRot = Quaternion::Concatenate(startIKRot, startIKRotInZ);
	}

	clip.IKRotations[startJoint.BoneID] = startIKRot;
	clip.IKRotations[midJoint.BoneID] = midIKRot;
	
	return true;
}
