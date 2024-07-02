#include "pch.h"
#include "Resource.h"
#include "framework.h"
#include "Renderer\Renderer.h"

#ifdef _DEBUG
#include <crtdbg.h>
void CheckD3DMemoryLeak();
#endif

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	Renderer* pRenderer = new Renderer;

	pRenderer->Initizlie();
	pRenderer->Run();

	if (pRenderer)
	{
		delete pRenderer;
		pRenderer = nullptr;
	}

#ifdef _DEBUG
	CheckD3DMemoryLeak();
	_ASSERT(_CrtCheckMemory());
#endif

	return 0;
}

#ifdef _DEBUG
void CheckD3DMemoryLeak()
{
	HMODULE dxgiDebugDLL = GetModuleHandleW(L"dxgidebug.dll");
	decltype(&DXGIGetDebugInterface) GetDebugInterface = (decltype(&DXGIGetDebugInterface))(GetProcAddress(dxgiDebugDLL, "DXGIGetDebugInterface"));

	IDXGIDebug* pDebug = nullptr;

	GetDebugInterface(IID_PPV_ARGS(&pDebug));

	if (pDebug)
	{
		OutputDebugStringW(L"================================== Direct3D Object Memory Leak List ==================================\n");
		pDebug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_DETAIL);
		OutputDebugStringW(L"===========================================================================================\n");

		pDebug->Release();
	}
}
#endif
