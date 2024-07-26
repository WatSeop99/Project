#pragma once

#include "../Graphics/Light.h"
#include "../Util/LinkedList.h"
#include "../Model/Model.h"
#include "../Renderer/Renderer.h"
#include "../Util/Utility.h"
#include "../Renderer/Timer.h"

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
	void initExternalData(UINT64* pTotalRenderObjectCount);

	void updateAnimationState(SkinnedMeshModel* pCharacter, const float DELTA_TIME, int* pState, int* pFrame, SkinnedMeshModel::JointUpdateInfo* pUpdateInfo);
	void simulateCharacterContol(SkinnedMeshModel* pCharacter, SkinnedMeshModel::JointUpdateInfo* pUpdateInfo, const Vector3& DELTA_POS, const float DELTA_TIME);

private:
	Timer m_Timer;

	// data
	std::vector<Model*> m_RenderObjects;

	std::vector<Light> m_Lights;
	std::vector<Model*> m_LightSpheres;

	Texture m_EnvTexture;
	Texture m_IrradianceTexture;
	Texture m_SpecularTexture;
	Texture m_BRDFTexture;

	TextureHandle* m_pEnvTexture = nullptr;
	TextureHandle* m_pIrradianceTexture = nullptr;
	TextureHandle* m_pSpecularTexture = nullptr;
	TextureHandle* m_pBRDFTexture = nullptr;
	
	Model* m_pMirror = nullptr;
	SkinnedMeshModel* m_pCharacter = nullptr; // main character
	DirectX::SimpleMath::Plane m_MirrorPlane;
};

