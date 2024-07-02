#include "../pch.h"
#include "AnimationData.h"

void AnimationData::Update(int clipID, int frame)
{
	AnimationClip& clip = Clips[clipID];

	for (UINT64 boneID = 0, totalTransformSize = BoneTransforms.size(); boneID < totalTransformSize; ++boneID)
	{
		std::vector<AnimationClip::Key>& keys = clip.Keys[boneID];
		const UINT64 KEY_SIZE = keys.size();

		const int PARENT_IDX = BoneParents[boneID];
		const Matrix& PARENT_MATRIX = (PARENT_IDX >= 0 ? BoneTransforms[PARENT_IDX] : AccumulatedRootTransform);

		AnimationClip::Key key = (KEY_SIZE > 0 ? keys[frame % KEY_SIZE] : AnimationClip::Key());

		// Root일 경우.
		if (PARENT_IDX < 0)
		{
			if (frame != 0)
			{
				AccumulatedRootTransform = (Matrix::CreateTranslation(key.Position - PrevPos) * AccumulatedRootTransform); // root 뼈의 변환을 누적시킴.
			}
			else
			{
				Vector3 temp = AccumulatedRootTransform.Translation();
				temp.y = key.Position.y;

				AccumulatedRootTransform.Translation(temp);
			}

			PrevPos = key.Position;
			key.Position = Vector3(0.0f);
		}

		BoneTransforms[boneID] = key.GetTransform() * PARENT_MATRIX;
	}
}
