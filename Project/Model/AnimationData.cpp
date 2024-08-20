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

void Joint::ApplyJacobian(float deltaThetaX, float deltaThetaY, float deltaThetaZ, AnimationData* pAnimationData, int clipID, int frame)
{
	_ASSERT(pAnimationData);
	_ASSERT(BoneID != 0xffffffff);

	// 변환 각도.
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
			xAxisRot = Quaternion(xAxis * sin(xAxisAngle), cos(xAxisAngle));
		}
		else
		{
			xAxisRot = Quaternion((xAxis / xAxisAngle) * sin(xAxisAngle), cos(xAxisAngle));
		}
	}
	{
		const float yAxisAngle = yAxis.Length();
		if (yAxisAngle < EPSILON)
		{
			yAxisRot = Quaternion(yAxis * sin(yAxisAngle), cos(yAxisAngle));
		}
		else
		{
			yAxisRot = Quaternion((yAxis / yAxisAngle) * sin(yAxisAngle), cos(yAxisAngle));
		}
	}
	{
		const float zAxisAngle = zAxis.Length();
		if (zAxisAngle < EPSILON)
		{
			zAxisRot = Quaternion(zAxis * sin(zAxisAngle), cos(zAxisAngle));
		}
		else
		{
			zAxisRot = Quaternion((zAxis / zAxisAngle) * sin(zAxisAngle), cos(zAxisAngle));
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

	// 적용 전, joint 전체 제한 값 테스트.
	Matrix newUpdateRotMat = Matrix::CreateFromQuaternion(newUpdateRot);
	float pitch = asin(-newUpdateRotMat._23);
	float yaw = atan2(newUpdateRotMat._13, newUpdateRotMat._33);
	float roll = atan2(newUpdateRotMat._21, newUpdateRotMat._22);
	bool bUpdateFlag = false;

	// Restrict x-axis value.
	if (pitch < AngleLimitation[JointAxis_X].x || pitch > AngleLimitation[JointAxis_X].y)
	{
		deltaThetaX = 0.0f;
		bUpdateFlag = true;
	}

	// Restrict y-axis value.
	if (yaw < AngleLimitation[JointAxis_Y].x || yaw > AngleLimitation[JointAxis_Y].y)
	{
		deltaThetaY = 0.0f;
		bUpdateFlag = true;
	}

	// Restrict z-axis value.
	if (roll < AngleLimitation[JointAxis_Z].x || roll > AngleLimitation[JointAxis_Z].y)
	{
		deltaThetaZ = 0.0f;
		bUpdateFlag = true;
	}

	if (bUpdateFlag)
	{
		deltaRot = Quaternion::CreateFromYawPitchRoll(deltaThetaY, deltaThetaX, deltaThetaZ);
		newUpdateRot = Quaternion::Concatenate(prevUpdateRot, deltaRot);
	}


	clip.IKRotations[BoneID] = newUpdateRot;
	//clip.IKRotations[BoneID] = Quaternion::Concatenate(prevUpdateRot, deltaRot);
}

void Chain::Initialize(const int BODY_CHAIN_SIZE)
{
	BodyChain.resize(BODY_CHAIN_SIZE);

	m_JacobianMatrix = Eigen::MatrixXf(3, 3 * (BODY_CHAIN_SIZE - 1));
	m_DeltaPos = Eigen::MatrixXf(3, 1);
	m_DeltaTheta = Eigen::VectorXf(3 * (BODY_CHAIN_SIZE - 1));
}

bool Chain::SolveIK(AnimationData* pAnimationData, Vector3& targetPos, float* pDeltaThetas, const int CLIP_ID, const int FRAME, const float DELTA_TIME)
{
	_ASSERT(pAnimationData);
	_ASSERT(pDeltaThetas);
	_ASSERT(BodyChain.size() > 0);

	bool bRetContinue = true;

	const UINT64 TOTAL_JOINT = BodyChain.size();
	Joint* pEndEffector = &BodyChain[TOTAL_JOINT - 1];
	ZeroMemory(pDeltaThetas, sizeof(float) * 3 * (TOTAL_JOINT - 1)); // hip 제외.

	Vector3 deltaPos = targetPos - pEndEffector->Position;
	float deltaPosLength = deltaPos.Length();
	{
		char szDebugString[256];
		sprintf_s(szDebugString, 256, "length: %f\n", deltaPosLength);
		OutputDebugStringA(szDebugString);
		/*sprintf_s(szDebugString, 256, "deltaPos: %f, %f, %f\n", deltaPos.x, deltaPos.y, deltaPos.z);
		OutputDebugStringA(szDebugString);*/
	}
	if (deltaPosLength < 0.01f || deltaPosLength > 0.8f)
	{
		bRetContinue = false;
		goto LB_RET;
	}

	m_DeltaPos << deltaPos.x, deltaPos.y, deltaPos.z;
	/*m_DeltaPos(0, 0) = deltaPos.x;
	m_DeltaPos(1, 0) = deltaPos.y;
	m_DeltaPos(2, 0) = deltaPos.z;*/

	{
		int columnIndex = 0;
		const Vector3 X_AXIS(1.0f, 0.0f, 0.0f);
		const Vector3 Y_AXIS(0.0f, 1.0f, 0.0f);
		const Vector3 Z_AXIS(0.0f, 0.0f, 1.0f);

		for (UINT64 i = TOTAL_JOINT - 1, end = 0; i > end; --i)
		{
			Joint* pJoint = &BodyChain[i];
			Joint* pChildJoint = &BodyChain[i + 1];
			Vector3 diff = pChildJoint->Position - pJoint->Position;
			//Vector3 diff = targetPos - pJoint->Position;
			if (i == end - 1)
			{
				diff = targetPos - pJoint->Position;
			}

			// 3-dof.
			Vector3 partialX = X_AXIS.Cross(diff);
			Vector3 partialY = Y_AXIS.Cross(diff);
			Vector3 partialZ = Z_AXIS.Cross(diff);

			m_JacobianMatrix.col(columnIndex) << partialX.x, partialX.y, partialX.z;
			m_JacobianMatrix.col(columnIndex + 1) << partialY.x, partialY.y, partialY.z;
			m_JacobianMatrix.col(columnIndex + 2) << partialZ.x, partialZ.y, partialZ.z;
			/*m_JacobianMatrix(0, columnIndex) = partialX.x;
			m_JacobianMatrix(1, columnIndex) = partialX.y;
			m_JacobianMatrix(2, columnIndex) = partialX.z;
			m_JacobianMatrix(0, columnIndex + 1) = partialY.x;
			m_JacobianMatrix(1, columnIndex + 1) = partialY.y;
			m_JacobianMatrix(2, columnIndex + 1) = partialY.z;
			m_JacobianMatrix(0, columnIndex + 2) = partialZ.x;
			m_JacobianMatrix(1, columnIndex + 2) = partialZ.y;
			m_JacobianMatrix(2, columnIndex + 2) = partialZ.z;*/

			columnIndex += 3;
		}
	}

	m_DeltaTheta = m_JacobianMatrix.bdcSvd(Eigen::ComputeThinU | Eigen::ComputeThinV).solve(m_DeltaPos);
	//m_DeltaTheta *= DELTA_TIME;
	{
		char szDebugString[256];
		sprintf_s(szDebugString, 256, "Theta 1: %f, %f, %f  Theta 2: %f, %f, %f  Theta 3: %f, %f, %f  Theta 4: %f, %f, %f\n",
				  m_DeltaTheta[0], m_DeltaTheta[1], m_DeltaTheta[2],
				  m_DeltaTheta[3], m_DeltaTheta[4], m_DeltaTheta[5],
				  m_DeltaTheta[6], m_DeltaTheta[7], m_DeltaTheta[8],
				  m_DeltaTheta[9], m_DeltaTheta[10], m_DeltaTheta[11]);
		OutputDebugStringA(szDebugString);
	}

	for (UINT64 i = 0, end = 3 * (TOTAL_JOINT - 1); i < end; ++i)
	{
		pDeltaThetas[i] = m_DeltaTheta[i];
	}

LB_RET:
	return bRetContinue;
}
