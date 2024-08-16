#pragma once

#include <directxtk12/SimpleMath.h>
#include <unordered_map>
#include <vector>
#include <string>

//namespace Eigen
//{
//	/*template <typename T0, typename T1>
//	class MatrixXf<T0, T1>;*/
//
//	/*template <typename T>
//	class VectorXf<T>;*/
//}

using DirectX::SimpleMath::Matrix;
using DirectX::SimpleMath::Quaternion;
using DirectX::SimpleMath::Vector3;
using DirectX::SimpleMath::Vector2;

struct AnimationClip
{
	class Key
	{
	public:
		Key() = default;
		~Key() = default;

		Matrix GetTransform();

	public:
		Vector3 Position;
		Vector3 Scale = Vector3(1.0f);
		Quaternion Rotation;
		// Quaternion IKUpdateRotation;
		double Time = 0.0f;
	};

	std::string Name;					 // Name of this animation clip.
	std::vector<std::vector<Key>> Keys;  // Keys[boneID][frame or time].
	std::vector<Quaternion> IKRotations;
	int NumChannels;					 // Number of bones.
	double Duration;					 // Duration of animation in ticks.
	double TicksPerSec;					 // Frames per second.
};

class AnimationData
{
public:
	AnimationData() = default;
	~AnimationData() = default;

	void Update(const int CLIP_ID, const int FRAME, const float DELTA_TIME);
	void UpdateForIK(const int CLIP_ID, const int FRAME);
	void UpdateVelocity(const int CLIP_ID, const int FRAME);

	void ResetAllIKRotations(const int CLIP_ID);

	Matrix Get(const int CLIP_ID, const int FRAME, const int BONE_ID);
	Matrix GetRootBoneTransformWithoutLocalRot(const int CLIP_ID, const int FRAME);
	Matrix GetGlobalBonePositionMatix(const int CLIP_ID, const int FRAME, const int BONE_ID);

protected:
	void interpolateKeyData(Vector3* pOutPosition, Quaternion* pOutRotation, Vector3* pOutScale, AnimationClip* pClip, const int BONE_ID, const float ANIMATION_TIME_TICK);

	UINT findIndex(AnimationClip* pClip, const int BONE_ID, const float ANIMATION_TIME_TICK);

public:
	std::unordered_map<std::string, int> BoneNameToID;	// �� �̸��� �ε��� ����.
	std::vector<std::string> BoneIDToNames;				// BoneNameToID�� ID ������� �� �̸� ����.
	std::vector<int> BoneParents;					    // �θ� ���� �ε���.
	std::vector<Matrix> OffsetMatrices;					// ���� skin ������ ��ȯ. �� ��ǥ�迡�� mesh�� ��ġ.	
	std::vector<Matrix> InverseOffsetMatrices;		
	std::vector<Matrix> NodeTransforms;
	std::vector<Matrix> BoneTransforms;					// �ش� ���� key data�� �����ӿ� ���� ���� ��ȯ ���.
	std::vector<AnimationClip> Clips;					// �ִϸ��̼� ����.

	Matrix DefaultTransform;			// normalizing�� ���� ��ȯ ��� [-1, 1]^3
	Matrix InverseDefaultTransform;		// �� ��ǥ�� ���� ��ȯ ���.
	Vector3 PrevKeyPos;					// ���� clip�� Ű ������ ��ġ.
	Vector3 Position;					// ĳ���� ��ġ.
	Vector3 Direction;					// direction�� ȸ�� ���⸸ ����.
	Quaternion Rotation;				// ȸ�� ����.

	double TimeSinceLoaded = 0.25f;
	float NormalizingScale = 1.0f;
	float Velocity = 0.0f;
};

class Joint
{
public:
	Joint();
	~Joint() = default;

	void ApplyJacobian(float deltaX, float deltaY, float deltaZ, std::vector<AnimationClip>* pClips, int clipID, int frame);

public:
	enum eJointAxis
	{
		JointAxis_X = 0,
		JointAxis_Y,
		JointAxis_Z,
		JointAxis_AxisCount
	};

	UINT BoneID = 0xffffffff;

	Vector3 Position;
	Vector2 AngleLimitation[JointAxis_AxisCount]; // for all axis x, y, z. AngleLimitation[i].x = lower, AngleLimitation[i].y = upper.

	Matrix* pOffset = nullptr; 
	Matrix* pParentMatrix = nullptr;	// parent bone transform.
	Matrix* pJointTransform = nullptr; // bone transform.
};
class Chain
{
public:
	Chain() = default;
	~Chain() = default;

	void Initialize(const int BODY_CHAIN_SIZE);

	void Reset();

	void SolveIK(AnimationData* pAnimationData, Vector3& targetPos, float* pDeltaThetas, const int CLIP_ID, const int FRAME, const float DELTA_TIME);

public:
	std::vector<Joint> BodyChain; // root ~ child.

private:
	Eigen::MatrixXf m_JacobianMatrix;
	Eigen::MatrixXf m_DeltaPos;
	Eigen::VectorXf m_DeltaTheta;
};
