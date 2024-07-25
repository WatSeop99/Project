#pragma once

#include "../Graphics/EnumType.h"

struct RenderThreadDesc
{
	Renderer* pRenderer;
	ResourceManager* pResourceManager;
	UINT ThreadIndex;
	HANDLE hThread;
	HANDLE hEventList[RenderThreadEventType_Count];
};

UINT WINAPI RenderThread(void* pArg);
