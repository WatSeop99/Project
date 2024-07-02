#include "../pch.h"
#include "ConstantDataType.h"
#include "Light.h"

Light::Light(UINT width, UINT height) : ShadowMap(width, height)
{
	m_LightViewCamera.bUseFirstPersonView = true;
	m_LightViewCamera.SetAspectRatio((float)width / (float)height);
	m_LightViewCamera.SetEyePos(Property.Position);
	m_LightViewCamera.SetViewDir(Property.Direction);
	m_LightViewCamera.SetProjectionFovAngleY(120.0f);
	m_LightViewCamera.SetNearZ(0.1f);
	m_LightViewCamera.SetFarZ(50.0f);
}

void Light::Initialize(ResourceManager* pManager)
{
	Clear();

	switch (Property.LightType)
	{
		case LIGHT_DIRECTIONAL:
			m_LightViewCamera.SetFarZ(500.0f);
			ShadowMap.SetShadowWidth(2560);
			ShadowMap.SetShadowHeight(2560);
			break;

		case LIGHT_POINT:
			m_LightViewCamera.SetProjectionFovAngleY(90.0f);
			break;

		case LIGHT_SPOT:
		default:
			break;
	}

	ShadowMap.Initialize(pManager, Property.LightType);
}

void Light::Update(ResourceManager* pManager, const float DELTA_TIME, Camera& mainCamera)
{
	static Vector3 s_LightDev = Vector3(1.0f, 0.0f, 0.0f);
	if (bRotated)
	{
		s_LightDev = Vector3::Transform(s_LightDev, Matrix::CreateRotationY(DELTA_TIME * DirectX::XM_PI * 0.5f));

		Vector3 focusPosition = Vector3(0.0f, -0.5f, 1.7f);
		Property.Position = Vector3(0.0f, 1.1f, 2.0f) + s_LightDev;
		Property.Direction = focusPosition - Property.Position;
	}

	Property.Direction.Normalize();
	m_LightViewCamera.SetEyePos(Property.Position);
	m_LightViewCamera.SetViewDir(Property.Direction);

	if (Property.LightType & LIGHT_SHADOW)
	{
		Vector3 up = m_LightViewCamera.GetUpDir();
		if (fabs(up.Dot(Property.Direction) + 1.0f) < 1e-5)
		{
			up = Vector3(1.0f, 0.0f, 0.0f);
			m_LightViewCamera.SetUpDir(up);
		}

		// 그림자 맵 생성시 필요.
		Matrix lightView = DirectX::XMMatrixLookAtLH(Property.Position, Property.Position + Property.Direction, up); // 카메라를 이용하면 pitch, yaw를 고려하게됨. 이를 방지하기 위함.
		Matrix lightProjection = m_LightViewCamera.GetProjection();
		ShadowMap.Update(pManager, Property, m_LightViewCamera, mainCamera);

		ConstantBuffer* pShadowGlobalConstants = ShadowMap.GetShadowConstantsBufferPtr();
		switch (Property.LightType & m_TOTAL_LIGHT_TYPE)
		{
			case LIGHT_DIRECTIONAL:
				for (int i = 0; i < 4; ++i)
				{
					GlobalConstant* pShadowGlobalConstantData = (GlobalConstant*)(pShadowGlobalConstants[i].pData);
					Property.ViewProjections[i] = pShadowGlobalConstantData->ViewProjection;
					Property.Projections[i] = pShadowGlobalConstantData->Projection;
					Property.InverseProjections[i] = pShadowGlobalConstantData->InverseProjection;
				}
				break;

			case LIGHT_POINT:
				for (int i = 0; i < 6; ++i)
				{
					GlobalConstant* pShadowGlobalConstantData = (GlobalConstant*)(pShadowGlobalConstants[i].pData);
					Property.ViewProjections[i] = (pShadowGlobalConstantData->View.Transpose() * lightProjection).Transpose();
				}
				Property.Projections[0] = lightProjection.Transpose();
				Property.InverseProjections[0] = lightProjection.Invert().Transpose();
				break;

			case LIGHT_SPOT:
				Property.ViewProjections[0] = (lightView * lightProjection).Transpose();
				Property.Projections[0] = lightProjection.Transpose();
				Property.InverseProjections[0] = lightProjection.Invert().Transpose();
				break;

			default:
				break;
		}
	}
}

void Light::RenderShadowMap(ResourceManager* pManager, std::vector<Model*>& renderObjects, SkinnedMeshModel* pCharacter, Model* pMirror)
{
	if (Property.LightType & LIGHT_SHADOW)
	{
		ShadowMap.Render(pManager, renderObjects, pCharacter, pMirror);
	}
}

void Light::RenderShadowMap(ResourceManager* pManager, ID3D12GraphicsCommandList* pCommandList, std::vector<Model*>& renderObjects, SkinnedMeshModel* pCharacter, Model* pMirror)
{
	if (Property.LightType & LIGHT_SHADOW)
	{
		ShadowMap.Render(pManager, pCommandList, renderObjects, pCharacter, pMirror);
	}
}

void Light::Clear()
{
	ShadowMap.Clear();
	bRotated = false;
	bVisible = true;
}
