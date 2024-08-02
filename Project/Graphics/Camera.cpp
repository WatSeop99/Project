#include "../pch.h"
#include "Camera.h"

void Camera::UpdateViewDir()
{
	// 이동할 때 기준이 되는 정면/오른쪽 방향 계산.
	m_ViewDirection = Vector3::Transform(Vector3(0.0f, 0.0f, 1.0f), Matrix::CreateRotationY(m_Yaw));
	m_RightDirection = m_UpDirection.Cross(m_ViewDirection);
}

void Camera::UpdateKeyboard(const float DELTA_TIME, Keyboard* pKeyboard)
{
	if (bUseFirstPersonView)
	{
		if (pKeyboard->bPressed['W'])
		{
			MoveForward(DELTA_TIME);
		}
		if (pKeyboard->bPressed['S'])
		{
			MoveForward(-DELTA_TIME);
		}
		if (pKeyboard->bPressed['D'])
		{
			MoveRight(DELTA_TIME);
		}
		if (pKeyboard->bPressed['A'])
		{
			MoveRight(-DELTA_TIME);
		}
		if (pKeyboard->bPressed['E'])
		{
			MoveUp(DELTA_TIME);
		}
		if (pKeyboard->bPressed['Q'])
		{
			MoveUp(-DELTA_TIME);
		}
	}
}

void Camera::UpdateMouse(const float MOUSE_NDC_X, const float MOUSE_NDC_Y)
{
	if (bUseFirstPersonView)
	{
		// 얼마나 회전할지 계산.
		m_Yaw = MOUSE_NDC_X * DirectX::XM_2PI;       // 좌우 360도.
		m_Pitch = -MOUSE_NDC_Y * DirectX::XM_PIDIV2; // 위 아래 90도.
		UpdateViewDir();
	}
}

void Camera::MoveForward(const float DELTA_TIME)
{
	// 이동후의_위치 = 현재_위치 + 이동방향 * 속도 * 시간차이.
	m_Position += m_ViewDirection * m_Speed * DELTA_TIME;
}

void Camera::MoveRight(const float DELTA_TIME)
{
	// 이동후의_위치 = 현재_위치 + 이동방향 * 속도 * 시간차이.
	m_Position += m_RightDirection * m_Speed * DELTA_TIME;
}

void Camera::MoveUp(const float DELTA_TIME)
{
	// 이동후의_위치 = 현재_위치 + 이동방향 * 속도 * 시간차이.
	m_Position += m_UpDirection * m_Speed * DELTA_TIME;
}

void Camera::Reset(const Vector3& POS, const float YAW, const float PITCH)
{
	m_Position = POS;
	m_Yaw = YAW;
	m_Pitch = PITCH;
	UpdateViewDir();
}

Matrix Camera::GetView()
{
	return (Matrix::CreateTranslation(-m_Position) * Matrix::CreateRotationY(-m_Yaw) * Matrix::CreateRotationX(-m_Pitch)); // m_Pitch가 양수이면 고개를 드는 방향.
}

Matrix Camera::GetProjection()
{
	return (m_bUsePerspectiveProjection ?
			DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(m_ProjectionFovAngleY), m_Aspect, m_NearZ, m_FarZ) :
			DirectX::XMMatrixOrthographicOffCenterLH(-m_Aspect, m_Aspect, -1.0f, 1.0f, m_NearZ, m_FarZ));
}