#include "../pch.h"
#include "../Model/GeometryGenerator.h"
#include "../Util/Utility.h"
#include "Renderer.h"

Renderer* g_pRendrer = nullptr;

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return g_pRendrer->MsgProc(hWnd, msg, wParam, lParam);
}

Renderer::Renderer()
{
	g_pRendrer = this;
	m_Camera.SetAspectRatio((float)m_ScreenWidth / (float)m_ScreenHeight);
}

Renderer::~Renderer()
{
	g_pRendrer = nullptr;
	Cleanup();
}

void Renderer::Initizlie()
{
	initMainWidndow();
	initDirect3D();
	initPhysics();
	initScene();
	initRenderTargets();
	initDepthStencils();
	initShaderResources();

	D3D12_CPU_DESCRIPTOR_HANDLE nullSrv = {};
	if (m_pSRVUAVAllocator->AllocDescriptorHandle(&nullSrv) == -1)
	{
		__debugbreak();
	}
	
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	m_pDevice->CreateShaderResourceView(nullptr, &srvDesc, nullSrv);
	m_pResourceManager->NullSRVDescriptor = nullSrv;


	PostProcessor::PostProcessingBuffers config =
	{
		{ m_pRenderTargets[0], m_pRenderTargets[1] },
		m_pFloatBuffer,
		m_pPrevBuffer,
		&m_GlobalConstantData,
		m_MainRenderTargetOffset, m_MainRenderTargetOffset + 1, m_FloatBufferSRVOffset, m_PrevBufferSRVOffset
	};
	m_pPostProcessor = new PostProcessor;
	m_pPostProcessor->Initizlie(this, config, m_ScreenWidth, m_ScreenHeight, 2);

	m_ScreenViewport.TopLeftX = 0;
	m_ScreenViewport.TopLeftY = 0;
	m_ScreenViewport.Width = (float)m_ScreenWidth;
	m_ScreenViewport.Height = (float)m_ScreenHeight;
	m_ScreenViewport.MinDepth = 0.0f;
	m_ScreenViewport.MaxDepth = 1.0f;

	m_ScissorRect.left = 0;
	m_ScissorRect.top = 0;
	m_ScissorRect.right = m_ScreenWidth;
	m_ScissorRect.bottom = m_ScreenHeight;
}

void Renderer::Update(const float DELTA_TIME)
{
	m_Camera.UpdateKeyboard(DELTA_TIME, &m_Keyboard);
	processMouseControl(DELTA_TIME);

	updateGlobalConstants(DELTA_TIME);
	updateLightConstants(DELTA_TIME);
}

void Renderer::Render()
{
	beginRender();

	renderShadowmap();
	renderObject();
	renderMirror();
	renderObjectBoundingModel();
	postProcess();

	endRender();

	present();
}

void Renderer::ProcessByThread(UINT threadIndex, ResourceManager* pManager, int renderPass)
{
	_ASSERT(threadIndex >= 0 && threadIndex < m_RenderThreadCount);
	_ASSERT(pManager);

	ID3D12CommandQueue* pCommandQueue = m_pCommandQueue;
	CommandListPool* pCommandListPool = m_pppCommandListPool[m_FrameIndex][threadIndex];
	DynamicDescriptorPool* pDescriptorPool = m_pppDescriptorPool[m_FrameIndex][threadIndex];
	ConstantBufferManager* pConstantBufferManager = m_pppConstantBufferManager[m_FrameIndex][threadIndex];

	switch (renderPass)
	{
		case RenderPass_Shadow:
			m_pppRenderQueue[renderPass][threadIndex]->ProcessLight(threadIndex, pCommandQueue, pCommandListPool, pManager, pDescriptorPool, pConstantBufferManager, 100);
			break;

		case RenderPass_Object:
		case RenderPass_Mirror:
		case RenderPass_Collider:
		{
			ID3D12GraphicsCommandList* pCommandList = pCommandListPool->GetCurrentCommandList();
			CD3DX12_CPU_DESCRIPTOR_HANDLE floatBufferRtvHandle(m_pRTVAllocator->GetDescriptorHeap()->GetCPUDescriptorHandleForHeapStart(), m_FloatBufferRTVOffset, m_pResourceManager->RTVDescriptorSize);
			CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_pDSVAllocator->GetDescriptorHeap()->GetCPUDescriptorHandleForHeapStart());

			pCommandList->RSSetViewports(1, &m_ScreenViewport);
			pCommandList->RSSetScissorRects(1, &m_ScissorRect);
			pCommandList->OMSetRenderTargets(1, &floatBufferRtvHandle, FALSE, &dsvHandle);
			m_pppRenderQueue[renderPass][threadIndex]->Process(threadIndex, pCommandQueue, pCommandListPool, pManager, pDescriptorPool, pConstantBufferManager, 100);
		}
		break;

		default:
			__debugbreak();
			break;
	}

	long curActiveThreadCount = _InterlockedDecrement(&m_pActiveThreadCounts[renderPass]);
	if (curActiveThreadCount == 0)
	{
		SetEvent(m_phCompletedEvents[renderPass]);
	}
}

UINT64 Renderer::Fence()
{
	++m_FenceValue;
	m_pCommandQueue->Signal(m_pFence, m_FenceValue);
	m_LastFenceValues[m_FrameIndex] = m_FenceValue;
	return m_FenceValue;
}

void Renderer::WaitForFenceValue(UINT64 expectedFenceValue)
{
	// Wait until the previous frame is finished.
	if (m_pFence->GetCompletedValue() < expectedFenceValue)
	{
		m_pFence->SetEventOnCompletion(expectedFenceValue, m_hFenceEvent);
		WaitForSingleObject(m_hFenceEvent, INFINITE);
	}
}

void Renderer::Cleanup()
{
	Fence();
	for (UINT i = 0; i < SWAP_CHAIN_FRAME_COUNT; ++i)
	{
		WaitForFenceValue(m_LastFenceValues[i]);
	}

#ifdef USE_MULTI_THREAD

	if (m_pThreadDescList)
	{
		for (UINT i = 0; i < m_RenderThreadCount; ++i)
		{
			SetEvent(m_pThreadDescList[i].hEventList[RenderThreadEventType_Desctroy]);

			WaitForSingleObject(m_pThreadDescList[i].hThread, INFINITE);
			CloseHandle(m_pThreadDescList[i].hThread);
			m_pThreadDescList[i].hThread = nullptr;

			for (UINT j = 0; j < RenderThreadEventType_Count; ++j)
			{
				CloseHandle(m_pThreadDescList[i].hEventList[j]);
				m_pThreadDescList[i].hEventList[j] = nullptr;
			}
		}

		delete[] m_pThreadDescList;
		m_pThreadDescList = nullptr;
	}
	for (UINT i = 0; i < RenderPass_RenderPassCount; ++i)
	{
		CloseHandle(m_phCompletedEvents[i]);
		m_phCompletedEvents[i] = nullptr;
	}

#endif

	if (m_pPhysicsManager)
	{
		delete m_pPhysicsManager;
		m_pPhysicsManager = nullptr;
	}
	if (m_pPostProcessor)
	{
		delete m_pPostProcessor;
		m_pPostProcessor = nullptr;
	}

	cleanShaderResources();
	cleanDepthStencils();
	cleanRenderTargets();

	CD3DX12_CPU_DESCRIPTOR_HANDLE nullSRV(m_pResourceManager->NullSRVDescriptor);
	m_pSRVUAVAllocator->FreeDescriptorHandle(nullSRV);

	m_pRenderObjects = nullptr;
	m_pLights = nullptr;
	m_pLightSpheres = nullptr;
	m_pMirror = nullptr;
	m_pPickedModel = nullptr;
	m_pPickedEndEffector = nullptr;
	m_pMirrorPlane = nullptr;

	if (m_hFenceEvent)
	{
		CloseHandle(m_hFenceEvent);
		m_hFenceEvent = nullptr;
	}
	m_FenceValue = 0;
	SAFE_RELEASE(m_pFence);

	if (m_pDSVAllocator)
	{
		delete m_pDSVAllocator;
		m_pDSVAllocator = nullptr;
	}
	if (m_pSRVUAVAllocator)
	{
		delete m_pSRVUAVAllocator;
		m_pSRVUAVAllocator = nullptr;
	}
	if (m_pRTVAllocator)
	{
		delete m_pRTVAllocator;
		m_pRTVAllocator = nullptr;
	}

	if (m_pResourceManager)
	{
		delete m_pResourceManager;
		m_pResourceManager = nullptr;
	}

	for (UINT i = 0; i < SWAP_CHAIN_FRAME_COUNT; ++i)
	{
		for (UINT j = 0; j < MAX_RENDER_THREAD_COUNT; ++j)
		{
			if (m_pppCommandListPool[i][j])
			{
				delete m_pppCommandListPool[i][j];
				m_pppCommandListPool[i][j] = nullptr;
			}
			if (m_pppDescriptorPool[i][j])
			{
				delete m_pppDescriptorPool[i][j];
				m_pppDescriptorPool[i][j] = nullptr;
			}
			if (m_pppConstantBufferManager[i][j])
			{
				delete m_pppConstantBufferManager[i][j];
				m_pppConstantBufferManager[i][j] = nullptr;
			}
		}
	}
	for (int i = 0; i < RenderPass_RenderPassCount; ++i)
	{
		for (UINT j = 0; j < MAX_RENDER_THREAD_COUNT; ++j)
		{
			if (m_pppRenderQueue[i][j])
			{
				delete m_pppRenderQueue[i][j];
				m_pppRenderQueue[i][j] = nullptr;
			}
		}
	}

	if (m_pTextureManager)
	{
		delete m_pTextureManager;
		m_pTextureManager = nullptr;
	}


	SAFE_RELEASE(m_pSwapChain);
	SAFE_RELEASE(m_pCommandQueue);

	if (m_pDevice)
	{
		ULONG refCount = m_pDevice->Release();

#ifdef _DEBUG
		// debug layer release.
		if (refCount)
		{
			IDXGIDebug1* pDebug = nullptr;
			if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug))))
			{
				pDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_SUMMARY);
				pDebug->Release();
			}
			__debugbreak();
		}
