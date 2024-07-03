#pragma once

#include "../Renderer/KnMControl.h"
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
	void initScene(ULONGLONG* pTotalRenderObject);

	void updateAnimation(const float DELTA_TIME);

private:
	Renderer* m_pRenderer = nullptr;

	Timer m_Timer;

	Keyboard m_Keyboard;
	Mouse m_Mouse;

	// data
	/*std::vector<Model*> m_RenderObjects;
	std::vector<Light> m_Lights;
	std::vector<Model*> m_LightSpheres;*/

	ListElem* m_pRenderObjectsHead = nullptr;
	ListElem* m_pRenderObjectsTail = nullptr;
	Container* m_Lights = nullptr;
	Container* m_LightSpheres = nullptr;

	Texture m_EnvTexture;
	Texture m_IrradianceTexture;
	Texture m_SpecularTexture;
	Texture m_BRDFTexture;
	
	Model* m_pSkybox = nullptr;
	Model* m_pGround = nullptr;
	Model* m_pMirror = nullptr;
	Model* m_pPickedModel = nullptr;
	Model* m_pCharacter = nullptr;
	DirectX::SimpleMath::Plane m_MirrorPlane;
};

