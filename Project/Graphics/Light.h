#pragma once

#include "Camera.h"
#include "ShadowMap.h"

class Renderer;

class Light
{
public:
	Light(UINT width = 1280, UINT height = 1280);
	~Light() { Cleanup(); }

	void Initialize(Renderer* pRenderer);

	void Update(const float DELTA_TIME, Camera& mainCamera);

	void RenderShadowMap(std::vector<Model*>* pRenderObjects);

	void Cleanup();

	inline void SetPosition(const Vector3& POS) { m_LightViewCamera.SetEyePos(POS); }
	inline void SetDirection(const Vector3& DIR) { m_LightViewCamera.SetViewDir(DIR); }
	inline void SetShadowSize(const UINT WIDTH, const UINT HEIGHT) { LightShadowMap.SetShadowWidth(WIDTH); LightShadowMap.SetShadowHeight(HEIGHT); }
	
public:
	LightProperty Property;
	ShadowMap LightShadowMap;
	bool bRotated = false;
	bool bVisible = true;

private:
	Renderer* m_pRenderer = nullptr;

	Camera m_LightViewCamera;
	const UINT m_TOTAL_LIGHT_TYPE = (LIGHT_DIRECTIONAL | LIGHT_POINT | LIGHT_SPOT);
};