#endif

		m_pDevice = nullptr;
	}
}

LRESULT Renderer::MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		case WM_SIZE:
		{
			if (!m_pSwapChain)
			{
				break;
			}
			
			// 화면 해상도가 바뀌면 SwapChain을 다시 생성.
			Fence();
			for (int i = 0; i < SWAP_CHAIN_FRAME_COUNT; ++i)
			{
				WaitForFenceValue(m_LastFenceValues[i]);
			}

			m_ScreenWidth = (UINT)LOWORD(lParam);
			m_ScreenHeight = (UINT)HIWORD(lParam);

			m_ScreenViewport.Width = (float)m_ScreenWidth;
			m_ScreenViewport.Height = (float)m_ScreenHeight;
			m_ScissorRect.right = m_ScreenWidth;
			m_ScissorRect.bottom = m_ScreenHeight;

			// 윈도우가 Minimize 모드에서는 screenWidth/Height가 0.
			if(m_ScreenWidth == 0 && m_ScreenHeight == 0)
			{
				break;
			}

#ifdef _DEBUG
			char szDebugString[256] = { 0, };
			sprintf_s(szDebugString, 256, "Resize SwapChain to %d %d\n", m_ScreenWidth, m_ScreenHeight);
			OutputDebugStringA(szDebugString);
#endif 

			// 기존 버퍼 초기화.
			cleanShaderResources();
			cleanDepthStencils();
			cleanRenderTargets();

			m_pPostProcessor->Cleanup();
			m_FrameIndex = m_pSwapChain->GetCurrentBackBufferIndex();

			// swap chain resize.
			m_pSwapChain->ResizeBuffers(0,					 // 현재 개수 유지.
										m_ScreenWidth,		 // 해상도 변경.
										m_ScreenHeight,
										DXGI_FORMAT_UNKNOWN, // 현재 포맷 유지.
										0);
			// 버퍼 재생성.
			initRenderTargets();
			initDepthStencils();
			initShaderResources();

			PostProcessor::PostProcessingBuffers config =
			{
				{ m_pRenderTargets[0], m_pRenderTargets[1] },
				m_pFloatBuffer,
				m_pPrevBuffer,
				&m_GlobalConstantData,
				m_MainRenderTargetOffset, m_MainRenderTargetOffset + 1, m_FloatBufferSRVOffset, m_PrevBufferSRVOffset
			};
			m_pPostProcessor->Initizlie(this, config, m_ScreenWidth, m_ScreenHeight, 4);
			m_Camera.SetAspectRatio((float)m_ScreenWidth / (float)m_ScreenHeight);
		}
		break;

		case WM_SYSCOMMAND:
			if ((wParam & 0xfff0) == SC_KEYMENU) // ALT키 비활성화.
			{
				return 0;
			}
			break;

		case WM_MOUSEMOVE:
			onMouseMove(LOWORD(lParam), HIWORD(lParam));
			break;

		case WM_LBUTTONDOWN:
			if (!m_Mouse.bMouseLeftButton)
			{
				m_Mouse.bMouseDragStartFlag = true; // 드래그를 새로 시작하는지 확인.
			}
			m_Mouse.bMouseLeftButton = true;
			onMouseClick(LOWORD(lParam), HIWORD(lParam));
			break;

		case WM_LBUTTONUP:
			m_Mouse.bMouseLeftButton = false;
			break;

		case WM_RBUTTONDOWN:
			if (!m_Mouse.bMouseRightButton)
			{
				m_Mouse.bMouseDragStartFlag = true; // 드래그를 새로 시작하는지 확인.
			}
			m_Mouse.bMouseRightButton = true;
			break;

		case WM_RBUTTONUP:
			m_Mouse.bMouseRightButton = false;
			break;

		case WM_KEYDOWN:
			m_Keyboard.bPressed[wParam] = true;
			if (wParam == VK_ESCAPE) // ESC키 종료.
			{
				DestroyWindow(m_hMainWindow);
			}
			if (wParam == VK_SPACE)
			{
				(*m_pLights)[1].bRotated = !(*m_pLights)[1].bRotated;
			}
			break;

		case WM_KEYUP:
			if (wParam == 'F')  // f키 일인칭 시점.
			{
				m_Camera.bUseFirstPersonView = !m_Camera.bUseFirstPersonView;
			}

			m_Keyboard.bPressed[wParam] = false;
			break;

		case WM_MOUSEWHEEL:
			m_Mouse.WheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
			break;

		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;

		default:
			break;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}

ConstantBufferManager* Renderer::GetConstantBufferPool(UINT threadIndex)
{
	_ASSERT(threadIndex >= 0 && threadIndex < m_RenderThreadCount);
	return m_pppConstantBufferManager[m_FrameIndex][threadIndex];
}

ConstantBufferManager* Renderer::GetConstantBufferManager(UINT threadIndex)
{
	_ASSERT(threadIndex >= 0 && threadIndex < m_RenderThreadCount);
	return m_pppConstantBufferManager[m_FrameIndex][threadIndex];
}

DynamicDescriptorPool* Renderer::GetDynamicDescriptorPool(UINT threadIndex)
{
	_ASSERT(threadIndex >= 0 && threadIndex < m_RenderThreadCount);
	return m_pppDescriptorPool[m_FrameIndex][threadIndex];
}

void Renderer::SetExternalDatas(InitialData* pInitialData)
{
	_ASSERT(pInitialData);

	m_pRenderObjects = pInitialData->pRenderObjects;
	m_pLights = pInitialData->pLights;
	m_pLightSpheres = pInitialData->pLightSpheres;

	m_pMirror = pInitialData->pMirror;
	m_pMirrorPlane = pInitialData->pMirrorPlane;

	m_pEnvTextureHandle = pInitialData->pEnvTextureHandle;
	m_pIrradianceTextureHandle = pInitialData->pIrradianceTextureHandle;
	m_pSpecularTextureHandle = pInitialData->pSpecularTextureHandle;
	m_pBRDFTextureHandle = pInitialData->pBRDFTextureHandle;
}

void Renderer::initMainWidndow()
{
	WNDCLASSEX wc =
	{
		sizeof(WNDCLASSEX),			// cbSize
		CS_HREDRAW | CS_VREDRAW,	// style
		WndProc,					// lpfnWndProc
		0,							// cbClsExtra
		0,							// cbWndExtra
		GetModuleHandle(NULL),		// hInstance
		NULL, 						// hIcon
		NULL,						// hCursor
		(HBRUSH)(COLOR_WINDOW + 1),	// hbrBackground
		nullptr,					// lpszMenuName
		L"OWL_",					// lpszClassName
		NULL						// hIconSm
	};

	if (!RegisterClassEx(&wc))
	{
		__debugbreak();
	}

	RECT wr = { 0, 0, (long)m_ScreenWidth, (long)m_ScreenHeight };
	BOOL bResult = AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
	m_hMainWindow = CreateWindow(wc.lpszClassName,
								 L"DX12",
								 WS_OVERLAPPEDWINDOW,
								 100,							// 윈도우 좌측 상단의 x 좌표
								 100,							// 윈도우 좌측 상단의 y 좌표
								 wr.right - wr.left,			// 윈도우 가로 방향 해상도
								 wr.bottom - wr.top,			// 윈도우 세로 방향 해상도
								 NULL, NULL, wc.hInstance, NULL);

	if (!m_hMainWindow)
	{
		__debugbreak();
	}

	ShowWindow(m_hMainWindow, SW_SHOWDEFAULT);
	UpdateWindow(m_hMainWindow);
}

void Renderer::initDirect3D()
{
	HRESULT hr = S_OK;

	ID3D12Debug* pDebugController = nullptr;
	UINT createDeviceFlags = 0;
	UINT createFactoryFlags = 0;

#ifdef _DEBUG
	ID3D12Debug6* pDebugController6 = nullptr;

	hr = D3D12GetDebugInterface(IID_PPV_ARGS(&pDebugController));
	if (SUCCEEDED(hr))
	{
		pDebugController->EnableDebugLayer();

		createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;

		hr = pDebugController->QueryInterface(IID_PPV_ARGS(&pDebugController6));
		if (SUCCEEDED(hr))
		{
			pDebugController6->SetEnableGPUBasedValidation(TRUE);
			pDebugController6->SetEnableAutoName(TRUE);
			SAFE_RELEASE(pDebugController6);
		}
	}
#endif

	const D3D_FEATURE_LEVEL FEATURE_LEVELS[5] =
	{
		D3D_FEATURE_LEVEL_12_2,
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};
	IDXGIFactory5* pFactory5 = nullptr;
	IDXGIAdapter3* pAdapter3 = nullptr;

	hr = CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&pFactory5));
	BREAK_IF_FAILED(hr);

	for (UINT featureLevelIndex = 0; featureLevelIndex < 5; ++featureLevelIndex)
	{
		UINT adapterIndex = 0;
		while (pFactory5->EnumAdapters1(adapterIndex, (IDXGIAdapter1**)(&pAdapter3)) != DXGI_ERROR_NOT_FOUND)
		{
			pAdapter3->GetDesc2(&m_AdapterDesc);
			hr = D3D12CreateDevice(pAdapter3, FEATURE_LEVELS[featureLevelIndex], IID_PPV_ARGS(&m_pDevice));
			if (SUCCEEDED(hr))
			{
				goto LB_EXIT;
			}

			SAFE_RELEASE(pAdapter3);
			++adapterIndex;
		}
	}
