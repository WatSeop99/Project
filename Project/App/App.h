#pragma once

#include "../Graphics/Light.h"
#include "../Util/LinkedList.h"
#include "../Model/Model.h"
#include "../Renderer/Renderer.h"
#include "../Util/Utility.h"

class App
{
public:
	App() = default;
	~App() { Clear(); }

	void Initialize();

	int Run();

	void Update(const float DELTA_TIME);

	void Clear();

protected:
	void initScene();

	void updateAnimation(const float DELTA_TIME);

private:
	Renderer* m_pRenderer = nullptr;

	Timer m_Timer;

	// data
	/*std::vector<Model*> m_RenderObjects;
	std::vector<Light> m_Lights;*/
	Container* m_RenderObjects = nullptr;

	ListElem* m_pRenderObjectsHead = nullptr;
	ListElem* m_pRenderObjectsTail = nullptr;

	Container* m_Lights = nullptr;

	Texture m_EnvTexture;
	Texture m_IrradianceTexture;
	Texture m_SpecularTexture;
	Texture m_BRDFTexture;

	// std::vector<Model*> m_LightSpheres;
	Container* m_LightSpheres = nullptr;
	Model* m_pSkybox = nullptr;
	Model* m_pGround = nullptr;
	Model* m_pMirror = nullptr;
	Model* m_pPickedModel = nullptr;
	Model* m_pCharacter = nullptr;
	DirectX::SimpleMath::Plane m_MirrorPlane;
};

