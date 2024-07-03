#pragma once

struct Keyboard
{
	bool Pressed[256];
};

struct Mouse
{
	bool m_bMouseLeftButton;
	bool m_bMouseRightButton;
	bool m_bMouseDragStartFlag;
};