LB_EXIT:
	BREAK_IF_FAILED(hr);
	m_pDevice->SetName(L"D3DDevice");

	if (pDebugController)
	{
		SetDebugLayerInfo(m_pDevice);
	}

	UINT physicalCoreCount = 0;
	UINT logicalCoreCount = 0;
	GetPhysicalCoreCount(&physicalCoreCount, &logicalCoreCount);
	m_RenderThreadCount = physicalCoreCount;
	if (m_RenderThreadCount > MAX_RENDER_THREAD_COUNT)
	{
		m_RenderThreadCount = MAX_RENDER_THREAD_COUNT;
	}

	// create command queue
	{
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

		hr = m_pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_pCommandQueue));
		BREAK_IF_FAILED(hr);
		m_pCommandQueue->SetName(L"CommandQueue");
	}

	// create swapchain
	{
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
		swapChainDesc.Width = m_ScreenWidth;
		swapChainDesc.Height = m_ScreenHeight;
		// swapChainDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
		swapChainDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = SWAP_CHAIN_FRAME_COUNT;
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.Scaling = DXGI_SCALING_NONE;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
		swapChainDesc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsSwapChainDesc = {};
		/*fsSwapChainDesc.RefreshRate.Numerator = 60;
		fsSwapChainDesc.RefreshRate.Denominator = 1;*/
		fsSwapChainDesc.Windowed = TRUE;

		IDXGISwapChain1* pSwapChain1 = nullptr;
		hr = pFactory5->CreateSwapChainForHwnd(m_pCommandQueue, m_hMainWindow, &swapChainDesc, &fsSwapChainDesc, nullptr, &pSwapChain1);
		BREAK_IF_FAILED(hr);
		pSwapChain1->QueryInterface(IID_PPV_ARGS(&m_pSwapChain));
		SAFE_RELEASE(pSwapChain1);

		m_BackBufferFormat = swapChainDesc.Format;

		m_FrameIndex = m_pSwapChain->GetCurrentBackBufferIndex();
	}

	// create render queue and command list pool, descriptor pool
	{
		for (int i = 0; i < RenderPass_RenderPassCount; ++i)
		{
			for (UINT j = 0; j < MAX_RENDER_THREAD_COUNT; ++j)
			{
				m_pppRenderQueue[i][j] = new RenderQueue;
				m_pppRenderQueue[i][j]->Initialize(8192);
			}
		}

		for (UINT i = 0; i < SWAP_CHAIN_FRAME_COUNT; i++)
		{
			for (UINT j = 0; j < m_RenderThreadCount; j++)
			{
				m_pppCommandListPool[i][j] = new CommandListPool;
				m_pppCommandListPool[i][j]->Initialize(m_pDevice, D3D12_COMMAND_LIST_TYPE_DIRECT, 256);

				m_pppDescriptorPool[i][j] = new DynamicDescriptorPool;
				m_pppDescriptorPool[i][j]->Initialize(m_pDevice, 4096);

				m_pppConstantBufferManager[i][j] = new ConstantBufferManager;
				m_pppConstantBufferManager[i][j]->Initialize(m_pDevice, 4096);
			}
		}
	}

	// create fence
	{
		hr = m_pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_pFence));
		BREAK_IF_FAILED(hr);

		m_FenceValue = 0;
		m_hFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	}

	m_pResourceManager = new ResourceManager;
	m_pResourceManager->Initialize(this);

	m_pTextureManager = new TextureManager;
	m_pTextureManager->Initialize(this, 1024 / 16, 1024);

	m_pRTVAllocator = new DescriptorAllocator;
	m_pRTVAllocator->Initialize(m_pDevice, 8, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	m_pDSVAllocator = new DescriptorAllocator;
	m_pDSVAllocator->Initialize(m_pDevice, 16, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	m_pSRVUAVAllocator = new DescriptorAllocator;
	m_pSRVUAVAllocator->Initialize(m_pDevice, 4096, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

#ifdef USE_MULTI_THREAD
	// create thread and event
	{
		initRenderThreadPool(m_RenderThreadCount);
		for (int i = 0; i < RenderPass_RenderPassCount; ++i)
		{
			m_phCompletedEvents[i] = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		}
	}
#endif

	SAFE_RELEASE(pDebugController);
	SAFE_RELEASE(pFactory5);
	SAFE_RELEASE(pAdapter3);
}

void Renderer::initPhysics()
{
	m_pPhysicsManager = new PhysicsManager;
	m_pPhysicsManager->Initialize(m_RenderThreadCount);
}

void Renderer::initScene()
{
	// m_Camera.Reset(Vector3(3.74966f, 5.03645f, -2.54918f), -0.819048f, 0.741502f);
	m_Camera.Reset(Vector3(0.0f, 3.0f, -2.0f), -0.819048f, 0.741502f);
	
	m_pResourceManager->SetGlobalConstants(&m_GlobalConstantData, &m_LightConstantData, &m_ReflectionGlobalConstantData);

	// 공용 global constant 설정.
	// m_GlobalConstantData.StrengthIBL = 0.3f;
	/*for (int i = 0; i < MAX_LIGHTS; ++i)
	{
		memcpy(&m_LightConstantData.Lights[i], &(*m_pLights)[i].Property, sizeof(LightProperty));
	}*/
}

void Renderer::initRenderThreadPool(UINT renderThreadCount)
{
	m_pThreadDescList = new RenderThreadDesc[renderThreadCount];
	ZeroMemory(m_pThreadDescList, sizeof(RenderThreadDesc) * renderThreadCount);

	for (UINT i = 0; i < renderThreadCount; ++i)
	{
		for (int j = 0; j < RenderThreadEventType_Count; ++j)
		{
			m_pThreadDescList[i].hEventList[j] = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		}

		m_pThreadDescList[i].pRenderer = this;
		m_pThreadDescList[i].pResourceManager = m_pResourceManager;
		m_pThreadDescList[i].ThreadIndex = i;
		UINT threadID = 0;
		m_pThreadDescList[i].hThread = (HANDLE)_beginthreadex(nullptr, 0, RenderThread, m_pThreadDescList + i, 0, &threadID);
	}
}

void Renderer::initRenderTargets()
{
	_ASSERT(m_pRTVAllocator);

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = {};
	UINT offset;

	// Create two main render target view.
	for(UINT i = 0; i < SWAP_CHAIN_FRAME_COUNT; ++i)
	{
		offset = m_pRTVAllocator->AllocDescriptorHandle(&rtvHandle);
		if (m_MainRenderTargetOffset == -1)
		{
			m_MainRenderTargetOffset = offset;
		}

		m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&m_pRenderTargets[i]));
		m_pRenderTargets[i]->SetName(L"RenderTarget");
		m_pDevice->CreateRenderTargetView(m_pRenderTargets[i], nullptr, rtvHandle);
	}


	// Create float buffer.
	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Alignment = 0;
	resourceDesc.Width = m_ScreenWidth;
	resourceDesc.Height = m_ScreenHeight;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

	HRESULT hr = m_pDevice->CreateCommittedResource(&heapProps,
													D3D12_HEAP_FLAG_NONE,
													&resourceDesc,
													D3D12_RESOURCE_STATE_COMMON,
													nullptr,
													IID_PPV_ARGS(&m_pFloatBuffer));
	BREAK_IF_FAILED(hr);
	m_pFloatBuffer->SetName(L"FloatBuffer");

	offset = m_pRTVAllocator->AllocDescriptorHandle(&rtvHandle);
	if (m_FloatBufferRTVOffset == -1)
	{
		m_FloatBufferRTVOffset = offset;
	}

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;
	m_pDevice->CreateRenderTargetView(m_pFloatBuffer, &rtvDesc, rtvHandle);
}

void Renderer::initDepthStencils()
{
	_ASSERT(m_pDSVAllocator);

	HRESULT hr = S_OK;
	
	// Create deafult depth stencil.
	D3D12_RESOURCE_DESC dsvDesc = {};
	dsvDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	dsvDesc.Alignment = 0;
	dsvDesc.Width = m_ScreenWidth;
	dsvDesc.Height = m_ScreenHeight;
	dsvDesc.DepthOrArraySize = 1;
	dsvDesc.MipLevels = 1;
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.SampleDesc.Count = 1;
	dsvDesc.SampleDesc.Quality = 0;
	dsvDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	dsvDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	clearValue.DepthStencil.Depth = 1.0f;
	clearValue.DepthStencil.Stencil = 0;

	hr = m_pDevice->CreateCommittedResource(&heapProps,
											D3D12_HEAP_FLAG_NONE,
											&dsvDesc,
											D3D12_RESOURCE_STATE_DEPTH_WRITE,
											&clearValue,
											IID_PPV_ARGS(&m_pDefaultDepthStencil));
	BREAK_IF_FAILED(hr);
	m_pDefaultDepthStencil->SetName(L"DefaultDepthStencil");


	// Create default depth stencil view.
	D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc = {};
	depthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthStencilViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	depthStencilViewDesc.Texture2D.MipSlice = 0;

	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = {};
	m_DefaultDepthStencilOffset = m_pDSVAllocator->AllocDescriptorHandle(&dsvHandle);
	m_pDevice->CreateDepthStencilView(m_pDefaultDepthStencil, &depthStencilViewDesc, dsvHandle);
}

void Renderer::initShaderResources()
{
	_ASSERT(m_pSRVUAVAllocator);
	_ASSERT(m_pFloatBuffer);

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Alignment = 0;
	resourceDesc.Width = m_ScreenWidth;
	resourceDesc.Height = m_ScreenHeight;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

	HRESULT hr = m_pDevice->CreateCommittedResource(&heapProps,
													D3D12_HEAP_FLAG_NONE,
													&resourceDesc,
													D3D12_RESOURCE_STATE_COMMON,
													nullptr,
													IID_PPV_ARGS(&m_pPrevBuffer));
	BREAK_IF_FAILED(hr);
	m_pPrevBuffer->SetName(L"PrevBuffer");


	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = {};
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	// Create float buffer srv.
	m_FloatBufferSRVOffset = m_pSRVUAVAllocator->AllocDescriptorHandle(&srvHandle);
	if(m_FloatBufferSRVOffset == -1)
	{
		__debugbreak();
	}
	m_pDevice->CreateShaderResourceView(m_pFloatBuffer, &srvDesc, srvHandle);

	// Create prev buffer srv.
	m_PrevBufferSRVOffset = m_pSRVUAVAllocator->AllocDescriptorHandle(&srvHandle);
	if(m_PrevBufferSRVOffset == -1)
	{
		__debugbreak();
	}
	m_pDevice->CreateShaderResourceView(m_pPrevBuffer, &srvDesc, srvHandle);
}

