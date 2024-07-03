#include "../pch.h"
#include "Camera.h"

void Camera::UpdateViewDir()
{
	// 이동할 때 기준이 되는 정면/오른쪽 방향 계산.
	m_ViewDirection = Vector3::Transform(Vector3(0.0f, 0.0f, 1.0f), Matrix::CreateRotationY(m_Yaw));
	m_RightDirection = m_UpDirection.Cross(m_ViewDirection);
}

void Camera::UpdateKeyboard(const float DELTA_TIME, const Keyboard* const pKeyboard)
{
	if (bUseFirstPersonView)
	{
		if (pKeyboard->Pressed['W'])
		{
			MoveForward(DELTA_TIME);
		}
		if (pKeyboard->Pressed['S'])
		{
			MoveForward(-DELTA_TIME);
		}
		if (pKeyboard->Pressed['D'])
		{
			MoveRight(DELTA_TIME);
		}
		if (pKeyboard->Pressed['A'])
		{
			MoveRight(-DELTA_TIME);
		}
		if (pKeyboard->Pressed['E'])
		{
			MoveUp(DELTA_TIME);
		}
		if (pKeyboard->Pressed['Q'])
		{
			MoveUp(-DELTA_TIME);
		}
	}
}

void Camera::UpdateMouse(float mouseNDCX, float mouseNDCY)
{
	if (bUseFirstPersonView)
	{
		// 얼마나 회전할지 계산.
		m_Yaw = mouseNDCX * DirectX::XM_2PI;       // 좌우 360도.
		m_Pitch = -mouseNDCY * DirectX::XM_PIDIV2; // 위 아래 90도.
		UpdateViewDir();
	}
}

void Camera::MoveForward(float deltaTime)
{
	// 이동후의_위치 = 현재_위치 + 이동방향 * 속도 * 시간차이.
	m_Position += m_ViewDirection * m_Speed * deltaTime;
}

void Camera::MoveRight(float deltaTime)
{
	// 이동후의_위치 = 현재_위치 + 이동방향 * 속도 * 시간차이.
	m_Position += m_RightDirection * m_Speed * deltaTime;
}

void Camera::MoveUp(float deltaTime)
{
	// 이동후의_위치 = 현재_위치 + 이동방향 * 속도 * 시간차이.
	m_Position += m_UpDirection * m_Speed * deltaTime;
}
