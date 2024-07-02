#pragma once

#include "Camera.h"
#include "ShadowMap.h"

class Light
{
public:
	Light(UINT width = 1280, UINT height = 1280);
	~Light() { Clear(); }

	void Initialize(ResourceManager* pManager);

	void Update(ResourceManager* pManager, const float DELTA_TIME, Camera& mainCamera);

	void RenderShadowMap(ResourceManager* pManager, std::vector<Model*>& renderObjects, SkinnedMeshModel* pCharacter, Model* pMirror);
	void RenderShadowMap(ResourceManager* pManager, ID3D12GraphicsCommandList* pCommandList, std::vector<Model*>& renderObjects, SkinnedMeshModel* pCharacter, Model* pMirror);

	void Clear();

	inline void SetPosition(const Vector3& POS) { m_LightViewCamera.SetEyePos(POS); }
	inline void SetDirection(const Vector3& DIR) { m_LightViewCamera.SetViewDir(DIR); }
	inline void SetShadowSize(const UINT WIDTH, const UINT HEIGHT) { ShadowMap.SetShadowWidth(WIDTH); ShadowMap.SetShadowHeight(HEIGHT); }

public:
	LightProperty Property;
	ShadowMap ShadowMap;
	bool bRotated = false;
	bool bVisible = true;

private:
	Camera m_LightViewCamera;
	const UINT m_TOTAL_LIGHT_TYPE = (LIGHT_DIRECTIONAL | LIGHT_POINT | LIGHT_SPOT);
};
