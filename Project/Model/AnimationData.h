#pragma once

#include <directxtk12/SimpleMath.h>
#include <unordered_map>
#include <vector>
#include <string>

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
		Quaternion IKUpdateRotation;
	};

	std::string Name;					 // Name of this animation clip.
	std::vector<std::vector<Key>> Keys;  // Keys[boneID][frame].
	int NumChannels;					 // Number of bones.
	int NumKeys;						 // Number of frames of this animation clip.
	double Duration;					 // Duration of animation in ticks.
	double TicksPerSec;					 // Frames per second.
};

class AnimationData
{
public:
	AnimationData() = default;
	~AnimationData() = default;

	void Update(const int CLIP_ID, const int FRAME);

	void ResetAllUpdateRotationInClip(const int CLIP_ID);

	Matrix Get(const int BONE_ID);
	Matrix GetRootBoneTransformWithoutLocalRot(const int CLIP_ID, const int FRAME);

public:
	std::unordered_map<std::string, int> BoneNameToID;	// �� �̸��� �ε��� ����.
	std::vector<std::string> BoneIDToNames;				// BoneNameToID�� ID ������� �� �̸� ����.
	std::vector<int> BoneParents;					    // �θ� ���� �ε���.
	std::vector<Matrix> OffsetMatrices;					// ���� skin ������ ��ȯ. �� ��ǥ�迡�� mesh�� ��ġ.	
	std::vector<Matrix> BoneTransforms;					// �ش� ���� key data�� �����ӿ� ���� ���� ��ȯ ���.
	std::vector<AnimationClip> Clips;					// �ִϸ��̼� ����.

	Matrix DefaultTransform;			// normalizing�� ���� ��ȯ ��� [-1, 1]^3
	Matrix InverseDefaultTransform;		// �� ��ǥ�� ���� ��ȯ ���.
	Matrix AccumulatedRootTransform;	// root ���� ������ ��ȯ ���.
	Matrix InverseRootGlobalTransform;
	Vector3 PrevKeyPos;					// ���� clip�� Ű ������ ��ġ.
	Vector3 Position;					// ĳ���� ��ġ.
	Vector3 Direction;					// direction�� ȸ�� ���⸸ ����.
	Quaternion Rotation;				// ȸ�� ����.
	float Velocity = 0.0f;
};

class Joint
{
public:
	Joint();
	~Joint() = default;

	void Update(float deltaX, float deltaY, float deltaZ, std::vector<AnimationClip>* pClips, Matrix* pDefaultTransform, Matrix* pInverseDefaultTransform, int clipID, int frame);

	void JacobianX(Vector3* pOutput, Vector3& parentPos);
	void JacobianY(Vector3* pOutput, Vector3& parentPos);
	void JacobianZ(Vector3* pOutput, Vector3& parentPos);

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

	Matrix CharacterWorld;	// ĳ���� world.
	Matrix Correction;		// world�� ���� ������.
};
class Chain
{
public:
	Chain() = default;
	~Chain() = default;

	void SolveIK(Vector3& targetPos, int clipID, int frame, const float DELTA_TIME);

public:
	std::vector<Joint> BodyChain; // root ~ child.
	std::vector<AnimationClip>* pAnimationClips = nullptr;
	Matrix DefaultTransform;
	Matrix InverseDefaultTransform;
};
