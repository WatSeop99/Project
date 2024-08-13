#include <Eigen/Dense>
#include "../pch.h"
#include "../Util/Utility.h"
#include "AnimationData.h"

Matrix AnimationClip::Key::GetTransform()
{
	Quaternion adjustedRot = Rotation;
	if (IKUpdateRotation != Quaternion())
	{
		adjustedRot = Quaternion::Concatenate(Rotation, IKUpdateRotation);
		IKUpdateRotation = Quaternion();
	}
	return (Matrix::CreateScale(Scale) * Matrix::CreateFromQuaternion(adjustedRot) * Matrix::CreateTranslation(Position));
}

void AnimationData::Update(const int CLIP_ID, const int FRAME, const float DELTA_TIME)
{
	TimeSinceLoaded += DELTA_TIME;

	AnimationClip& clip = Clips[CLIP_ID];
	float timeInTicks = (float)(TimeSinceLoaded * clip.TicksPerSec);
	float animationTimeTicks = fmod(timeInTicks, (float)clip.Duration);

	// root bone id�� 0(�ƴ� �� ����).
	// root bone�� ���� bone transform update.
	{
		const int ROOT_BONE_ID = 0;
		std::vector<AnimationClip::Key>& keys = clip.Keys[ROOT_BONE_ID];
		const UINT64 KEY_SIZE = keys.size();
		AnimationClip::Key& key = keys[FRAME % KEY_SIZE];

		//if (FRAME != 0)
		//{
		//	AnimationClip::Key& nextKey = keys[(FRAME + 1) % KEY_SIZE];

		//	// �ȱ� �����ϰų� ���߱� �����ϴ� ������ ��ü �ӵ��� �ʹ� ŭ. ���� 0.25�� ����.
		//	// Velocity = ((CLIP_ID == 1 || CLIP_ID == 3) ? 0.25f : (key.Position - PrevKeyPos).Length());
		//	// Velocity = ((CLIP_ID == 1 || CLIP_ID == 3) ? 0.25f : (nextKey.Position - key.Position).Length());
		//	AccumulatedRootTransform = Matrix::CreateFromQuaternion(Rotation) * Matrix::CreateTranslation(Position);
		//}
		//else
		//{
		//	AccumulatedRootTransform.Translation(Position);
		//}

		/*Quaternion adjustedRot = key.Rotation;
		if (key.IKUpdateRotation != Quaternion())
		{
			adjustedRot = Quaternion::Concatenate(key.Rotation, key.IKUpdateRotation);
			key.IKUpdateRotation = Quaternion();
		}
		BoneTransforms[ROOT_BONE_ID] = Matrix::CreateScale(key.Scale) * Matrix::CreateFromQuaternion(adjustedRot);*/
		
		Vector3 interpolatedPos;
		Vector3 interpolatedScale;
		Quaternion interpolatedRot;
		interpolateKeyData(&interpolatedPos, &interpolatedRot, &interpolatedScale, &clip, 0, animationTimeTicks);
		
		Quaternion adjustedRot = interpolatedRot;
		if (key.IKUpdateRotation != Quaternion())
		{
			adjustedRot = Quaternion::Concatenate(interpolatedRot, key.IKUpdateRotation);
			key.IKUpdateRotation = Quaternion();
		}
		BoneTransforms[ROOT_BONE_ID] = Matrix::CreateScale(interpolatedScale) * Matrix::CreateFromQuaternion(adjustedRot);
	}

	// ������ bone transform ������Ʈ.
	// bone id�� �θ�->�ڽ� ������ ���������� ����Ǿ� �ֱ⿡ ������.
	for (UINT64 boneID = 1, totalTransformSize = BoneTransforms.size(); boneID < totalTransformSize; ++boneID)
	{
		std::vector<AnimationClip::Key>& keys = clip.Keys[boneID];
		const UINT64 KEY_SIZE = keys.size();

		const int PARENT_ID = BoneParents[boneID];
		AnimationClip::Key& key = keys[FRAME % KEY_SIZE];

		// BoneTransforms[boneID] = key.GetTransform() * BoneTransforms[PARENT_ID];

		Vector3 interpolatedPos;
		Vector3 interpolatedScale;
		Quaternion interpolatedRot;
		interpolateKeyData(&interpolatedPos, &interpolatedRot, &interpolatedScale, &clip, (const int)boneID, animationTimeTicks);

		Quaternion adjustedRot = interpolatedRot;
		if (key.IKUpdateRotation != Quaternion())
		{
			adjustedRot = Quaternion::Concatenate(interpolatedRot, key.IKUpdateRotation);
			key.IKUpdateRotation = Quaternion();
		}
		BoneTransforms[boneID] = Matrix::CreateScale(interpolatedScale) * Matrix::CreateFromQuaternion(adjustedRot) * Matrix::CreateTranslation(interpolatedPos) * BoneTransforms[PARENT_ID];
	}
}