void Renderer::cleanRenderTargets()
{
	_ASSERT(m_pRTVAllocator);
	_ASSERT(m_pFloatBuffer);

	ID3D12DescriptorHeap* pRTVHeap = m_pRTVAllocator->GetDescriptorHeap();
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(pRTVHeap->GetCPUDescriptorHandleForHeapStart(), m_MainRenderTargetOffset, m_pResourceManager->RTVDescriptorSize);;
	for(UINT i = 0; i < SWAP_CHAIN_FRAME_COUNT; ++i)
	{
		m_pRTVAllocator->FreeDescriptorHandle(rtvHandle);
		SAFE_RELEASE(m_pRenderTargets[i]);
		rtvHandle.Offset(1, m_pResourceManager->RTVDescriptorSize);
	}
	m_MainRenderTargetOffset = 0xffffffff;

	rtvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(pRTVHeap->GetCPUDescriptorHandleForHeapStart(), m_FloatBufferRTVOffset, m_pResourceManager->RTVDescriptorSize);
	m_pRTVAllocator->FreeDescriptorHandle(rtvHandle);
	m_FloatBufferRTVOffset = 0xffffffff;
	SAFE_RELEASE(m_pFloatBuffer);
}

void Renderer::cleanDepthStencils()
{
	_ASSERT(m_pDSVAllocator);
	_ASSERT(m_pDefaultDepthStencil);

	ID3D12DescriptorHeap* pDSVHeap = m_pDSVAllocator->GetDescriptorHeap();
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(pDSVHeap->GetCPUDescriptorHandleForHeapStart(), m_DefaultDepthStencilOffset, m_pResourceManager->DSVDescriptorSize);

	m_pDSVAllocator->FreeDescriptorHandle(dsvHandle);
	m_DefaultDepthStencilOffset = 0xffffffff;
	SAFE_RELEASE(m_pDefaultDepthStencil);
}

void Renderer::cleanShaderResources()
{
	_ASSERT(m_pSRVUAVAllocator);
	_ASSERT(m_pFloatBuffer);
	_ASSERT(m_pPrevBuffer);

	ID3D12DescriptorHeap* pSRVUAVHeap = m_pSRVUAVAllocator->GetDescriptorHeap();
	CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(pSRVUAVHeap->GetCPUDescriptorHandleForHeapStart(), m_FloatBufferSRVOffset, m_pResourceManager->CBVSRVUAVDescriptorSize);

	m_pSRVUAVAllocator->FreeDescriptorHandle(srvHandle);
	m_FloatBufferSRVOffset = 0xffffffff;

	srvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(pSRVUAVHeap->GetCPUDescriptorHandleForHeapStart(), m_PrevBufferSRVOffset, m_pResourceManager->CBVSRVUAVDescriptorSize);
	m_pSRVUAVAllocator->FreeDescriptorHandle(srvHandle);
	m_PrevBufferSRVOffset = 0xffffffff;
	SAFE_RELEASE(m_pPrevBuffer);
}

void Renderer::beginRender()
{
	HRESULT hr = S_OK;

#ifdef USE_MULTI_THREAD

	CommandListPool* pCommandListPool = m_pppCommandListPool[m_FrameIndex][0];
	ID3D12GraphicsCommandList* pCommandList = pCommandListPool->GetCurrentCommandList();
	
	const CD3DX12_RESOURCE_BARRIER BARRIERs[2] =
	{
		CD3DX12_RESOURCE_BARRIER::Transition(m_pRenderTargets[m_FrameIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET),
		CD3DX12_RESOURCE_BARRIER::Transition(m_pFloatBuffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET)
	};
	pCommandList->ResourceBarrier(2, BARRIERs);

	const UINT RTV_DESCRIPTOR_SIZE = m_pResourceManager->m_RTVDescriptorSize;
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_pResourceManager->m_pRTVHeap->GetCPUDescriptorHandleForHeapStart(), m_MainRenderTargetOffset + m_FrameIndex, RTV_DESCRIPTOR_SIZE);
	CD3DX12_CPU_DESCRIPTOR_HANDLE floatRtvHandle(m_pResourceManager->m_pRTVHeap->GetCPUDescriptorHandleForHeapStart(), m_FloatBufferRTVOffset, RTV_DESCRIPTOR_SIZE);
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_pResourceManager->m_pDSVHeap->GetCPUDescriptorHandleForHeapStart());

	const float COLOR[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
	pCommandList->ClearRenderTargetView(rtvHandle, COLOR, 0, nullptr);
	pCommandList->ClearRenderTargetView(floatRtvHandle, COLOR, 0, nullptr);
	pCommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	pCommandListPool->ClosedAndExecute(m_pCommandQueue);

#else

    ID3D12GraphicsCommandList* pCommandList = GetCommandList();
    ID3D12DescriptorHeap* ppDescriptorHeaps[2] =
    {
        m_pppDescriptorPool[m_FrameIndex][0]->GetDescriptorHeap(),
        m_pResourceManager->m_pSamplerHeap
    };
    pCommandList->SetDescriptorHeaps(2, ppDescriptorHeaps);

#endif
}

