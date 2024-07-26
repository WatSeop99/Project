#pragma once

#include <PxPhysicsAPI.h>

#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#define _CRTDBG_MAP_ALLOC_NEW
#include <crtdbg.h>
#include <dxgidebug.h>

#define DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW
#endif

#include "targetver.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

// required .lib files
#pragma comment(lib, "DXGI.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "D3DCompiler.lib")
// #pragma comment(lib, "d2d1.lib")
// #pragma comment(lib, "dwrite.lib")

#include <Windows.h>
#include <initguid.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <dwrite.h>
#include <d3dx12/d3dx12.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
// #include <pix3.h>

#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <wchar.h>
#include <stdint.h>
#include <process.h>
#include <synchapi.h>

// #define USE_MULTI_THREAD

#include "Graphics/EnumType.h"
#include "Renderer/Renderer.h"

#define BREAK_IF_FAILED(hr) \
		if (FAILED(hr))		\
		{					\
			__debugbreak(); \
		}
#define SAFE_RELEASE(p)		\
		if (p)				\
		{					\
			(p)->Release(); \
			(p) = nullptr;	\
		}
