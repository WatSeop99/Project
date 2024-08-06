#include <Eigen/Dense>
#include "../pch.h"
#include "../Util/Utility.h"
#include "AnimationData.h"

Matrix AnimationClip::Key::GetTransform()
{
	Quaternion newRot = Quaternion::Concatenate(Rotation, IKUpdateRotation);
	IKUpdateRotation = Quaternion();
	return (Matrix::CreateScale(Scale) * Matrix::CreateFromQuaternion(newRot) * Matrix::CreateTranslation(Position));
}

void AnimationData::Update(const int CLIP_ID, const int FRAME)
{
	AnimationClip& clip = Clips[CLIP_ID];

	// root bone id은 0(아닐 수 있음).
	// root bone에 대한 bone transform update.
	{
		const int ROOT_BONE_ID = 0;
		std::vector<AnimationClip::Key>& keys = clip.Keys[ROOT_BONE_ID];
		const UINT64 KEY_SIZE = keys.size();

		const int PARENT_ID = BoneParents[ROOT_BONE_ID];
		const Matrix& PARENT_MATRIX = AccumulatedRootTransform;
		AnimationClip::Key& key = keys[FRAME % KEY_SIZE];

		if (FRAME != 0)
		{
			// 걷기 시작하거나 멈추기 시작하는 동작은 자체 속도가 너무 큼. 따라서 0.25로 고정.
			Velocity = ((CLIP_ID == 1 || CLIP_ID == 3) ? 0.25f : (key.Position - PrevKeyPos).Length());
			AccumulatedRootTransform = Matrix::CreateFromQuaternion(Rotation) * Matrix::CreateTranslation(Position);
		}
		else
		{
			AccumulatedRootTransform.Translation(Position);
		}
		PrevKeyPos = key.Position;

		Quaternion newRot = Quaternion::Concatenate(key.Rotation, key.IKUpdateRotation);
		BoneTransforms[ROOT_BONE_ID] = Matrix::CreateScale(key.Scale) * Matrix::CreateFromQuaternion(newRot) * Matrix::CreateTranslation(Vector3(0.0f)) * PARENT_MATRIX;
		key.IKUpdateRotation = Quaternion();
	}

	// 나머지 bone transform 업데이트.
	// bone id가 부모->자식 순으로 선형적으로 저장되어 있기에 가능함.
	for (UINT64 boneID = 1, totalTransformSize = BoneTransforms.size(); boneID < totalTransformSize; ++boneID)
	{
		std::vector<AnimationClip::Key>& keys = clip.Keys[boneID];
		const UINT64 KEY_SIZE = keys.size();

		const int PARENT_ID = BoneParents[boneID];
		const Matrix& PARENT_MATRIX = BoneTransforms[PARENT_ID];
		AnimationClip::Key& key = keys[FRAME % KEY_SIZE];

		BoneTransforms[boneID] = key.GetTransform() * PARENT_MATRIX;
	}
}

void AnimationData::ResetAllUpdateRotationInClip(const int CLIP_ID)
{
	for (UINT64 i = 0, totalBone = BoneIDToNames.size(); i < totalBone; ++i)
	{
		std::vector<AnimationClip::Key>& keys = Clips[CLIP_ID].Keys[i];
		const UINT64 KEY_SIZE = keys.size();

		for (UINT j = 0; j < KEY_SIZE; ++j)
		{
			AnimationClip::Key& key = keys[j];
			key.IKUpdateRotation = Quaternion();
		}
	}
}

Matrix AnimationData::Get(const int BONE_ID)
{
	return (InverseDefaultTransform * OffsetMatrices[BONE_ID] * BoneTransforms[BONE_ID] * DefaultTransform);
}

Matrix AnimationData::GetRootBoneTransformWithoutLocalRot(const int CLIP_ID, const int FRAME)
{
	AnimationClip& clip = Clips[CLIP_ID];

	// root bone id은 0(아닐 수 있음).
	// root bone에 대한 bone transform update.
	const int ROOT_BONE_ID = 0;
	std::vector<AnimationClip::Key>& keys = clip.Keys[ROOT_BONE_ID];
	const UINT64 KEY_SIZE = keys.size();

	const int PARENT_ID = BoneParents[ROOT_BONE_ID];
	const Matrix& PARENT_MATRIX = AccumulatedRootTransform;
	AnimationClip::Key& key = keys[FRAME % KEY_SIZE];

	return (InverseDefaultTransform * OffsetMatrices[ROOT_BONE_ID] * Matrix::CreateScale(key.Scale) * Matrix::CreateTranslation(Vector3(0.0f)) * PARENT_MATRIX * DefaultTransform);
}

Matrix AnimationData::GetGlobalBonePositionMatix(const int BONE_ID)
{
	return (InverseDefaultTransform * BoneTransforms[BONE_ID] * DefaultTransform);
}

Joint::Joint()
{
	// 초기 제한 값은 최대로 해놓음.
	for (int i = 0; i < JointAxis_AxisCount; ++i)
	{
		AngleLimitation[i] = Vector2(-FLT_MAX, FLT_MAX);
	}
}

void Joint::Update(float deltaThetaX, float deltaThetaY, float deltaThetaZ, std::vector<AnimationClip>* pClips, Matrix* pDefaultTransform, Matrix* pInverseDefaultTransform, int clipID, int frame)
{
	_ASSERT(pClips);

	// 변환 각도.
	Quaternion deltaRot = Quaternion::CreateFromYawPitchRoll(deltaThetaY, deltaThetaX, deltaThetaZ);
	
	// 원래 키 데이터에서의 변환 각도.
	std::vector<AnimationClip::Key>& keys = (*pClips)[clipID].Keys[BoneID];
	const UINT64 KEY_SIZE = keys.size();
	AnimationClip::Key& key = keys[frame % KEY_SIZE];
	Quaternion originRot = key.Rotation;
	Quaternion prevUpdateRot = key.IKUpdateRotation;

	/*for (UINT64 i = 0; i < KEY_SIZE; ++i)
	{
		// 원래 키 데이터에서의 변환 각도.
		AnimationClip::Key& key = keys[i % KEY_SIZE];
		Quaternion originRot = key.Rotation;
		Quaternion prevUpdateRot = key.UpdateRotation;

		// 원래 각도에서 변환 각도 적용.
		Quaternion newUpdateRot = Quaternion::Concatenate(prevUpdateRot, deltaRot);
		key.UpdateRotation = newUpdateRot;
	}*/

	// 원래 각도에서 변환 각도 적용.
	Quaternion newUpdateRot = Quaternion::Concatenate(prevUpdateRot, deltaRot);

	// 적용 전, joint 전체 제한 값 테스트.
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

	// 적용.
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
	Eigen::MatrixXf J(3, 3 * TOTAL_JOINT); // 3차원, 관여 joint 2개임.
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
