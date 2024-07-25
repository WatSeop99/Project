#include "../pch.h"
#include "Renderer.h"
#include "RenderThread.h"

class Renderer;

UINT WINAPI RenderThread(void* pArg)
{
	RenderThreadDesc* pDesc = (RenderThreadDesc*)pArg;
	Renderer* pRenderer = pDesc->pRenderer;
	ResourceManager* pManager = pDesc->pResourceManager;
	UINT threadIndex = pDesc->ThreadIndex;
	const HANDLE* phEventList = pDesc->hEventList;

	while (true)
	{
		int eventIndex = WaitForMultipleObjects(RenderThreadEventType_Count, phEventList, FALSE, INFINITE);

		switch (eventIndex)
		{
			case RenderThreadEventType_Shadow:
			case RenderThreadEventType_Object:
			case RenderThreadEventType_Mirror:
			case RenderThreadEventType_Collider:
			case RenderThreadEventType_MainRender:
				pRenderer->ProcessByThread(threadIndex, pManager, eventIndex);
				break;

			case RenderThreadEventType_Desctroy:
				goto LB_EXIT;

			default:
				__debugbreak();
				break;
		}
	}

LB_EXIT:
	_endthreadex(0);
	return 0;
}
