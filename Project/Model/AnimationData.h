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
struct CharacterMoveInfo
{
	Vector3 Position;
	Vector3 Direction; // direction은 회전 방향만 결정.
	Quaternion Rotation;
	float Velocity;
};

class AnimationData
{
public:
	AnimationData() = default;
	~AnimationData() = default;

	void Update(int clipID, int frame, const CharacterMoveInfo& MOVE_INFO);

	void ResetAllUpdateRotationInClip(int clipID);

	inline Matrix Get(int boneID)
	{
		return (InverseDefaultTransform * OffsetMatrices[boneID] * BoneTransforms[boneID] * DefaultTransform);
	}
	Matrix GetRootBoneTransformWithoutLocalRot(int clipID, int frame);

public:
	std::unordered_map<std::string, int> BoneNameToID; // 뼈 이름과 인덱스 정수.
	std::vector<std::string> BoneIDToNames;				// BoneNameToID의 ID 순서대로 뼈 이름 저장.
	std::vector<int> BoneParents;					    // 부모 뼈의 인덱스.
	std::vector<Matrix> OffsetMatrices;					// root 뼈로부터 위치 offset 변환 행렬.
	std::vector<Matrix> GlobalTransforms;				// 모델 좌표계 내 각 뼈들의 위치.
	std::vector<Matrix> BoneTransforms;					// 해당 시점 key data의 움직임에 따른 뼈의 변환 행렬.
	std::vector<AnimationClip> Clips;					// 애니메이션 동작.

	Matrix DefaultTransform;		// normalizing을 위한 변환 행렬 [-1, 1]^3
	Matrix InverseDefaultTransform;	// 모델 좌표계 복귀 변환 행렬.
	Matrix RootTransform;
	Matrix AccumulatedRootTransform;
	Vector3 PrevPos;
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

protected:
	

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

	Matrix CharacterWorld;	// 캐릭터 world.
	Matrix Correction;		// world를 위한 보정값.
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