void Renderer::renderShadowmap()
{
#ifdef USE_MULTI_THREAD

	CommandListPool* pCommandListPool = m_pppCommandListPool[m_FrameIndex][0];
	const int TOTAL_LIGHT_TYPE = LIGHT_DIRECTIONAL | LIGHT_POINT | LIGHT_SPOT;

	for (int i = 0; i < MAX_LIGHTS; ++i)
	{
		CD3DX12_RESOURCE_BARRIER barrier;
		ID3D12GraphicsCommandList* pCommandList = pCommandListPool->GetCurrentCommandList();

		Light* pCurLight = &(*m_pLights)[i];
		eRenderPSOType renderPSO;
		
		// set shadow map writing state for depth buffer.
		switch (pCurLight->Property.LightType & TOTAL_LIGHT_TYPE)
		{
			case LIGHT_DIRECTIONAL:
				barrier = CD3DX12_RESOURCE_BARRIER::Transition(pCurLight->LightShadowMap.GetDirectionalLightShadowBufferPtr()->GetResource(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);
				renderPSO = RenderPSOType_DepthOnlyCascadeDefault;
				break;

			case LIGHT_POINT:
				barrier = CD3DX12_RESOURCE_BARRIER::Transition(pCurLight->LightShadowMap.GetPointLightShadowBufferPtr()->GetResource(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);
				renderPSO = RenderPSOType_DepthOnlyCubeDefault;
				break;

			case LIGHT_SPOT:
				barrier = CD3DX12_RESOURCE_BARRIER::Transition(pCurLight->LightShadowMap.GetSpotLightShadowBufferPtr()->GetResource(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);
				renderPSO = RenderPSOType_DepthOnlyDefault;
				break;

			default:
				__debugbreak();
				break;
		}
		pCommandList->ResourceBarrier(1, &barrier);

		// register object to render queue.
		m_CurThreadIndex = 0;
		for (UINT64 i = 0, size = m_pRenderObjects->size(); i < size; ++i)
		{
			RenderQueue* pRenderQue = m_pppRenderQueue[RenderPass_Shadow][m_CurThreadIndex];
			Model* pModel = (*m_pRenderObjects)[i];

			if (!pModel->bIsVisible || !pModel->bCastShadow)
			{
				continue;
			}

			RenderItem item;
			item.ModelType = (eRenderObjectType)pModel->ModelType;
			item.pObjectHandle = (void*)pModel;
			item.pLight = (void*)pCurLight;
			item.pFilter = nullptr;
			item.PSOType = renderPSO;

			if (pModel->ModelType == RenderObjectType_SkinnedType)
			{
				item.PSOType = (eRenderPSOType)(renderPSO + 1);			
			}

			if (!pRenderQue->Add(&item))
			{
				__debugbreak();
			}
			++m_CurThreadIndex;
		}
	}

	pCommandListPool->ClosedAndExecute(m_pCommandQueue);

#else

	for (int i = 0; i < MAX_LIGHTS; ++i)
	{
		(*m_pLights)[i].RenderShadowMap(m_pRenderObjects);
	}

#endif
}

void Renderer::renderObject()
{
#ifdef USE_MULTI_THREAD

	// register obejct to render queue.
	m_CurThreadIndex = 0;
	for (UINT64 i = 0, size = m_pRenderObjects->size(); i < size; ++i)
	{
		RenderQueue* pRenderQue = m_pppRenderQueue[RenderPass_Object][m_CurThreadIndex];
		Model* pCurModel = (*m_pRenderObjects)[i];

		if (!pCurModel->bIsVisible)
		{
			continue;
		}

		RenderItem item;
		item.ModelType = (eRenderObjectType)pCurModel->ModelType;
		item.pObjectHandle = (void*)pCurModel;
		item.pLight = nullptr;
		item.pFilter = nullptr;

		switch (pCurModel->ModelType)
		{
			case RenderObjectType_DefaultType:
				item.PSOType = RenderPSOType_Default;
				break;

			case RenderObjectType_SkinnedType:
				item.PSOType = RenderPSOType_Skinned;
				break;

			case RenderObjectType_SkyboxType:
				item.PSOType = RenderPSOType_Skybox;
				break;

			default:
				break;
		}

		if (!pRenderQue->Add(&item))
		{
			__debugbreak();
		}
		++m_CurThreadIndex;
	}

#else

	ID3D12GraphicsCommandList* pCommandList = GetCommandList();
	ID3D12DescriptorHeap* pRTVHeap = m_pRTVAllocator->GetDescriptorHeap();
	ID3D12DescriptorHeap* pDSVHeap = m_pDSVAllocator->GetDescriptorHeap();

	const UINT RTV_DESCRIPTOR_SIZE = m_pResourceManager->RTVDescriptorSize;
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(pRTVHeap->GetCPUDescriptorHandleForHeapStart(), m_MainRenderTargetOffset + m_FrameIndex, RTV_DESCRIPTOR_SIZE);
	CD3DX12_CPU_DESCRIPTOR_HANDLE floatRtvHandle(pRTVHeap->GetCPUDescriptorHandleForHeapStart(), m_FloatBufferRTVOffset, RTV_DESCRIPTOR_SIZE);
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(pDSVHeap->GetCPUDescriptorHandleForHeapStart());

	const CD3DX12_RESOURCE_BARRIER RTV_BEFORE_BARRIERS[2] =
	{
		CD3DX12_RESOURCE_BARRIER::Transition(m_pRenderTargets[m_FrameIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET),
		CD3DX12_RESOURCE_BARRIER::Transition(m_pFloatBuffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET),
	};
	pCommandList->ResourceBarrier(2, RTV_BEFORE_BARRIERS);

	const float COLOR[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
	pCommandList->ClearRenderTargetView(rtvHandle, COLOR, 0, nullptr);
	pCommandList->ClearRenderTargetView(floatRtvHandle, COLOR, 0, nullptr);
	pCommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	pCommandList->RSSetViewports(1, &m_ScreenViewport);
	pCommandList->RSSetScissorRects(1, &m_ScissorRect);
	pCommandList->OMSetRenderTargets(1, &floatRtvHandle, FALSE, &dsvHandle);

	for (UINT64 i = 0, size = m_pRenderObjects->size(); i < size; ++i)
	{
		Model* pCurModel = (*m_pRenderObjects)[i];

		if (!pCurModel->bIsVisible)
		{
			continue;
		}

		switch (pCurModel->ModelType)
		{
			case RenderObjectType_DefaultType:
				m_pResourceManager->SetCommonState(RenderPSOType_Default);
				pCurModel->Render(RenderPSOType_Default);
				break;

			case RenderObjectType_SkinnedType:
			{
				SkinnedMeshModel* pCharacter = (SkinnedMeshModel*)pCurModel;
				m_pResourceManager->SetCommonState(RenderPSOType_Skinned);
				pCharacter->Render(RenderPSOType_Skinned);
			}
			break;

			case RenderObjectType_SkyboxType:
				m_pResourceManager->SetCommonState(RenderPSOType_Skybox);
				pCurModel->Render(RenderPSOType_Skybox);
				break;

			default:
				break;
		}
	}

#endif
}

void Renderer::renderMirror()
{
	if (!m_pMirror)
	{
		return;
	}

#ifdef USE_MULTI_THREAD

	// register object to render queue.
	m_CurThreadIndex = 0;
	for (UINT64 i = 0, size = m_pRenderObjects->size(); i < size; ++i)
	{
		RenderQueue* pRenderQue = m_pppRenderQueue[RenderPass_Mirror][m_CurThreadIndex];
		Model* pCurModel = (*m_pRenderObjects)[i];

		if (!pCurModel->bIsVisible)
		{
			continue;
		}

		RenderItem item;
		item.ModelType = (eRenderObjectType)pCurModel->ModelType;
		item.pObjectHandle = (void*)pCurModel;
		item.pLight = nullptr;
		item.pFilter = nullptr;

		switch (pCurModel->ModelType)
		{
			case RenderObjectType_DefaultType:
				item.PSOType = RenderPSOType_ReflectionDefault;
				break;

			case RenderObjectType_SkinnedType:
				item.PSOType = RenderPSOType_ReflectionSkinned;
				break;

			case RenderObjectType_SkyboxType:
				item.PSOType = RenderPSOType_ReflectionSkybox;
				break;

			default:
				break;
		}

		if (!pRenderQue->Add(&item))
		{
			__debugbreak();
		}
		++m_CurThreadIndex;
	}

#else

	// 0.5의 투명도를 가진다고 가정.
	// 거울 위치만 StencilBuffer에 1로 표기.
	m_pResourceManager->SetCommonState(RenderPSOType_StencilMask);
	m_pMirror->Render(RenderPSOType_StencilMask);

	// 거울 위치에 반사된 물체들을 렌더링.
	for (UINT64 i = 0, size = m_pRenderObjects->size(); i < size; ++i)
	{
		Model* pCurModel = (*m_pRenderObjects)[i];

		if (!pCurModel->bIsVisible)
		{
			continue;
		}

		switch (pCurModel->ModelType)
		{
			case RenderObjectType_DefaultType:
			{
				m_pResourceManager->SetCommonState(RenderPSOType_ReflectionDefault);
				pCurModel->Render(RenderPSOType_ReflectionDefault);
			}
			break;

			case RenderObjectType_SkinnedType:
			{
				SkinnedMeshModel* pCharacter = (SkinnedMeshModel*)pCurModel;
				m_pResourceManager->SetCommonState(RenderPSOType_ReflectionSkinned);
				pCharacter->Render(RenderPSOType_ReflectionSkinned);
			}
			break;

			case RenderObjectType_SkyboxType:
			{
				m_pResourceManager->SetCommonState(RenderPSOType_ReflectionSkybox);
				pCurModel->Render(RenderPSOType_ReflectionSkybox);
			}
			break;

			default:
				break;
		}
	}

	// 거울 렌더링.
	m_pResourceManager->SetCommonState(RenderPSOType_MirrorBlend);
	m_pMirror->Render(RenderPSOType_MirrorBlend);

#endif
}

void Renderer::renderObjectBoundingModel()
{
	Renderer* pRenderer = this;

	// obb rendering
	
#ifdef USE_MULTI_THREAD

	// register object to render queue.
	m_CurThreadIndex = 0;
	for (UINT64 i = 0, size = m_pRenderObjects->size(); i < size; ++i)
	{
		RenderQueue* pRenderQue = m_pppRenderQueue[RenderPass_Mirror][m_CurThreadIndex];
		Model* pCurModel = (*m_pRenderObjects)[i];

		if (!pCurModel->bIsVisible)
		{
			continue;
		}

		RenderItem item;
		item.ModelType = (eRenderObjectType)pCurModel->ModelType;
		item.pObjectHandle = (void*)pCurModel;
		item.pLight = nullptr;
		item.pFilter = nullptr;

		switch (pCurModel->ModelType)
		{
			case RenderObjectType_DefaultType:
				item.PSOType = RenderPSOType_ReflectionDefault;
				break;

			case RenderObjectType_SkinnedType:
				item.PSOType = RenderPSOType_ReflectionSkinned;
				break;

			case RenderObjectType_SkyboxType:
				item.PSOType = RenderPSOType_ReflectionSkybox;
				break;

			default:
				break;
		}

		m_pResourceManager->SetCommonState(RenderPSOType_Wire);
		switch (pCurModel->ModelType)
		{
			case RenderObjectType_DefaultType:
				// pCurModel->RenderBoundingSphere(pRenderer, RenderPSOType_Wire);
				break;

			case RenderObjectType_SkinnedType:
			{
				SkinnedMeshModel* pCharacter = (SkinnedMeshModel*)pCurModel;
				pCharacter->RenderBoundingCapsule(pRenderer, RenderPSOType_Wire);
				pCharacter->RenderJointSphere(pRenderer, RenderPSOType_Wire);
			}
			break;

			default:
				break;
		}

		if (!pRenderQue->Add(&item))
		{
			__debugbreak();
		}
		++m_CurThreadIndex;
	}

#else

	for (UINT64 i = 0, size = m_pRenderObjects->size(); i < size; ++i)
	{
		Model* pCurModel = (*m_pRenderObjects)[i];

		if (!pCurModel->bIsVisible)
		{
			continue;
		}

		m_pResourceManager->SetCommonState(RenderPSOType_Wire);
		switch (pCurModel->ModelType)
		{
			case RenderObjectType_DefaultType:
				// pCurModel->RenderBoundingSphere(RenderPSOType_Wire);
				break;

			case RenderObjectType_SkinnedType:
			{
				SkinnedMeshModel* pCharacter = (SkinnedMeshModel*)pCurModel;
				// pCharacter->RenderBoundingSphere(RenderPSOType_Wire);
				// pCharacter->RenderBoundingCapsule(RenderPSOType_Wire);
				pCharacter->RenderJointSphere(RenderPSOType_Wire);
			}
			break;

			default:
				break;
		}
	}

#endif
}

void Renderer::postProcess()
{
#ifdef USE_MULTI_THREAD

	//m_CurThreadIndex = 1;

	//RenderQueue* pRenderQueue = m_pppRenderQueue[RenderPass_Post][m_CurThreadIndex];

	//RenderItem item;
	//item.ModelType = RenderObjectType_DefaultType;
	//item.pObjectHandle = (void*)m_PostProcessor.GetScreenMeshPtr();
	//item.pLight = nullptr;

	//// sampling.
	//item.PSOType = RenderPSOType_Sampling;
	//item.pFilter = (void*)m_PostProcessor.GetSamplingFilterPtr();
	//if (!pRenderQueue->Add(&item))
	//{
	//	__debugbreak();
	//}
	//// +m_CurThreadIndex;

	//// bloom.
	//std::vector<ImageFilter>* pBloomDownFilters = m_PostProcessor.GetBloomDownFiltersPtr();
	//std::vector<ImageFilter>* pBloomUpFilters = m_PostProcessor.GetBloomUpFiltersPtr();

	//item.PSOType = RenderPSOType_BloomDown;
	//for (UINT64 i = 0, size = pBloomDownFilters->size(); i < size; ++i)
	//{
	//	// pRenderQueue = m_ppRenderQueue[RenderPass_Post][m_CurThreadIndex];
	//	item.pFilter = (void*)&(*pBloomDownFilters)[i];

	//	if (!pRenderQueue->Add(&item))
	//	{
	//		__debugbreak();
	//	}
	//	// ++m_CurThreadIndex;
	//}
	//item.PSOType = RenderPSOType_BloomUp;
	//for (UINT64 i = 0, size = pBloomUpFilters->size(); i < size; ++i)
	//{
	//	// pRenderQueue = m_ppRenderQueue[RenderPass_Post][m_CurThreadIndex];
	//	item.pFilter = (void*)&(*pBloomUpFilters)[i];

	//	if (!pRenderQueue->Add(&item))
	//	{
	//		__debugbreak();
	//	}
	//	// ++m_CurThreadIndex;
	//}

	//// combine.
	//item.PSOType = RenderPSOType_Combine;
	//item.pFilter = (void*)m_PostProcessor.GetCombineFilterPtr();
	//if (!pRenderQueue->Add(&item))
	//{
	//	__debugbreak();
	//}

#else

	ID3D12GraphicsCommandList* pCommandList = GetCommandList();

	const CD3DX12_RESOURCE_BARRIER BARRIER = CD3DX12_RESOURCE_BARRIER::Transition(m_pFloatBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON);
	pCommandList->ResourceBarrier(1, &BARRIER);
	m_pPostProcessor->Render(m_FrameIndex);

#endif
}

void Renderer::endRender()
{
#ifdef USE_MULTI_THREAD

	CommandListPool* pCommandListPool = m_pppCommandListPool[m_FrameIndex][0];
	for (int i = 0; i < RenderPass_RenderPassCount; ++i)
	{
		m_pActiveThreadCounts[i] = m_RenderThreadCount;
	}

	// shadow pass.
	for (UINT i = 0; i < m_RenderThreadCount; ++i)
	{
		SetEvent(m_pThreadDescList[i].hEventList[RenderThreadEventType_Shadow]);
	}
	WaitForSingleObject(m_phCompletedEvents[RenderPass_Shadow], INFINITE);
	
	const int TOTAL_LIGHT_TYPE = LIGHT_DIRECTIONAL | LIGHT_POINT | LIGHT_SPOT;
	// resource barrier.
	for (int i = 0; i < MAX_LIGHTS; ++i)
	{
		CD3DX12_RESOURCE_BARRIER barrier;
		ID3D12GraphicsCommandList* pCommandList = pCommandListPool->GetCurrentCommandList();
		Light* pCurLight = &(*m_pLights)[i];

		switch (pCurLight->Property.LightType & TOTAL_LIGHT_TYPE)
		{
			case LIGHT_DIRECTIONAL:
				barrier = CD3DX12_RESOURCE_BARRIER::Transition(pCurLight->LightShadowMap.GetDirectionalLightShadowBufferPtr()->GetResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);
				break;

			case LIGHT_POINT:
				barrier = CD3DX12_RESOURCE_BARRIER::Transition(pCurLight->LightShadowMap.GetPointLightShadowBufferPtr()->GetResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);
				break;

			case LIGHT_SPOT:
				barrier = CD3DX12_RESOURCE_BARRIER::Transition(pCurLight->LightShadowMap.GetSpotLightShadowBufferPtr()->GetResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);
				break;

			default:
				__debugbreak();
				break;
		}

		pCommandList->ResourceBarrier(1, &barrier);
	}
	pCommandListPool->ClosedAndExecute(m_pCommandQueue);

	// default render pass.
	for (UINT i = 0; i < m_RenderThreadCount; ++i)
	{
		SetEvent(m_pThreadDescList[i].hEventList[RenderThreadEventType_Object]);
	}
	WaitForSingleObject(m_phCompletedEvents[RenderPass_Object], INFINITE);

	// mirror pass.
	// stencil process.
	{
		ID3D12GraphicsCommandList* pCommandList = pCommandListPool->GetCurrentCommandList();
		DynamicDescriptorPool* pDescriptorPool = m_pppDescriptorPool[m_FrameIndex][0];
		ConstantBufferManager* pConstantBufferManager = m_pppConstantBufferManager[m_FrameIndex][0];
		ID3D12DescriptorHeap* ppDescriptorHeaps[2] =
		{
			pDescriptorPool->GetDescriptorHeap(),
			m_pResourceManager->m_pSamplerHeap,
		};
		CD3DX12_CPU_DESCRIPTOR_HANDLE floatBufferRtvHandle(m_pResourceManager->m_pRTVHeap->GetCPUDescriptorHandleForHeapStart(), m_FloatBufferRTVOffset, m_pResourceManager->m_RTVDescriptorSize);
		CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_pResourceManager->m_pDSVHeap->GetCPUDescriptorHandleForHeapStart());
		
		m_PostProcessor.SetViewportsAndScissorRects(pCommandList);
		pCommandList->OMSetRenderTargets(1, &floatBufferRtvHandle, FALSE, &dsvHandle);
		pCommandList->SetDescriptorHeaps(2, ppDescriptorHeaps);
		m_pResourceManager->SetCommonState(0, pCommandList, pDescriptorPool, pConstantBufferManager, RenderPSOType_StencilMask);
		m_pMirror->Render(0, pCommandList, pDescriptorPool, pConstantBufferManager, m_pResourceManager, RenderPSOType_StencilMask);
		
		pCommandListPool->ClosedAndExecute(m_pCommandQueue);
	}
	for (UINT i = 0; i < m_RenderThreadCount; ++i)
	{
		SetEvent(m_pThreadDescList[i].hEventList[RenderThreadEventType_Mirror]);
	}
	WaitForSingleObject(m_phCompletedEvents[RenderPass_Mirror], INFINITE);
	// mirror blend process.
	{
		ID3D12GraphicsCommandList* pCommandList = pCommandListPool->GetCurrentCommandList();
		DynamicDescriptorPool* pDescriptorPool = m_pppDescriptorPool[m_FrameIndex][0];
		ConstantBufferManager* pConstantBufferManager = m_pppConstantBufferManager[m_FrameIndex][0];
		ID3D12DescriptorHeap* ppDescriptorHeaps[2] =
		{
			pDescriptorPool->GetDescriptorHeap(),
			m_pResourceManager->m_pSamplerHeap,
		};
		CD3DX12_CPU_DESCRIPTOR_HANDLE floatBufferRtvHandle(m_pResourceManager->m_pRTVHeap->GetCPUDescriptorHandleForHeapStart(), m_FloatBufferRTVOffset, m_pResourceManager->m_RTVDescriptorSize);
		CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_pResourceManager->m_pDSVHeap->GetCPUDescriptorHandleForHeapStart());

		m_PostProcessor.SetViewportsAndScissorRects(pCommandList);
		pCommandList->OMSetRenderTargets(1, &floatBufferRtvHandle, FALSE, &dsvHandle);
		pCommandList->SetDescriptorHeaps(2, ppDescriptorHeaps);
		m_pResourceManager->SetCommonState(0, pCommandList, pDescriptorPool, pConstantBufferManager, RenderPSOType_MirrorBlend);
		m_pMirror->Render(0, pCommandList, pDescriptorPool, pConstantBufferManager, m_pResourceManager, RenderPSOType_MirrorBlend);
		
		const CD3DX12_RESOURCE_BARRIER BARRIER = CD3DX12_RESOURCE_BARRIER::Transition(m_pFloatBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON);
		pCommandList->ResourceBarrier(1, &BARRIER);

		pCommandListPool->ClosedAndExecute(m_pCommandQueue);
	}

	// postprocessing pass.
	{
		ID3D12GraphicsCommandList* pCommandList = pCommandListPool->GetCurrentCommandList();
		DynamicDescriptorPool* pDescriptorPool = m_pppDescriptorPool[m_FrameIndex][0];
		ConstantBufferManager* pConstantBufferManager = m_pppConstantBufferManager[m_FrameIndex][0];
		ID3D12DescriptorHeap* ppDescriptorHeaps[2] =
		{
			pDescriptorPool->GetDescriptorHeap(),
			m_pResourceManager->m_pSamplerHeap,
		};
		
		pCommandList->SetDescriptorHeaps(2, ppDescriptorHeaps);
		m_PostProcessor.Render(0, pCommandList, pDescriptorPool, pConstantBufferManager, m_pResourceManager, m_FrameIndex);
	
		const CD3DX12_RESOURCE_BARRIER RTV_AFTER_BARRIER = CD3DX12_RESOURCE_BARRIER::Transition(m_pRenderTargets[m_FrameIndex], D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT);
		pCommandList->ResourceBarrier(1, &RTV_AFTER_BARRIER);
		pCommandListPool->ClosedAndExecute(m_pCommandQueue);
	}

	for (int i = 0; i < RenderPass_RenderPassCount; ++i)
	{
		for (UINT j = 0; j < m_RenderThreadCount; ++j)
		{
			m_pppRenderQueue[i][j]->Reset();
		}
	}
	
#else

	ID3D12GraphicsCommandList* pCommandList = GetCommandList();
	const CD3DX12_RESOURCE_BARRIER RTV_AFTER_BARRIER = CD3DX12_RESOURCE_BARRIER::Transition(m_pRenderTargets[m_FrameIndex], D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT);
	
	pCommandList->ResourceBarrier(1, &RTV_AFTER_BARRIER);

	m_pppCommandListPool[m_FrameIndex][0]->ClosedAndExecute(m_pCommandQueue);

#endif
}

void Renderer::present()
{
	Fence();

	UINT syncInterval = 1;	  // VSync On
	// UINT syncInterval = 0;  // VSync Off
	UINT presentFlags = 0;

	/*if (!syncInterval)
	{
		presentFlags = DXGI_PRESENT_ALLOW_TEARING;
	}*/

	HRESULT hr = m_pSwapChain->Present(syncInterval, presentFlags);
	if (hr == DXGI_ERROR_DEVICE_REMOVED)
	{
		__debugbreak();
	}

	// for next frame.
	UINT nextFrameIndex = m_pSwapChain->GetCurrentBackBufferIndex();
	WaitForFenceValue(m_LastFenceValues[nextFrameIndex]);

#ifdef USE_MULTI_THREAD

	for (UINT i = 0; i < m_RenderThreadCount; ++i)
	{
		m_pppCommandListPool[nextFrameIndex][i]->Reset();
		m_pppConstantBufferManager[nextFrameIndex][i]->Reset();
		m_pppDescriptorPool[nextFrameIndex][i]->Reset();
	}

#else

	m_pppCommandListPool[nextFrameIndex][0]->Reset();
	m_pppConstantBufferManager[nextFrameIndex][0]->Reset();
	m_pppDescriptorPool[nextFrameIndex][0]->Reset();

#endif

	m_FrameIndex = nextFrameIndex;
}

void Renderer::updateGlobalConstants(const float DELTA_TIME)
{
	const Vector3 EYE_WORLD = m_Camera.GetEyePos();
	const Matrix REFLECTION = Matrix::CreateReflection(*m_pMirrorPlane);
	const Matrix VIEW = m_Camera.GetView();
	const Matrix PROJECTION = m_Camera.GetProjection();

	m_GlobalConstantData.GlobalTime += DELTA_TIME;
	m_GlobalConstantData.EyeWorld = EYE_WORLD;
	m_GlobalConstantData.View = VIEW.Transpose();
	m_GlobalConstantData.Projection = PROJECTION.Transpose();
	m_GlobalConstantData.InverseProjection = PROJECTION.Invert().Transpose();
	m_GlobalConstantData.ViewProjection = (VIEW * PROJECTION).Transpose();
	m_GlobalConstantData.InverseView = VIEW.Invert().Transpose();
	m_GlobalConstantData.InverseViewProjection = m_GlobalConstantData.ViewProjection.Invert();

	memcpy(&m_ReflectionGlobalConstantData, &m_GlobalConstantData, sizeof(GlobalConstant));
	m_ReflectionGlobalConstantData.View = (REFLECTION * VIEW).Transpose();
	m_ReflectionGlobalConstantData.ViewProjection = (REFLECTION * VIEW * PROJECTION).Transpose();
	m_ReflectionGlobalConstantData.InverseViewProjection = m_ReflectionGlobalConstantData.ViewProjection.Invert();
}

void Renderer::updateLightConstants(const float DELTA_TIME)
{
	for (int i = 0; i < MAX_LIGHTS; ++i)
	{
		Light* pLight = &(*m_pLights)[i];

		pLight->Update(DELTA_TIME, m_Camera);
		(*m_pLightSpheres)[i]->UpdateWorld(Matrix::CreateScale(Max(0.01f, pLight->Property.Radius)) * Matrix::CreateTranslation(pLight->Property.Position));
		memcpy(&m_LightConstantData.Lights[i], &pLight->Property, sizeof(LightProperty));
	}
}

void Renderer::onMouseMove(const int MOUSE_X, const int MOUSE_Y)
{
	m_Mouse.MouseX = MOUSE_X;
	m_Mouse.MouseY = MOUSE_Y;

	// 마우스 커서의 위치를 NDC로 변환.
	// 마우스 커서는 좌측 상단 (0, 0), 우측 하단(width-1, height-1).
	// NDC는 좌측 하단이 (-1, -1), 우측 상단(1, 1).
	m_Mouse.MouseNDCX = (float)MOUSE_X * 2.0f / (float)m_ScreenWidth - 1.0f;
	m_Mouse.MouseNDCY = (float)(-MOUSE_Y) * 2.0f / (float)m_ScreenHeight + 1.0f;

	// 커서가 화면 밖으로 나갔을 경우 범위 조절.
	m_Mouse.MouseNDCX = Clamp(m_Mouse.MouseNDCX, -1.0f, 1.0f);
	m_Mouse.MouseNDCY = Clamp(m_Mouse.MouseNDCY, -1.0f, 1.0f);

	// 카메라 시점 회전.
	m_Camera.UpdateMouse(m_Mouse.MouseNDCX, m_Mouse.MouseNDCY);
}

void Renderer::onMouseClick(const int MOUSE_X, const int MOUSE_Y)
{
	m_Mouse.MouseX = MOUSE_X;
	m_Mouse.MouseY = MOUSE_Y;

	m_Mouse.MouseNDCX = (float)MOUSE_X * 2.0f / (float)m_ScreenWidth - 1.0f;
	m_Mouse.MouseNDCY = (float)(-MOUSE_Y) * 2.0f / (float)m_ScreenHeight + 1.0f;
}

void Renderer::processMouseControl(const float DELTA_TIME)
{
	static Model* s_pActiveModel = nullptr;
	static Mesh* s_pEndEffector = nullptr;
	static int s_EndEffectorType = -1;
	static float s_PrevRatio = 0.0f;
	static Vector3 s_PrevPos(0.0f);
	static Vector3 s_PrevVector(0.0f);

	// 적용할 회전과 이동 초기화.
	Quaternion dragRotation = Quaternion::CreateFromAxisAngle(Vector3(1.0f, 0.0f, 0.0f), 0.0f);
	Vector3 dragTranslation(0.0f);
	Vector3 pickPoint(0.0f);
	float dist = 0.0f;

	// 사용자가 두 버튼 중 하나만 누른다고 가정.
	if (m_Mouse.bMouseLeftButton || m_Mouse.bMouseRightButton)
	{
		const Matrix VIEW = m_Camera.GetView();
		const Matrix PROJECTION = m_Camera.GetProjection();
		const Vector3 NDC_NEAR = Vector3(m_Mouse.MouseNDCX, m_Mouse.MouseNDCY, 0.0f);
		const Vector3 NDC_FAR = Vector3(m_Mouse.MouseNDCX, m_Mouse.MouseNDCY, 1.0f);
		const Matrix INV_PROJECTION_VIEW = (VIEW * PROJECTION).Invert();
		const Vector3 WORLD_NEAR = Vector3::Transform(NDC_NEAR, INV_PROJECTION_VIEW);
		const Vector3 WORLD_FAR = Vector3::Transform(NDC_FAR, INV_PROJECTION_VIEW);
		Vector3 dir = WORLD_FAR - WORLD_NEAR;
		dir.Normalize();

		const DirectX::SimpleMath::Ray PICKING_RAY(WORLD_NEAR, dir);

		if (!s_pActiveModel) // 이전 프레임에서 아무 물체도 선택되지 않았을 경우에는 새로 선택.
		{
			Mesh* pEndEffector = nullptr;
			Model* pSelectedModel = pickClosest(PICKING_RAY, &dist, &pEndEffector, &s_EndEffectorType);
			if (pSelectedModel)
			{
#ifdef _DEBUG
				{
					char szDebugString[256];
					sprintf_s(szDebugString, 256, "Newly selected model: %s\n", pSelectedModel->Name.c_str());
					OutputDebugStringA(szDebugString);
				}
#endif
				s_pActiveModel = pSelectedModel;
				s_pEndEffector = pEndEffector;
				m_pPickedModel = s_pActiveModel; // GUI 조작용 포인터.
				m_pPickedEndEffector = s_pEndEffector;
				m_PickedEndEffectorType = s_EndEffectorType;
				pickPoint = PICKING_RAY.position + dist * PICKING_RAY.direction;

				if (s_pEndEffector)
				{
					// 이동만 처리.
					if (m_Mouse.bMouseRightButton)
					{
						m_Mouse.bMouseDragStartFlag = false;
						s_PrevRatio = dist / (WORLD_FAR - WORLD_NEAR).Length();
						s_PrevPos = pickPoint;
					}
				}
				else
				{
					if (m_Mouse.bMouseLeftButton) // 왼쪽 버튼 회전 준비.
					{
						s_PrevVector = pickPoint - s_pActiveModel->BoundingSphere.Center;
						s_PrevVector.Normalize();
					}
					else if (m_Mouse.bMouseRightButton) // 오른쪽 버튼 이동 준비.
					{ 
						m_Mouse.bMouseDragStartFlag = false;
						s_PrevRatio = dist / (WORLD_FAR - WORLD_NEAR).Length();
						s_PrevPos = pickPoint;
					}
				}
			}
		}
		else // 이미 선택된 물체가 있었던 경우.
		{
			if (s_pEndEffector)
			{
				// 이동만 처리.
				if (m_Mouse.bMouseRightButton)
				{
					Vector3 newPos = WORLD_NEAR + s_PrevRatio * (WORLD_FAR - WORLD_NEAR);
					if ((newPos - s_PrevPos).Length() > 1e-3)
					{
						dragTranslation = newPos - s_PrevPos;
						m_PickedTranslation = dragTranslation;
						s_PrevPos = newPos;
					}
					pickPoint = newPos; // Cursor sphere 그려질 위치.
				}
			}
			else
			{
				if (m_Mouse.bMouseLeftButton) // 왼쪽 버튼으로 계속 회전.
				{
					if (PICKING_RAY.Intersects(s_pActiveModel->BoundingSphere, dist))
					{
						pickPoint = PICKING_RAY.position + dist * PICKING_RAY.direction;
					}
					else // 바운딩 스피어에 가장 가까운 점을 찾기.
					{
						Vector3 c = s_pActiveModel->BoundingSphere.Center - WORLD_NEAR;
						Vector3 centerToRay = dir.Dot(c) * dir - c;
						pickPoint = c + centerToRay * Clamp(s_pActiveModel->BoundingSphere.Radius / centerToRay.Length(), 0.0f, 1.0f);
						pickPoint += WORLD_NEAR;
					}

					Vector3 currentVector = pickPoint - s_pActiveModel->BoundingSphere.Center;
					currentVector.Normalize();
					float theta = acos(s_PrevVector.Dot(currentVector));
					if (theta > DirectX::XM_PI / 180.0f * 3.0f)
					{
						Vector3 axis = s_PrevVector.Cross(currentVector);
						axis.Normalize();
						dragRotation = DirectX::SimpleMath::Quaternion::CreateFromAxisAngle(axis, theta);
						s_PrevVector = currentVector;
					}

				}
				else if (m_Mouse.bMouseRightButton) // 오른쪽 버튼으로 계속 이동.
				{
					Vector3 newPos = WORLD_NEAR + s_PrevRatio * (WORLD_FAR - WORLD_NEAR);
					if ((newPos - s_PrevPos).Length() > 1e-3)
					{
						dragTranslation = newPos - s_PrevPos;
						m_PickedTranslation = dragTranslation;
						s_PrevPos = newPos;
					}
					pickPoint = newPos; // Cursor sphere 그려질 위치.
				}
			}
		}
	}
	else
	{
		// 버튼에서 손을 땠을 경우에는 움직일 모델은 nullptr로 설정.
		s_pActiveModel = nullptr;
		s_pEndEffector = nullptr;
		m_pPickedModel = nullptr;
		m_pPickedEndEffector = nullptr;
		m_PickedEndEffectorType = -1;
	}

	if (s_pActiveModel)
	{
		Vector3 translation;
		if (s_pEndEffector)
		{
			MeshConstant& meshConstantData = s_pEndEffector->MeshConstantData;
			translation = meshConstantData.World.Transpose().Translation() + dragTranslation;
			m_PickedTranslation = translation;

			SkinnedMeshModel* pCharacter = (SkinnedMeshModel*)s_pActiveModel;
			switch (s_EndEffectorType)
			{
				case 0:
					OutputDebugStringA("RightArm Control.\n");
					break;

				case 1:
					OutputDebugStringA("LeftArm Control.\n");
					break;

				case 2:
					OutputDebugStringA("RightLeg Control.\n");
					break;
					
				case 3:
					OutputDebugStringA("LeftLeg Control.\n");
					break;

				default:
					__debugbreak();
					break;
			}
		}
		else
		{
			translation = s_pActiveModel->World.Translation();
			s_pActiveModel->World.Translation(Vector3(0.0f));
			s_pActiveModel->UpdateWorld(s_pActiveModel->World* Matrix::CreateFromQuaternion(dragRotation)* Matrix::CreateTranslation(dragTranslation + translation));
			s_pActiveModel->BoundingSphere.Center = s_pActiveModel->World.Translation();
		}

		// 충돌 지점에 작은 구 그리기.
		/*m_pCursorSphere->bIsVisible = true;
		m_pCursorSphere->UpdateWorld(Matrix::CreateTranslation(pickPoint));*/
	}
	else
	{
		// m_pCursorSphere->bIsVisible = false;
	}
}

Model* Renderer::pickClosest(const DirectX::SimpleMath::Ray& PICKING_RAY, float* pMinDist, Mesh** ppEndEffector, int* pEndEffectorType)
{
	*pMinDist = 1e5f;
	Model* pMinModel = nullptr;

	for (UINT64 i = 0, size = m_pRenderObjects->size(); i < size; ++i)
	{
		Model* pCurModel = (*m_pRenderObjects)[i];
		float dist = 0.0f;

		switch (pCurModel->ModelType)
		{
			case RenderObjectType_DefaultType:
			{
				if (pCurModel->bIsPickable &&
					PICKING_RAY.Intersects(pCurModel->BoundingSphere, dist) &&
					dist < *pMinDist)
				{
					pMinModel = pCurModel;
					*pMinDist = dist;
				}
			}
			break;

			case RenderObjectType_SkinnedType:
			{
				if (pCurModel->bIsPickable &&
					PICKING_RAY.Intersects(pCurModel->BoundingSphere, dist) &&
					dist < *pMinDist)
				{
					pMinModel = pCurModel;
					*pMinDist = dist;
					dist = FLT_MAX;

					// 4개 end-effector 중 어디에 해당되는 지 확인.
					SkinnedMeshModel* pCharacter = (SkinnedMeshModel*)pCurModel;
					/*{
						std::string debugString;

						Vector3 rayOriginToSphereCencter(Vector3(pCharacter->BoundingSphere.Center) - PICKING_RAY.position);
						float distAtRayAndSphereCenter = rayOriginToSphereCencter.Cross(PICKING_RAY.direction).Length() / PICKING_RAY.direction.Length();
						debugString = std::string("rayOriginToSphereCenter: ") + std::to_string(distAtRayAndSphereCenter) + std::string("\n");
						OutputDebugStringA(debugString.c_str());

						Vector3 rayOriginToRightToeCenter(Vector3(pCharacter->RightToe.Center) - PICKING_RAY.position);
						float distAtRayAndRightToeCenter = rayOriginToRightToeCenter.Cross(PICKING_RAY.direction).Length() / PICKING_RAY.direction.Length();
						debugString = std::string("distAtRayAndRightToeCenter: ") + std::to_string(distAtRayAndRightToeCenter) + std::string("\n");
						OutputDebugStringA(debugString.c_str());

						Vector3 rayOriginToLeftToeCenter(Vector3(pCharacter->LeftToe.Center) - PICKING_RAY.position);
						float distAtRayAndLeftToeCenter = rayOriginToRightToeCenter.Cross(PICKING_RAY.direction).Length() / PICKING_RAY.direction.Length();
						debugString = std::string("distAtRayAndLeftToeCenter: ") + std::to_string(distAtRayAndLeftToeCenter) + std::string("\n");
						OutputDebugStringA(debugString.c_str());

						Vector3 rayOriginToRightHandCenter(Vector3(pCharacter->RightHandMiddle.Center) - PICKING_RAY.position);
						float distAtRayAndRightHandCenter = rayOriginToRightHandCenter.Cross(PICKING_RAY.direction).Length() / PICKING_RAY.direction.Length();
						debugString = std::string("distAtRayAndRightHand: ") + std::to_string(distAtRayAndRightHandCenter) + std::string("\n");
						OutputDebugStringA(debugString.c_str());

						Vector3 rayOriginToLeftHandCenter(Vector3(pCharacter->LeftHandMiddle.Center) - PICKING_RAY.position);
						float distAtRayAndLeftHandCenter = rayOriginToLeftHandCenter.Cross(PICKING_RAY.direction).Length() / PICKING_RAY.direction.Length();
						debugString = std::string("distAtRayAndLeftHand: ") + std::to_string(distAtRayAndLeftHandCenter) + std::string("\n");
						OutputDebugStringA(debugString.c_str());

						// Vector3 rightToeToCenter(Vector3(pCharacter->BoundingSphere.Center) - Vector3(pCharacter->RightToe.Center));
						// Vector3 leftToeToCenter(Vector3(pCharacter->BoundingSphere.Center) - Vector3(pCharacter->LeftToe.Center));
						// Vector3 rightHandToCenter(Vector3(pCharacter->BoundingSphere.Center) - Vector3(pCharacter->RightHandMiddle.Center));
						// Vector3 leftHandToCenter(Vector3(pCharacter->BoundingSphere.Center) - Vector3(pCharacter->LeftHandMiddle.Center));
						//
						// debugString = std::string("rightToeToCenter: ") + std::to_string(rightToeToCenter.Length()) + std::string("\n");
						// OutputDebugStringA(debugString.c_str());
						// debugString = std::string("leftToeToCenter: ") + std::to_string(leftToeToCenter.Length()) + std::string("\n");
						// OutputDebugStringA(debugString.c_str());
						// debugString = std::string("rightHandToCenter: ") + std::to_string(rightHandToCenter.Length()) + std::string("\n");
						// OutputDebugStringA(debugString.c_str());
						// debugString = std::string("leftHandToCenter: ") + std::to_string(leftHandToCenter.Length()) + std::string("\n");
						// OutputDebugStringA(debugString.c_str());
						// debugString = std::string("sphere radius: ") + std::to_string(pCharacter->BoundingSphere.Radius) + std::string("\n");
						// OutputDebugStringA(debugString.c_str());

						OutputDebugStringA("\n\n");
					}*/

					//if (PICKING_RAY.Intersects(pCharacter->RightHandMiddle, dist) &&
					//	dist < *pMinDist)
					//{
					//	*ppEndEffector = *(pCharacter->GetRightArmsMesh() + 3);
					//	*pMinDist = dist;
					//}
					//if (PICKING_RAY.Intersects(pCharacter->LeftHandMiddle, dist) &&
					//	dist < *pMinDist)
					//{
					//	*ppEndEffector = *(pCharacter->GetLeftArmsMesh() + 3);
					//	*pMinDist = dist;
					//}
					//if (PICKING_RAY.Intersects(pCharacter->RightToe, dist) &&
					//	dist < *pMinDist)
					//{
					//	*ppEndEffector = *(pCharacter->GetRightLegsMesh() + 3);
					//	*pMinDist = dist;
					//}
					//if (PICKING_RAY.Intersects(pCharacter->LeftToe, dist) &&
					//	dist < *pMinDist)
					//{
					//	*ppEndEffector = *(pCharacter->GetLeftLegsMesh() + 3);
					//	*pMinDist = dist;
					//}

					// 기존 picking 방식을 사용하면, 각 joint별 sphere가 너무 작아서인지 오차가 생기는 것 같음.
					// 그래서 가정을 하나 함. 모델 picking하면 end-effector를 잡는다고 가정.
					// picking ray와 각 end-effector 중심 거리가 가장 작은 것을 선택.
					Vector3 rayToEndEffectorCenter;
					float rayToEndEffectorDist;
					const float PICKING_RAY_DIR_LENGTH = PICKING_RAY.direction.Length();

					// right hand middle.
					rayToEndEffectorCenter = pCharacter->RightHandMiddle.Center - PICKING_RAY.position;
					rayToEndEffectorDist = rayToEndEffectorCenter.Cross(PICKING_RAY.direction).Length() / PICKING_RAY_DIR_LENGTH;
					if (rayToEndEffectorDist < dist)
					{
						*ppEndEffector = *(pCharacter->GetRightArmsMesh() + 3);
						*pEndEffectorType = 0;
						dist = rayToEndEffectorDist;
					}

					// left hand middle.
					rayToEndEffectorCenter = pCharacter->LeftHandMiddle.Center - PICKING_RAY.position;
					rayToEndEffectorDist = rayToEndEffectorCenter.Cross(PICKING_RAY.direction).Length() / PICKING_RAY_DIR_LENGTH;
					if (rayToEndEffectorDist < dist)
					{
						*ppEndEffector = *(pCharacter->GetLeftArmsMesh() + 3);
						*pEndEffectorType = 1;
						dist = rayToEndEffectorDist;
					}

					// right toe.
					rayToEndEffectorCenter = pCharacter->RightToe.Center - PICKING_RAY.position;
					rayToEndEffectorDist = rayToEndEffectorCenter.Cross(PICKING_RAY.direction).Length() / PICKING_RAY_DIR_LENGTH;
					if (rayToEndEffectorDist < dist)
					{
						*ppEndEffector = *(pCharacter->GetRightLegsMesh() + 3);
						*pEndEffectorType = 2;
						dist = rayToEndEffectorDist;
					}

					// left toe.
					rayToEndEffectorCenter = pCharacter->LeftToe.Center - PICKING_RAY.position;
					rayToEndEffectorDist = rayToEndEffectorCenter.Cross(PICKING_RAY.direction).Length() / PICKING_RAY_DIR_LENGTH;
					if (rayToEndEffectorDist < dist)
					{
						*ppEndEffector = *(pCharacter->GetLeftLegsMesh() + 3);
						*pEndEffectorType = 3;
						dist = rayToEndEffectorDist;
					}
				}
			}
			break;

			default:
				break;
		}
	}

	return pMinModel;
}
