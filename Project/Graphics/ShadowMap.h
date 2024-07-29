#pragma once

#include "Camera.h"
#include "../Renderer/ConstantDataType.h"
#include "../Model/SkinnedMeshModel.h"
#include "Texture.h"
#include "../Renderer/TextureManager.h"

class Renderer;

class ShadowMap
{
public:
	ShadowMap(UINT width = 1280, UINT height = 1280) : m_ShadowMapWidth(width), m_ShadowMapHeight(height) { }
	~ShadowMap() { Cleanup(); }

	void Initialize(Renderer* pRenderer, UINT lightType);

	void Update(LightProperty& property, Camera& lightCam, Camera& mainCamera);

	void Render(std::vector<Model*>* pRenderObjects);

	void Cleanup();

	inline UINT GetShadowWidth() { return m_ShadowMapWidth; }
	inline UINT GetShadowHeight() { return m_ShadowMapHeight; }

	inline TextureHandle* GetSpotLightShadowBufferPtr() { return m_pSpotLightShadowBuffer; }
	inline TextureHandle* GetPointLightShadowBufferPtr() { return m_pPointLightShadowBuffer; }
	inline TextureHandle* GetDirectionalLightShadowBufferPtr() { return m_pDirectionalLightShadowBuffer; }

	inline GlobalConstant* GetShadowConstantsBufferDataPtr() { return m_ShadowConstantBufferDatas; }
	inline ShadowConstant* GetShadowConstantBufferDataForGSPtr() { return &m_ShadowConstantsBufferDataForGS; }

	void SetShadowWidth(const UINT WIDTH);
	void SetShadowHeight(const UINT HEIGHT);

	void SetViewportsAndScissorRect(ID3D12GraphicsCommandList* pCommandList);

protected:
	void setShadowViewport(ID3D12GraphicsCommandList* pCommandList);
	void setShadowScissorRect(ID3D12GraphicsCommandList* pCommandList);

	void calculateCascadeLightViewProjection(Vector3* pPosition, Matrix* pView, Matrix* pProjection, const Matrix& VIEW, const Matrix& PROJECTION, const Vector3& DIR, int cascadeIndex);

private:
	Renderer* m_pRenderer = nullptr;

	UINT m_ShadowMapWidth;
	UINT m_ShadowMapHeight;
	UINT m_LightType = LIGHT_OFF;
	const UINT m_TOTAL_LIGHT_TYPE = (LIGHT_DIRECTIONAL | LIGHT_POINT | LIGHT_SPOT);

	D3D12_VIEWPORT m_pViewPorts[6] = { 0.0f, };
	D3D12_RECT m_pScissorRects[6] = { 0, };

	union
	{
		TextureHandle* m_pSpotLightShadowBuffer;
		TextureHandle* m_pPointLightShadowBuffer;
		TextureHandle* m_pDirectionalLightShadowBuffer;
	};
	GlobalConstant m_ShadowConstantBufferDatas[6];	 // spot, point, direc => 0, 6, 4개씩 사용.
	ShadowConstant m_ShadowConstantsBufferDataForGS; // 2개 이상의 view 행렬을 사용하는 광원을 위한  geometry용 상수버퍼;
};
