#pragma once

#include "../Graphics/Light.h"
#include "../Util/LinkedList.h"
#include "../Model/Model.h"
#include "../Renderer/Renderer.h"
#include "../Util/Utility.h"

class App final : public Renderer
{
public:
	App() = default;
	~App() { Cleanup(); }

	void Initialize();

	int Run();

	void Update(const float DELTA_TIME);

	void Cleanup();

protected:
	void initExternalData();

	void updateAnimationState(SkinnedMeshModel* pCharacter, const float DELTA_TIME, int* pState, int* pFrame, Vector3* pDeltapos, bool* pEndEffectorUpdateFlag);
	void updateEndEffectorPosition(SkinnedMeshModel* pCharacter, SkinnedMeshModel::JointUpdateInfo* pUpdateInfo);
	void simulateCharacterContol(SkinnedMeshModel* pCharacter, const Vector3& DELTA_POS, const float DELTA_TIME, const int CLIP_ID, const int FRAME, bool* pEndEffectorUpdateFlag);

private:
	// data
	std::vector<Model*> m_RenderObjects;
	std::vector<SkinnedMeshModel*> m_Characters;

	std::vector<Light> m_Lights;
	std::vector<Model*> m_LightSpheres;

	SkinnedMeshModel* m_pCharacter = nullptr; // main character
	DirectX::SimpleMath::Plane m_MirrorPlane;
};