void AnimationData::UpdateVelocity(const int CLIP_ID, const int FRAME)
{
	if (CLIP_ID == 0)
	{
		Velocity = 0.0f;
		return;
	}

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

Matrix AnimationData::Get(const int CLIP_ID, const int FRAME, const int BONE_ID)
{
	return (InverseDefaultTransform * OffsetMatrices[BONE_ID] * BoneTransforms[BONE_ID] * InverseOffsetMatrices[0] * DefaultTransform);
}

Matrix AnimationData::GetRootBoneTransformWithoutLocalRot(const int CLIP_ID, const int FRAME)
{
	AnimationClip& clip = Clips[CLIP_ID];

	// root bone id�� 0(�ƴ� �� ����).
	// root bone�� ���� bone transform update.
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

void AnimationData::interpolateKeyData(Vector3* pOutPosition, Quaternion* pOutRotation, Vector3* pOutScale, AnimationClip* pClip, const int BONE_ID, const float ANIMATION_TIME_TICK)
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

	UINT positionIndex = findIndex(pClip, BONE_ID, ANIMATION_TIME_TICK);
	UINT nextPositionIndex = positionIndex + 1;
	_ASSERT(nextPositionIndex < KEY_SIZE);

	AnimationClip::Key& curKey = keys[positionIndex];
	AnimationClip::Key& nextKey = keys[nextPositionIndex];

	float t1 = (float)curKey.Time;
	float t2 = (float)nextKey.Time;
	float deltaTime = t2 - t1;
	float factor = (ANIMATION_TIME_TICK - t1) / deltaTime;

	// Get position data.
	const Vector3& START_POS = curKey.Position;
	const Vector3& END_POS = nextKey.Position;
	Vector3 deltaPos = END_POS - START_POS;
	*pOutPosition = START_POS + factor * deltaPos;

	// Get rotation data.
	const Quaternion& START_ROT = curKey.Rotation;
	const Quaternion& END_ROT = nextKey.Rotation;
	Quaternion interporlated = DirectX::XMQuaternionSlerp(START_ROT, END_ROT, factor);
	interporlated.Normalize();
	*pOutRotation = interporlated;

	// Get scale data.
	const Vector3& START_SCALE = curKey.Scale;
	const Vector3& END_SCALE = nextKey.Scale;
	Vector3 deltaScale = END_SCALE - START_SCALE;
	*pOutScale = START_SCALE + factor * deltaScale;
}

UINT AnimationData::findIndex(AnimationClip* pClip, const int BONE_ID, const float ANIMATION_TIME_TICK)
{
	_ASSERT(pClip);

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
	// �ʱ� ���� ���� �ִ�� �س���.
	for (int i = 0; i < JointAxis_AxisCount; ++i)
	{
		AngleLimitation[i] = Vector2(-FLT_MAX, FLT_MAX);
	}
}

void Joint::Update(float deltaThetaX, float deltaThetaY, float deltaThetaZ, std::vector<AnimationClip>* pClips, Matrix* pDefaultTransform, Matrix* pInverseDefaultTransform, int clipID, int frame)
{
	_ASSERT(pClips);

	// ��ȯ ����.
	Quaternion deltaRot = Quaternion::CreateFromYawPitchRoll(deltaThetaY, deltaThetaX, deltaThetaZ);
	
	// ���� Ű �����Ϳ����� ��ȯ ����.
	std::vector<AnimationClip::Key>& keys = (*pClips)[clipID].Keys[BoneID];
	const UINT64 KEY_SIZE = keys.size();
	AnimationClip::Key& key = keys[frame % KEY_SIZE];
	Quaternion originRot = key.Rotation;
	Quaternion prevUpdateRot = key.IKUpdateRotation;

	/*for (UINT64 i = 0; i < KEY_SIZE; ++i)
	{
		// ���� Ű �����Ϳ����� ��ȯ ����.
		AnimationClip::Key& key = keys[i % KEY_SIZE];
		Quaternion originRot = key.Rotation;
		Quaternion prevUpdateRot = key.UpdateRotation;

		// ���� �������� ��ȯ ���� ����.
		Quaternion newUpdateRot = Quaternion::Concatenate(prevUpdateRot, deltaRot);
		key.UpdateRotation = newUpdateRot;
	}*/

	// ���� �������� ��ȯ ���� ����.
	Quaternion newUpdateRot = Quaternion::Concatenate(prevUpdateRot, deltaRot);

	// ���� ��, joint ��ü ���� �� �׽�Ʈ.
	Matrix newUpdateRotMat = Matrix::CreateFromQuaternion(newUpdateRot);
	float pitch = asin(-newUpdateRotMat._23);
	float yaw = atan2(newUpdateRotMat._13, newUpdateRotMat._33);
	float roll = atan2(newUpdateRotMat._21, newUpdateRotMat._22);
	bool bUpdateFlag = false;

	if (pitch < AngleLimitation[JointAxis_X].x || pitch > AngleLimitation[JointAxis_X].y)
	{
		deltaThetaX = 0.0f;
		bUpdateFlag = true;
	}
	if (yaw < AngleLimitation[JointAxis_Y].x || yaw > AngleLimitation[JointAxis_Y].y)
	{
		deltaThetaY = 0.0f;
		bUpdateFlag = true;
	}
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

	// ����.
	// key.UpdateRotation = newUpdateRot;
	/*for (UINT64 i = 0; i < KEY_SIZE; ++i)
	{
		AnimationClip::Key& key = keys[i % KEY_SIZE];
		key.IKUpdateRotation = newUpdateRot;
	}*/
	keys[frame % KEY_SIZE].IKUpdateRotation = newUpdateRot;
}

void Joint::JacobianX(Vector3* pOutput, Vector3& parentPos)
{
	Vector3 xAxis(1.0f, 0.0f, 0.0f);
	Vector3 diff = Position - parentPos;
	
	*pOutput = xAxis.Cross(diff);
}

void Joint::JacobianY(Vector3* pOutput, Vector3& parentPos)
{
	Vector3 yAxis(0.0f, 1.0f, 0.0f);
	Vector3 diff = Position - parentPos;

	*pOutput = yAxis.Cross(diff);
}

void Joint::JacobianZ(Vector3* pOutput, Vector3& parentPos)
{
	Vector3 zAxis(0.0f, 0.0f, 1.0f);
	Vector3 diff = Position - parentPos;

	*pOutput = zAxis.Cross(diff);
}

void Chain::SolveIK(Vector3& targetPos, int clipID, int frame, const float DELTA_TIME)
{
	_ASSERT(BodyChain.size() > 0);

	const UINT64 TOTAL_JOINT = BodyChain.size();
	Eigen::MatrixXf J(3, 3 * TOTAL_JOINT); // 3����, ���� joint 2����.
	Eigen::MatrixXf b(3, 1);
	Eigen::VectorXf deltaTheta(TOTAL_JOINT * 3);
	Joint& endEffector = BodyChain[TOTAL_JOINT - 1];

	for (int step = 0; step < 20; ++step)
	{
		Vector3 deltaPos = targetPos - endEffector.Position;
		float deltaPosLength = deltaPos.Length();
		/*{
			char szDebugString[256];
			sprintf_s(szDebugString, 256, "deltaPosLength: %f\n", deltaPosLength);
			OutputDebugStringA(szDebugString);
		}*/

		if (deltaPosLength <= 0.2f || deltaPosLength >= 0.5f)
		{
			break;
		}

		b << deltaPos.x, deltaPos.y, deltaPos.z;

		int columnIndex = 0;
		for (UINT64 i = 0; i < TOTAL_JOINT; ++i)
		{
			Joint* pJoint = &BodyChain[i];
			Vector3 jointPos = pJoint->Position;
			Vector3 partialX;
			Vector3 partialY;
			Vector3 partialZ;

			endEffector.JacobianX(&partialX, jointPos);
			endEffector.JacobianY(&partialY, jointPos);
			endEffector.JacobianZ(&partialZ, jointPos);

			J.col(columnIndex) << partialX.x, partialX.y, partialX.z;
			J.col(columnIndex + 1) << partialY.x, partialY.y, partialY.z;
			J.col(columnIndex + 2) << partialZ.x, partialZ.y, partialZ.z;
			
			columnIndex += 3;
		}

		deltaTheta = J.bdcSvd(Eigen::ComputeThinU | Eigen::ComputeThinV).solve(b);
		deltaTheta *= DELTA_TIME;
		/*{
			char szDebugString[256];
			sprintf_s(szDebugString, 256, "Theta 1: %f, %f, %f  Theta 2: %f, %f, %f  Theta 3: %f, %f, %f\n", 
					  deltaTheta[0], deltaTheta[1], deltaTheta[2], deltaTheta[3], deltaTheta[4], deltaTheta[5], deltaTheta[6], deltaTheta[7], deltaTheta[8]);
			OutputDebugStringA(szDebugString);
		}*/
		columnIndex = 0;
		for (UINT64 i = 0; i < TOTAL_JOINT; ++i)
		{
			Joint* pJoint = &BodyChain[i];
			pJoint->Update(deltaTheta[columnIndex], deltaTheta[columnIndex + 1], deltaTheta[columnIndex + 2], pAnimationClips, &DefaultTransform, &InverseDefaultTransform, clipID, frame);
			columnIndex += 3;
		}
	}
}
