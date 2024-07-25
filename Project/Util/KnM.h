#pragma once

struct Keyboard
{
	bool bPressed[256] = { false, };
};
struct Mouse
{
	float MouseNDCX = 0.0f;
	float MouseNDCY = 0.0f;
	float WheelDelta = 0.0f;
	int MouseX = -1;
	int MouseY = -1;
	bool bMouseLeftButton = false;
	bool bMouseRightButton = false;
	bool bMouseDragStartFlag = false;
};
