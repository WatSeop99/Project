#pragma once

#include "Camera.h"
#include "ConstantDataType.h"
#include "ConstantBuffer.h"
#include "../Model/SkinnedMeshModel.h"
#include "Texture.h"

class ShadowMap
{
public:
	ShadowMap(UINT width = 1280, UINT height = 1280) : m_ShadowMapWidth(width), m_ShadowMapHeight(height) { }
	~ShadowMap() { Clear(); }

	void Initialize(ResourceManager* pManager, UINT lightType);

	void Update(ResourceManager* pManager, LightProperty& property, Camera& lightCam, Camera& mainCamera);

	void Render(ResourceManager* pManager, Model* pFirstRenderObject, SkinnedMeshModel* pCharacter, Model* pMirror);
	void Render(ResourceManager* pManager, ID3D12GraphicsCommandList* pCommandList, Model* pFirstRenderObject, SkinnedMeshModel* pCharacter, Model* pMirror);

	/*void Render(ResourceManager* pManager, std::vector<Model*>* pRenderObjects, SkinnedMeshModel* pCharacter, Model* pMirror);
	void Render(ResourceManager* pManager, ID3D12GraphicsCommandList* pCommandList, std::vector<Model*>* pRenderObjects, SkinnedMeshModel* pCharacter, Model* pMirror);*/


	void Clear();

	inline UINT GetShadowWidth() { return m_ShadowMapWidth; }
	inline UINT GetShadowHeight() { return m_ShadowMapHeight; }

	/*inline Texture* GetSpotLightShadowBufferPtr() { return &m_SpotLightShadowBuffer; }
	inline Texture* GetPointLightShadowBufferPtr() { return &m_PointLightShadowBuffer; }
	inline Texture* GetDirectionalLightShadowBufferPtr() { return &m_DirectionalLightShadowBuffer; }*/
	inline Texture* GetShadowBuffer() { return &m_ShadowBuffer; }

	inline ConstantBuffer* GetShadowConstantsBufferPtr() { return m_ShadowConstantBuffers; }

	inline void SetShadowWidth(const UINT WIDTH) { m_ShadowMapWidth = WIDTH; }
	inline void SetShadowHeight(const UINT HEIGHT) { m_ShadowMapHeight = HEIGHT; }

	void SetDescriptorHeap(ResourceManager* pManager);

protected:
	void setShadowViewport(ID3D12GraphicsCommandList* pCommandList);
	void setShadowScissorRect(ID3D12GraphicsCommandList* pCommandList);

	void calculateCascadeLightViewProjection(Vector3* pPosition, Matrix* pView, Matrix* pProjection, const Matrix& VIEW, const Matrix& PROJECTION, const Vector3& DIR, int cascadeIndex);

private:
	UINT m_ShadowMapWidth;
	UINT m_ShadowMapHeight;
	UINT m_LightType = LIGHT_OFF;
	const UINT m_TOTAL_LIGHT_TYPE = (LIGHT_DIRECTIONAL | LIGHT_POINT | LIGHT_SPOT);

	/*union
	{
		Texture m_SpotLightShadowBuffer;
		Texture m_PointLightShadowBuffer;
		Texture m_DirectionalLightShadowBuffer;
	};*/
	Texture m_ShadowBuffer;
	ConstantBuffer m_ShadowConstantBuffers[6];	 // spot, point, direc => 0, 6, 4개씩 사용.
	ConstantBuffer m_ShadowConstantsBufferForGS; // 2개 이상의 view 행렬을 사용하는 광원을 위한  geometry용 상수버퍼;
};
