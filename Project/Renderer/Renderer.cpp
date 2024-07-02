#include "../pch.h"
#include "../Model/GeometryGenerator.h"
#include "../Util/Utility.h"
#include "Renderer.h"

Renderer* g_pRendrer = nullptr;
Renderer* Renderer::s_pRenderer = nullptr;
ResourceManager* Renderer::m_pResourceManager = nullptr;

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return g_pRendrer->MsgProc(hWnd, msg, wParam, lParam);
}

Renderer::Renderer()
{
	s_pRenderer = this;
	g_pRendrer = s_pRenderer;
	m_Camera.SetAspectRatio((float)m_ScreenWidth / (float)m_ScreenHeight);
}

Renderer::~Renderer()
{
	s_pRenderer = nullptr;
	g_pRendrer = nullptr;
	Clear();
}

void Renderer::Initizlie()
{
	initMainWidndow();
	initDirect3D();
	initScene();
	initDescriptorHeap();

	PostProcessor::PostProcessingBuffers config =
	{
		{ m_pRenderTargets[0], m_pRenderTargets[1] },
		m_pFloatBuffer,
		m_pPrevBuffer,
		&m_GlobalConstant,
		m_MainRenderTargetOffset, m_MainRenderTargetOffset + 1, m_FloatBufferSRVOffset, m_PrevBufferSRVOffset
	};
	m_PostProcessor.Initizlie(m_pResourceManager, config, m_ScreenWidth, m_ScreenHeight, 4);
	m_PostProcessor.SetDescriptorHeap(m_pResourceManager);

	m_DynamicDescriptorPool.Initialize(m_pDevice, 768);

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

#if !SINGLETHREAD
	initThreadAndEvent();
#endif
}

int Renderer::Run()
{
	MSG msg = { 0, };

	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			m_Timer.Tick();
			
			static UINT s_FrameCount = 0;
			static UINT64 s_PrevUpdateTick = 0;
			static UINT64 s_PrevFrameCheckTick = 0;

			float frameTime = (float)m_Timer.GetElapsedSeconds();
			float frameChange = 2.0f * frameTime;
			UINT64 curTick = GetTickCount64();

			++s_FrameCount;

			Update(frameChange);
			s_PrevUpdateTick = curTick;
			Render();

			if (curTick - s_PrevFrameCheckTick > 1000)
			{
				s_PrevFrameCheckTick = curTick;

				WCHAR txt[64];
				swprintf_s(txt, L"KHU  %uFPS", s_FrameCount);
				SetWindowText(m_hMainWindow, txt);

				s_FrameCount = 0;
			}
		}
	}

	return (int)msg.wParam;
}

void Renderer::Update(const float DELTA_TIME)
{
	m_Camera.UpdateKeyboard(DELTA_TIME, m_bKeyPressed);
	processMouseControl();

	updateGlobalConstants(DELTA_TIME);
	updateLightConstants(DELTA_TIME);

	if (m_pMirror)
	{
		m_pMirror->UpdateConstantBuffers();
	}
	for (UINT64 i = 0, size = m_RenderObjects.size(); i < size; ++i)
	{
		Model* pCurModel = m_RenderObjects[i];
		pCurModel->UpdateConstantBuffers();
	}

	m_pCharacter->UpdateConstantBuffers();
	updateAnimation(DELTA_TIME);
}

void Renderer::Render()
{
#if SINGLETHREAD
	beginRender();

	shadowMapRender();
	objectRender();
	mirrorRender();
	postProcess();

	endRender();

	present();

#else
	DWORD waitReturn = 0;

	preRender();
	for (int i = 0; i < LIGHT_THREADS; ++i)
	{
		SetEvent(m_hBeginShadowPass[i]); // Tell each worker to start drawing.
	}
	midRender();
	for (int i = 0; i < NUM_THREADS; ++i)
	{
		SetEvent(m_hBeginRenderPass[i]); // Tell each worker to start drawing.
	}
	postRender();

	waitReturn = WaitForMultipleObjects(LIGHT_THREADS, m_hFinishShadowPass, TRUE, INFINITE);
	m_pCommandQueue->ExecuteCommandLists(LIGHT_THREADS + 1, m_pResourceManager->m_pBatchSubmits); // Submit MID and shadows.

	waitReturn = WaitForMultipleObjects(NUM_THREADS, m_hFinishMainRenderPass, TRUE, INFINITE);
	m_pCommandQueue->ExecuteCommandLists(NUM_THREADS * 2 + 1, m_pResourceManager->m_pBatchSubmits + LIGHT_THREADS + 1); // Submit mainRender, mirrrorRender, POST

	present();
#endif
}

void Renderer::Clear()
{
	waitForFenceValue();

	if (m_hFenceEvent)
	{
		CloseHandle(m_hFenceEvent);
		m_hFenceEvent = nullptr;
	}
	m_FenceValue = 0;
	SAFE_RELEASE(m_pFence);

	for (int i = 0; i < LIGHT_THREADS; ++i)
	{
		CloseHandle(m_hBeginShadowPass[i]);
		CloseHandle(m_hFinishShadowPass[i]);
	}
	for (int i = 0; i < NUM_THREADS; ++i)
	{
		CloseHandle(m_hBeginRenderPass[i]);
		CloseHandle(m_hFinishMainRenderPass[i]);
		CloseHandle(m_hThreadHandles[i]);
	}

	m_pMirror = nullptr;
	if (m_pCharacter)
	{
		delete m_pCharacter;
		m_pCharacter = nullptr;
	}
	if (m_pSkybox)
	{
		delete m_pSkybox;
		m_pSkybox = nullptr;
	}
	if (m_pGround)
	{
		delete m_pGround;
		m_pGround = nullptr;
	}
	for (UINT64 i = 0, size = m_RenderObjects.size(); i < size; ++i)
	{
		Model* pCurModel = m_RenderObjects[i];
		delete pCurModel;
		pCurModel = nullptr;
	}
	m_RenderObjects.clear();

	m_Lights.clear();

	m_GlobalConstant.Clear();
	m_LightConstant.Clear();
	m_ReflectionGlobalConstant.Clear();
	m_EnvTexture.Clear();
	m_IrradianceTexture.Clear();
	m_SpecularTexture.Clear();
	m_BRDFTexture.Clear();

	if (m_pResourceManager)
	{
		delete m_pResourceManager;
		m_pResourceManager = nullptr;
	}

	m_PostProcessor.Clear();
	m_DynamicDescriptorPool.Clear();

	SAFE_RELEASE(m_pDefaultDepthStencil);

	SAFE_RELEASE(m_pFloatBuffer);
	SAFE_RELEASE(m_pPrevBuffer);
	for (UINT i = 0; i < SWAP_CHAIN_FRAME_COUNT; ++i)
	{
		SAFE_RELEASE(m_pRenderTargets[i]);
	}

	SAFE_RELEASE(m_pCommandList);
	SAFE_RELEASE(m_pCommandAllocator);
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
			// 화면 해상도가 바뀌면 SwapChain을 다시 생성.
			if (m_pSwapChain)
			{
				m_ScreenWidth = (UINT)LOWORD(lParam);
				m_ScreenHeight = (UINT)HIWORD(lParam);

				m_ScreenViewport.Width = (float)m_ScreenWidth;
				m_ScreenViewport.Height = (float)m_ScreenHeight;
				m_ScissorRect.right = m_ScreenWidth;
				m_ScissorRect.bottom = m_ScreenHeight;

				// 윈도우가 Minimize 모드에서는 screenWidth/Height가 0.
				if (m_ScreenWidth && m_ScreenHeight)
				{
#ifdef _DEBUG
					char debugString[256] = { 0, };
					OutputDebugStringA("Resize SwapChain to ");
					sprintf(debugString, "%d", m_ScreenWidth);
					OutputDebugStringA(debugString);
					OutputDebugStringA(" ");
					sprintf(debugString, "%d", m_ScreenHeight);
					OutputDebugStringA(debugString);
					OutputDebugStringA("\n");
#endif 

					// 기존 버퍼 초기화.
					for (UINT i = 0; i < SWAP_CHAIN_FRAME_COUNT; ++i)
					{
						m_pRenderTargets[i]->Release();
						m_pRenderTargets[i] = nullptr;
					}
					m_pFloatBuffer->Release();
					m_pFloatBuffer = nullptr;
					m_pPrevBuffer->Release();
					m_pPrevBuffer = nullptr;
					m_pDefaultDepthStencil->Release();
					m_pDefaultDepthStencil = nullptr;
					m_PostProcessor.Clear();
					m_FrameIndex = m_pSwapChain->GetCurrentBackBufferIndex();

					// swap chain resize.
					m_pSwapChain->ResizeBuffers(0,					 // 현재 개수 유지.
												m_ScreenWidth,		 // 해상도 변경.
												m_ScreenHeight,
												DXGI_FORMAT_UNKNOWN, // 현재 포맷 유지.
												0);
					// float buffer, prev buffer 재생성.
					{
						HRESULT hr = S_OK;

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

						D3D12_HEAP_PROPERTIES heapProps = {};
						heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
						heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
						heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
						heapProps.CreationNodeMask = 1;
						heapProps.VisibleNodeMask = 1;

						hr = m_pDevice->CreateCommittedResource(&heapProps,
																D3D12_HEAP_FLAG_NONE,
																&resourceDesc,
																D3D12_RESOURCE_STATE_COMMON,
																nullptr,
																IID_PPV_ARGS(&m_pFloatBuffer));
						BREAK_IF_FAILED(hr);
						m_pFloatBuffer->SetName(L"FloatBuffer");


						resourceDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
						resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
						hr = m_pDevice->CreateCommittedResource(&heapProps,
																D3D12_HEAP_FLAG_NONE,
																&resourceDesc,
																D3D12_RESOURCE_STATE_COMMON,
																nullptr,
																IID_PPV_ARGS(&m_pPrevBuffer));
						BREAK_IF_FAILED(hr);
						m_pPrevBuffer->SetName(L"PrevBuffer");
					}

					// 변경된 크기에 맞춰 rtv, dsv, srv 재생성.
					{
						const UINT RTV_DESCRIPTOR_SIZE = m_pResourceManager->m_RTVDescriptorSize;
						CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_pResourceManager->m_pRTVHeap->GetCPUDescriptorHandleForHeapStart(), m_MainRenderTargetOffset, RTV_DESCRIPTOR_SIZE);
						CD3DX12_CPU_DESCRIPTOR_HANDLE startRtvHandle(m_pResourceManager->m_pRTVHeap->GetCPUDescriptorHandleForHeapStart());
						for (UINT i = 0; i < SWAP_CHAIN_FRAME_COUNT; ++i)
						{
							m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&m_pRenderTargets[i]));
							m_pDevice->CreateRenderTargetView(m_pRenderTargets[i], nullptr, rtvHandle);
							rtvHandle.Offset(1, m_pResourceManager->m_RTVDescriptorSize);
						}
						rtvHandle = startRtvHandle;

						D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
						rtvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
						rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
						rtvDesc.Texture2D.MipSlice = 0;
						rtvDesc.Texture2D.PlaneSlice = 0;
						rtvHandle.Offset(m_FloatBufferRTVOffset, RTV_DESCRIPTOR_SIZE);
						m_pDevice->CreateRenderTargetView(m_pFloatBuffer, &rtvDesc, rtvHandle);
					}
					{
						HRESULT hr = S_OK;

						D3D12_RESOURCE_DESC dsvDesc;
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
						dsvDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

						D3D12_HEAP_PROPERTIES heapProps;
						heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
						heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
						heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
						heapProps.CreationNodeMask = 1;
						heapProps.VisibleNodeMask = 1;

						D3D12_CLEAR_VALUE clearValue;
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

						D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc = {};
						depthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
						depthStencilViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
						depthStencilViewDesc.Texture2D.MipSlice = 0;

						CD3DX12_CPU_DESCRIPTOR_HANDLE depthHandle(m_pResourceManager->m_pDSVHeap->GetCPUDescriptorHandleForHeapStart());
						m_pDevice->CreateDepthStencilView(m_pDefaultDepthStencil, &depthStencilViewDesc, depthHandle);
					}
					{
						const UINT SRV_DESCRIPTOR_SIZE = m_pResourceManager->m_CBVSRVUAVDescriptorSize;
						CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(m_pResourceManager->m_pCBVSRVUAVHeap->GetCPUDescriptorHandleForHeapStart(), m_FloatBufferSRVOffset, SRV_DESCRIPTOR_SIZE);
						CD3DX12_CPU_DESCRIPTOR_HANDLE startSrvHandle(m_pResourceManager->m_pCBVSRVUAVHeap->GetCPUDescriptorHandleForHeapStart());
						
						D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
						srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
						srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
						srvDesc.Texture2D.MostDetailedMip = 0;
						srvDesc.Texture2D.MipLevels = 1;
						srvDesc.Texture2D.PlaneSlice = 0;
						srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

						// float buffer srv
						srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
						m_pDevice->CreateShaderResourceView(m_pFloatBuffer, &srvDesc, srvHandle);

						// prev buffer srv
						srvHandle = startSrvHandle;
						srvHandle.Offset(m_PrevBufferSRVOffset, SRV_DESCRIPTOR_SIZE);
						srvDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
						m_pDevice->CreateShaderResourceView(m_pPrevBuffer, &srvDesc, srvHandle);
					}

					PostProcessor::PostProcessingBuffers config =
					{
						{ m_pRenderTargets[0], m_pRenderTargets[1] },
						m_pFloatBuffer,
						m_pPrevBuffer,
						&m_GlobalConstant,
						m_MainRenderTargetOffset, m_MainRenderTargetOffset + 1, m_FloatBufferSRVOffset, m_PrevBufferSRVOffset
					};
					m_PostProcessor.Initizlie(m_pResourceManager, config, m_ScreenWidth, m_ScreenHeight, 4);
					m_PostProcessor.SetDescriptorHeap(m_pResourceManager);
					m_Camera.SetAspectRatio((float)m_ScreenWidth / (float)m_ScreenHeight);
				}
			}
		}
		break;

		case WM_SYSCOMMAND:
		{
			if ((wParam & 0xfff0) == SC_KEYMENU) // ALT키 비활성화.
			{
				return 0;
			}
		}
		break;

		case WM_MOUSEMOVE:
			onMouseMove(LOWORD(lParam), HIWORD(lParam));
			break;

		case WM_LBUTTONDOWN:
		{
			if (!m_bMouseLeftButton)
			{
				m_bMouseDragStartFlag = true; // 드래그를 새로 시작하는지 확인.
			}
			m_bMouseLeftButton = true;
			onMouseClick(LOWORD(lParam), HIWORD(lParam));
		}
		break;

		case WM_LBUTTONUP:
			m_bMouseLeftButton = false;
			break;

		case WM_RBUTTONDOWN:
		{
			if (!m_bMouseRightButton)
			{
				m_bMouseDragStartFlag = true; // 드래그를 새로 시작하는지 확인.
			}
			m_bMouseRightButton = true;
		}
		break;

		case WM_RBUTTONUP:
			m_bMouseRightButton = false;
			break;

		case WM_KEYDOWN:
		{
			m_bKeyPressed[wParam] = true;
			if (wParam == VK_ESCAPE) // ESC키 종료.
			{
				DestroyWindow(m_hMainWindow);
			}
			if (wParam == VK_SPACE)
			{
				m_Lights[1].bRotated = !m_Lights[1].bRotated;
			}
		}
		break;

		case WM_KEYUP:
		{
			if (wParam == 'F')  // f키 일인칭 시점.
			{
				m_Camera.bUseFirstPersonView = !m_Camera.bUseFirstPersonView;
			}

			m_bKeyPressed[wParam] = false;
		}
		break;

		case WM_MOUSEWHEEL:
			m_WheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
			break;

		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;

		default:
			break;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}

void Renderer::ThreadWork(ThreadParameter* pParameter)
{
	int threadIndex = pParameter->ThreadIndex;
	ID3D12DescriptorHeap* ppDescriptorHeaps[2] =
	{
		m_DynamicDescriptorPool.GetDescriptorHeap(),
		m_pResourceManager->m_pSamplerHeap
	};
	const UINT RTV_DESCRIPTOR_SIZE = m_pResourceManager->m_RTVDescriptorSize; 
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_pResourceManager->m_pRTVHeap->GetCPUDescriptorHandleForHeapStart(), m_MainRenderTargetOffset + m_FrameIndex, RTV_DESCRIPTOR_SIZE);
	CD3DX12_CPU_DESCRIPTOR_HANDLE floatRtvHandle(m_pResourceManager->m_pRTVHeap->GetCPUDescriptorHandleForHeapStart(), m_FloatBufferRTVOffset, RTV_DESCRIPTOR_SIZE);
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_pResourceManager->m_pDSVHeap->GetCPUDescriptorHandleForHeapStart());

	_ASSERT(threadIndex >= 0);
	_ASSERT(threadIndex < NUM_THREADS);

	while (threadIndex >= 0 && threadIndex < NUM_THREADS)
	{
		WaitForSingleObject(m_hBeginRenderPass[threadIndex], INFINITE);
		
		ID3D12GraphicsCommandList* pRenderCommandList = m_pResourceManager->m_pRenderCommandLists[threadIndex];
		ID3D12GraphicsCommandList* pMirrorCommandList = m_pResourceManager->m_pMirrorCommandLists[threadIndex];

		pRenderCommandList->RSSetViewports(1, &m_ScreenViewport);
		pRenderCommandList->RSSetScissorRects(1, &m_ScissorRect);
		pRenderCommandList->OMSetRenderTargets(1, &floatRtvHandle, FALSE, &dsvHandle);

		// main render pass.
		pRenderCommandList->SetDescriptorHeaps(2, ppDescriptorHeaps);
		switch (threadIndex)
		{
			case 0:
				m_pResourceManager->SetCommonState(pRenderCommandList, Skybox);
				m_pSkybox->Render(m_pResourceManager, pRenderCommandList, Skybox);
				break;

			case 1: case 2: case 3: case 4:
				m_pResourceManager->SetCommonState(pRenderCommandList, Default);
				for (UINT64 i = threadIndex - 1, size = m_RenderObjects.size(); i < size; ++i)
				{
					Model* pCurModel = m_RenderObjects[i];

					if (pCurModel->bIsVisible)
					{
						pCurModel->Render(m_pResourceManager, pRenderCommandList, Default);
					}
				}
				break;

			case 5:
				m_pResourceManager->SetCommonState(pRenderCommandList, Skinned);
				m_pCharacter->Render(m_pResourceManager, pRenderCommandList, Skinned);
				break;

			default:
				__debugbreak();
				break;
		}

		pRenderCommandList->Close();
		

		// mirror blend pass.
		if (!m_pMirror)
		{
			goto LB_FIN;
		}

		pMirrorCommandList->RSSetViewports(1, &m_ScreenViewport);
		pMirrorCommandList->RSSetScissorRects(1, &m_ScissorRect);
		pMirrorCommandList->OMSetRenderTargets(1, &floatRtvHandle, FALSE, &dsvHandle);
		pMirrorCommandList->SetDescriptorHeaps(2, ppDescriptorHeaps);
		switch (threadIndex)
		{
			case 0:
				m_pResourceManager->SetCommonState(pMirrorCommandList, StencilMask);
				m_pMirror->Render(m_pResourceManager, pMirrorCommandList, StencilMask);
				break;

			case 1: case 2: case 3:
				m_pResourceManager->SetCommonState(pMirrorCommandList, ReflectionDefault);
				for (UINT64 i = threadIndex - 1, size = m_RenderObjects.size(); i < size; i += NUM_THREADS)
				{
					Model* pCurModel = m_RenderObjects[i];
					pCurModel->Render(m_pResourceManager, pMirrorCommandList, ReflectionDefault);
				}
				break;

			case 4:
				m_pResourceManager->SetCommonState(pMirrorCommandList, ReflectionSkinned);
				m_pCharacter->Render(m_pResourceManager, pMirrorCommandList, ReflectionSkinned);
				break;

			case 5:
				m_pResourceManager->SetCommonState(pMirrorCommandList, ReflectionSkybox);
				m_pSkybox->Render(m_pResourceManager, pMirrorCommandList, ReflectionSkybox);
				m_pResourceManager->SetCommonState(pMirrorCommandList, MirrorBlend);
				m_pMirror->Render(m_pResourceManager, pMirrorCommandList, MirrorBlend);
				break;

			default:
				__debugbreak();
				break;
		}

	LB_FIN:
		pMirrorCommandList->Close();
		SetEvent(m_hFinishMainRenderPass[threadIndex]);
	}
}

void Renderer::ThreadWork2(ThreadParameter* pParameter)
{
	int threadIndex = pParameter->ThreadIndex;
	ID3D12DescriptorHeap* ppDescriptorHeaps[2] =
	{
		m_DynamicDescriptorPool.GetDescriptorHeap(),
		m_pResourceManager->m_pSamplerHeap
	};

	_ASSERT(threadIndex >= 0);
	_ASSERT(threadIndex < LIGHT_THREADS);

	while (threadIndex >= 0 && threadIndex < LIGHT_THREADS)
	{
		WaitForSingleObject(m_hBeginShadowPass[threadIndex], INFINITE);

		ID3D12GraphicsCommandList* pShadowCommandList = m_pResourceManager->m_pShadowCommandLists[threadIndex];

		// shadow pass.
		pShadowCommandList->SetDescriptorHeaps(2, ppDescriptorHeaps);
		for (UINT64 i = threadIndex, size = m_Lights.size(); i < size; i += LIGHT_THREADS)
		{
			m_Lights[i].RenderShadowMap(m_pResourceManager, pShadowCommandList, m_RenderObjects, (SkinnedMeshModel*)m_pCharacter, m_pMirror);
		}

		pShadowCommandList->Close();
		SetEvent(m_hFinishShadowPass[threadIndex]);
	}
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
		L"KHUKHU",					// lpszClassName
		NULL						// hIconSm
	};

	if (!RegisterClassEx(&wc))
	{
		__debugbreak();
	}

	RECT wr = { 0, 0, (long)m_ScreenWidth, (long)m_ScreenHeight };
	BOOL bResult = AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
	m_hMainWindow = CreateWindow(wc.lpszClassName,
								 L"KHU",
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

	const D3D_FEATURE_LEVEL FEATURE_LEVELS[] =
	{
		D3D_FEATURE_LEVEL_12_2,
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};
	UINT numFeatureLevels = _countof(FEATURE_LEVELS);
	IDXGIFactory5* pFactory5 = nullptr;
	IDXGIAdapter3* pAdapter3 = nullptr;

	hr = CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&pFactory5));
	BREAK_IF_FAILED(hr);

	for (UINT featureLevelIndex = 0; featureLevelIndex < numFeatureLevels; ++featureLevelIndex)
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
		swapChainDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = SWAP_CHAIN_FRAME_COUNT;
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.Scaling = DXGI_SCALING_NONE;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
		swapChainDesc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsSwapChainDesc = {};
		// fsSwapChainDesc.RefreshRate.Numerator = 60;
		// fsSwapChainDesc.RefreshRate.Denominator = 1;
		fsSwapChainDesc.Windowed = TRUE;

		IDXGISwapChain1* pSwapChain1 = nullptr;
		hr = pFactory5->CreateSwapChainForHwnd(m_pCommandQueue, m_hMainWindow, &swapChainDesc, &fsSwapChainDesc, nullptr, &pSwapChain1);
		BREAK_IF_FAILED(hr);
		pSwapChain1->QueryInterface(IID_PPV_ARGS(&m_pSwapChain));
		SAFE_RELEASE(pSwapChain1);

		m_BackBufferFormat = swapChainDesc.Format;

		m_FrameIndex = m_pSwapChain->GetCurrentBackBufferIndex();
	}

	// create single command list
	{
		hr = m_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_pCommandAllocator));
		BREAK_IF_FAILED(hr);
		m_pCommandAllocator->SetName(L"CommandAllocator");

		hr = m_pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_pCommandAllocator, nullptr, IID_PPV_ARGS(&m_pCommandList));
		BREAK_IF_FAILED(hr);
		m_pCommandList->SetName(L"CommandList");

		// Command lists are created in the recording state, but there is nothing
		// to record yet. The main loop expects it to be closed, so close it now.
		m_pCommandList->Close();
	}

	// create fence
	{
		hr = m_pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_pFence));
		BREAK_IF_FAILED(hr);

		m_FenceValue = 0;

		m_hFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	}

	// create float buffer and prev buffer
	{
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

		D3D12_HEAP_PROPERTIES heapProps = {};
		heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
		heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProps.CreationNodeMask = 1;
		heapProps.VisibleNodeMask = 1;

		hr = m_pDevice->CreateCommittedResource(&heapProps,
												D3D12_HEAP_FLAG_NONE,
												&resourceDesc,
												D3D12_RESOURCE_STATE_COMMON,
												nullptr,
												IID_PPV_ARGS(&m_pFloatBuffer));
		BREAK_IF_FAILED(hr);
		m_pFloatBuffer->SetName(L"FloatBuffer");


		resourceDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		hr = m_pDevice->CreateCommittedResource(&heapProps,
												D3D12_HEAP_FLAG_NONE,
												&resourceDesc,
												D3D12_RESOURCE_STATE_COMMON,
												nullptr,
												IID_PPV_ARGS(&m_pPrevBuffer));
		BREAK_IF_FAILED(hr);
		m_pPrevBuffer->SetName(L"PrevBuffer");
	}

	m_pResourceManager = new ResourceManager;
	m_pResourceManager->Initialize(m_pDevice, m_pCommandQueue, m_pCommandAllocator, m_pCommandList, &m_DynamicDescriptorPool);
	m_pResourceManager->InitRTVDescriptorHeap(16);
	m_pResourceManager->InitDSVDescriptorHeap(8);
	m_pResourceManager->InitCBVSRVUAVDescriptorHeap(512);

	SAFE_RELEASE(pDebugController);
	SAFE_RELEASE(pFactory5);
	SAFE_RELEASE(pAdapter3);
}

void Renderer::initScene()
{
	m_Camera.Reset(Vector3(3.74966f, 5.03645f, -2.54918f), -0.819048f, 0.741502f);

	m_GlobalConstant.Initialize(m_pResourceManager, sizeof(GlobalConstant));
	m_LightConstant.Initialize(m_pResourceManager, sizeof(LightConstant));
	m_ReflectionGlobalConstant.Initialize(m_pResourceManager, sizeof(GlobalConstant));
	m_pResourceManager->SetGlobalConstants(&m_GlobalConstant, &m_LightConstant, &m_ReflectionGlobalConstant);

	// 환경맵 텍스쳐 로드.
	{
		m_EnvTexture.InitializeWithDDS(m_pResourceManager, L"./Assets/Textures/Cubemaps/HDRI/clear_pureskyEnvHDR.dds");
		m_IrradianceTexture.InitializeWithDDS(m_pResourceManager, L"./Assets/Textures/Cubemaps/HDRI/clear_pureskyEnvHDR.dds");
		m_SpecularTexture.InitializeWithDDS(m_pResourceManager, L"./Assets/Textures/Cubemaps/HDRI/clear_pureskyEnvHDR.dds");
		m_BRDFTexture.InitializeWithDDS(m_pResourceManager, L"./Assets/Textures/Cubemaps/HDRI/clear_pureskyEnvHDR.dds");
	}

	// 조명 설정.
	{
		m_Lights.resize(MAX_LIGHTS);

		// 조명 0.
		m_Lights[0].Property.Radiance = Vector3(3.0f);
		m_Lights[0].Property.FallOffEnd = 10.0f;
		m_Lights[0].Property.Position = Vector3(0.0f, 0.0f, 0.0f);
		m_Lights[0].Property.Direction = Vector3(0.0f, 0.0f, 1.0f);
		m_Lights[0].Property.SpotPower = 3.0f;
		m_Lights[0].Property.LightType = LIGHT_POINT | LIGHT_SHADOW;
		m_Lights[0].Property.Radius = 0.03f;
		m_Lights[0].Initialize(m_pResourceManager);

		// 조명 1.
		m_Lights[1].Property.Radiance = Vector3(3.0f);
		m_Lights[1].Property.FallOffEnd = 10.0f;
		m_Lights[1].Property.Position = Vector3(1.0f, 1.1f, 2.0f);
		m_Lights[1].Property.SpotPower = 2.0f;
		m_Lights[1].Property.Direction = Vector3(0.0f, -0.5f, 1.7f) - m_Lights[1].Property.Position;
		m_Lights[1].Property.Direction.Normalize();
		m_Lights[1].Property.LightType = LIGHT_SPOT | LIGHT_SHADOW;
		m_Lights[1].Property.Radius = 0.03f;
		m_Lights[1].Initialize(m_pResourceManager);

		// 조명 2.
		m_Lights[2].Property.Radiance = Vector3(5.0f);
		m_Lights[2].Property.Position = Vector3(5.0f, 5.0f, 5.0f);
		m_Lights[2].Property.Direction = Vector3(-1.0f, -1.0f, -1.0f);
		m_Lights[2].Property.Direction.Normalize();
		m_Lights[2].Property.LightType = LIGHT_DIRECTIONAL | LIGHT_SHADOW;
		m_Lights[2].Property.Radius = 0.05f;
		m_Lights[2].Initialize(m_pResourceManager);
	}

	// 조명 위치 표시.
	{
		m_LightSpheres.resize(MAX_LIGHTS);

		for (int i = 0; i < MAX_LIGHTS; ++i)
		{
			MeshInfo sphere = INIT_MESH_INFO;
			MakeSphere(&sphere, 1.0f, 20, 20);

			m_LightSpheres[i] = new Model(m_pResourceManager, { sphere });
			m_LightSpheres[i]->UpdateWorld(Matrix::CreateTranslation(m_Lights[i].Property.Position));

			MaterialConstant* pSphereMaterialConst = (MaterialConstant*)m_LightSpheres[i]->Meshes[0]->MaterialConstant.pData;
			pSphereMaterialConst->AlbedoFactor = Vector3(0.0f);
			pSphereMaterialConst->EmissionFactor = Vector3(1.0f, 1.0f, 0.0f);
			m_LightSpheres[i]->bCastShadow = false; // 조명 표시 물체들은 그림자 X.
			for (UINT64 j = 0, size = m_LightSpheres[i]->Meshes.size(); j < size; ++j)
			{
				Mesh* pCurMesh = m_LightSpheres[i]->Meshes[j];
				MaterialConstant* pMeshMaterialConst = (MaterialConstant*)(pCurMesh->MaterialConstant.pData);
				pMeshMaterialConst->AlbedoFactor = Vector3(0.0f);
				pMeshMaterialConst->EmissionFactor = Vector3(1.0f, 1.0f, 0.0f);
			}

			m_LightSpheres[i]->bIsVisible = true;
			m_LightSpheres[i]->Name = "LightSphere" + std::to_string(i);
			m_LightSpheres[i]->bIsPickable = false;

			m_RenderObjects.push_back(m_LightSpheres[i]); // 리스트에 등록.
		}
	}


	// 공용 global constant 설정.
	{
		GlobalConstant* pGlobalData = (GlobalConstant*)m_GlobalConstant.pData;
		pGlobalData->StrengthIBL = 0.3f;

		LightConstant* pLightData = (LightConstant*)m_LightConstant.pData;
		for (int i = 0; i < MAX_LIGHTS; ++i)
		{
			memcpy(&(pLightData->Lights[i]), &(m_Lights[i].Property), sizeof(LightProperty));
		}
	}

	// 바닥(거울).
	{
		MeshInfo mesh = INIT_MESH_INFO;
		MakeSquare(&mesh, 10.0f);

		std::wstring path = L"./Assets/Textures/PBR/stringy-marble-ue/";
		mesh.szAlbedoTextureFileName = path + L"stringy_marble_albedo.png";
		mesh.szEmissiveTextureFileName = L"";
		mesh.szAOTextureFileName = path + L"stringy_marble_ao.png";
		mesh.szMetallicTextureFileName = path + L"stringy_marble_Metallic.png";
		mesh.szNormalTextureFileName = path + L"stringy_marble_Normal-dx.png";
		mesh.szRoughnessTextureFileName = path + L"stringy_marble_Roughness.png";

		m_pGround = new Model(m_pResourceManager, { mesh });

		MaterialConstant* pGroundMaterialConst = (MaterialConstant*)m_pGround->Meshes[0]->MaterialConstant.pData;
		pGroundMaterialConst->AlbedoFactor = Vector3(0.7f);
		pGroundMaterialConst->EmissionFactor = Vector3(0.0f);
		pGroundMaterialConst->MetallicFactor = 0.5f;
		pGroundMaterialConst->RoughnessFactor = 0.3f;

		// Vector3 position = Vector3(0.0f, -1.0f, 0.0f);
		Vector3 position = Vector3(0.0f, -0.5f, 0.0f);
		m_pGround->UpdateWorld(Matrix::CreateRotationX(DirectX::XM_PI * 0.5f) * Matrix::CreateTranslation(position));
		m_pGround->bCastShadow = false; // 바닥은 그림자 만들기 생략.

		m_MirrorPlane = DirectX::SimpleMath::Plane(position, Vector3(0.0f, 1.0f, 0.0f));
		m_pMirror = m_pGround; // 바닥에 거울처럼 반사 구현.
	}

	// 환경 박스 초기화.
	{
		MeshInfo skyboxMeshInfo = INIT_MESH_INFO;
		MakeBox(&skyboxMeshInfo, 40.0f);

		std::reverse(skyboxMeshInfo.Indices.begin(), skyboxMeshInfo.Indices.end());
		m_pSkybox = new Model(m_pResourceManager, { skyboxMeshInfo });
		m_pSkybox->Name = "SkyBox";
	}

	// Main Object.
	{
		std::wstring path = L"./Assets/";
		std::vector<std::wstring> clipNames =
		{
			L"CatwalkIdleTwistR.fbx", L"CatwalkIdleToWalkForward.fbx",
			L"CatwalkWalkForward.fbx", L"CatwalkWalkStop.fbx",
		};
		AnimationData aniData;

		std::wstring filename = L"Remy.fbx";
		std::vector<MeshInfo> characterMeshInfo;
		AnimationData characterDefaultAnimData;
		ReadAnimationFromFile(characterMeshInfo, characterDefaultAnimData, path, filename);

		for (UINT64 i = 0, size = clipNames.size(); i < size; ++i)
		{
			std::wstring& name = clipNames[i];
			std::vector<MeshInfo> animationMeshInfo;
			AnimationData animationData;
			ReadAnimationFromFile(animationMeshInfo, animationData, path, name);

			if (aniData.Clips.empty())
			{
				aniData = animationData;
			}
			else
			{
				aniData.Clips.push_back(animationData.Clips[0]);
			}
		}

		Vector3 center(0.0f, 0.0f, 2.0f);
		m_pCharacter = new SkinnedMeshModel(m_pResourceManager, characterMeshInfo, aniData);
		for (UINT64 i = 0, size = m_pCharacter->Meshes.size(); i < size; ++i)
		{
			Mesh* pCurMesh = m_pCharacter->Meshes[i];
			MaterialConstant* pMeshConst = (MaterialConstant*)pCurMesh->MaterialConstant.pData;

			pMeshConst->AlbedoFactor = Vector3(1.0f);
			pMeshConst->RoughnessFactor = 0.8f;
			pMeshConst->MetallicFactor = 0.0f;
		}
		m_pCharacter->UpdateWorld(Matrix::CreateScale(1.0f) * Matrix::CreateTranslation(center));

		// m_RenderObjects.push_back(m_pCharacter);
	}
}

void Renderer::initDescriptorHeap()
{
	HRESULT hr = S_OK;

	const UINT RTV_DESCRITOR_SIZE = m_pResourceManager->m_RTVDescriptorSize;
	const UINT CBV_SRV_UAV_DESCRIPTOR_SIZE = m_pResourceManager->m_CBVSRVUAVDescriptorSize;

	// render target.
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_pResourceManager->m_pRTVHeap->GetCPUDescriptorHandleForHeapStart(), m_pResourceManager->m_RTVHeapSize, RTV_DESCRITOR_SIZE);

		m_MainRenderTargetOffset = m_pResourceManager->m_RTVHeapSize;

		for (UINT i = 0; i < SWAP_CHAIN_FRAME_COUNT; ++i)
		{
			m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&m_pRenderTargets[i]));
			m_pRenderTargets[i]->SetName(L"RenderTarget");
			m_pDevice->CreateRenderTargetView(m_pRenderTargets[i], nullptr, rtvHandle);
			rtvHandle.Offset(1, RTV_DESCRITOR_SIZE);
		}

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = 0;
		rtvDesc.Texture2D.PlaneSlice = 0;
		m_pDevice->CreateRenderTargetView(m_pFloatBuffer, &rtvDesc, rtvHandle);
		m_FloatBufferRTVOffset = 2;

		// 2 backbuffer + 1 floatbuffer.
		m_pResourceManager->m_RTVHeapSize += SWAP_CHAIN_FRAME_COUNT + 1;
	}

	// depth stencil.
	{
		D3D12_RESOURCE_DESC dsvDesc;
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

		D3D12_HEAP_PROPERTIES heapProps;
		heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
		heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProps.CreationNodeMask = 1;
		heapProps.VisibleNodeMask = 1;

		D3D12_CLEAR_VALUE clearValue;
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

		D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc = {};
		depthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthStencilViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		depthStencilViewDesc.Texture2D.MipSlice = 0;

		CD3DX12_CPU_DESCRIPTOR_HANDLE depthHandle(m_pResourceManager->m_pDSVHeap->GetCPUDescriptorHandleForHeapStart());
		m_pDevice->CreateDepthStencilView(m_pDefaultDepthStencil, &depthStencilViewDesc, depthHandle);

		++(m_pResourceManager->m_DSVHeapSize);
	}

	// Model에서 쓰이는 descriptor 저장.
	{
		UINT64 totalObject = m_RenderObjects.size();
		CD3DX12_CPU_DESCRIPTOR_HANDLE cbvSrvHandle(m_pResourceManager->m_pCBVSRVUAVHeap->GetCPUDescriptorHandleForHeapStart());

		// b0, b1
		m_pResourceManager->m_GlobalConstantViewStartOffset = m_pResourceManager->m_CBVSRVUAVHeapSize;

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
		cbvDesc.BufferLocation = m_GlobalConstant.GetGPUMemAddr();
		cbvDesc.SizeInBytes = (UINT)m_GlobalConstant.GetBufferSize();
		m_pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvHandle);
		m_GlobalConstant.SetCBVHandle(cbvSrvHandle);
		cbvSrvHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(m_pResourceManager->m_CBVSRVUAVHeapSize);

		cbvDesc.BufferLocation = m_ReflectionGlobalConstant.GetGPUMemAddr();
		cbvDesc.SizeInBytes = (UINT)m_ReflectionGlobalConstant.GetBufferSize();
		m_pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvHandle);
		m_ReflectionGlobalConstant.SetCBVHandle(cbvSrvHandle);
		cbvSrvHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(m_pResourceManager->m_CBVSRVUAVHeapSize);

		cbvDesc.BufferLocation = m_LightConstant.GetGPUMemAddr();
		cbvDesc.SizeInBytes = (UINT)m_LightConstant.GetBufferSize();
		m_pDevice->CreateConstantBufferView(&cbvDesc, cbvSrvHandle);
		m_LightConstant.SetCBVHandle(cbvSrvHandle);
		cbvSrvHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(m_pResourceManager->m_CBVSRVUAVHeapSize);


		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.PlaneSlice = 0;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		// float buffer srv
		srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		m_pDevice->CreateShaderResourceView(m_pFloatBuffer, &srvDesc, cbvSrvHandle);
		m_FloatBufferSRVOffset = m_pResourceManager->m_CBVSRVUAVHeapSize;
		cbvSrvHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(m_pResourceManager->m_CBVSRVUAVHeapSize);

		// prev buffer srv
		srvDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
		m_pDevice->CreateShaderResourceView(m_pPrevBuffer, &srvDesc, cbvSrvHandle);
		m_PrevBufferSRVOffset = m_pResourceManager->m_CBVSRVUAVHeapSize;
		cbvSrvHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(m_pResourceManager->m_CBVSRVUAVHeapSize);

		// t8, t9, t10
		m_pResourceManager->m_GlobalShaderResourceViewStartOffset = m_pResourceManager->m_CBVSRVUAVHeapSize;

		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		m_Lights[1].ShadowMap.SetDescriptorHeap(m_pResourceManager);
		cbvSrvHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);

		m_pDevice->CreateShaderResourceView(nullptr, &srvDesc, cbvSrvHandle);
		cbvSrvHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(m_pResourceManager->m_CBVSRVUAVHeapSize);

		m_pDevice->CreateShaderResourceView(nullptr, &srvDesc, cbvSrvHandle);
		cbvSrvHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(m_pResourceManager->m_CBVSRVUAVHeapSize);

		// t11
		m_Lights[0].ShadowMap.SetDescriptorHeap(m_pResourceManager);
		cbvSrvHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);

		// t12
		m_Lights[2].ShadowMap.SetDescriptorHeap(m_pResourceManager);
		cbvSrvHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);

		// t13
		D3D12_RESOURCE_DESC descInfo;
		descInfo = m_EnvTexture.GetResource()->GetDesc();
		srvDesc.Format = descInfo.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.MipLevels = 1;
		srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
		m_pDevice->CreateShaderResourceView(m_EnvTexture.GetResource(), &srvDesc, cbvSrvHandle);
		m_EnvTexture.SetSRVHandle(cbvSrvHandle);
		cbvSrvHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(m_pResourceManager->m_CBVSRVUAVHeapSize);

		// t14
		descInfo = m_IrradianceTexture.GetResource()->GetDesc();
		srvDesc.Format = descInfo.Format;
		m_pDevice->CreateShaderResourceView(m_IrradianceTexture.GetResource(), &srvDesc, cbvSrvHandle);
		m_IrradianceTexture.SetSRVHandle(cbvSrvHandle);
		cbvSrvHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(m_pResourceManager->m_CBVSRVUAVHeapSize);

		// t15
		descInfo = m_SpecularTexture.GetResource()->GetDesc();
		srvDesc.Format = descInfo.Format;
		m_pDevice->CreateShaderResourceView(m_SpecularTexture.GetResource(), &srvDesc, cbvSrvHandle);
		m_SpecularTexture.SetSRVHandle(cbvSrvHandle);
		cbvSrvHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(m_pResourceManager->m_CBVSRVUAVHeapSize);

		// t16
		descInfo = m_BRDFTexture.GetResource()->GetDesc();
		srvDesc.Format = descInfo.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.PlaneSlice = 0;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		m_pDevice->CreateShaderResourceView(m_BRDFTexture.GetResource(), &srvDesc, cbvSrvHandle);
		m_BRDFTexture.SetSRVHandle(cbvSrvHandle);
		cbvSrvHandle.Offset(1, CBV_SRV_UAV_DESCRIPTOR_SIZE);
		++(m_pResourceManager->m_CBVSRVUAVHeapSize);

		// Model 내 생성된 버퍼들 등록.
		for (UINT64 i = 0; i < totalObject; ++i)
		{
			Model* pModel = m_RenderObjects[i];
			pModel->SetDescriptorHeap(m_pResourceManager);
		}
		m_pSkybox->SetDescriptorHeap(m_pResourceManager);
		m_pGround->SetDescriptorHeap(m_pResourceManager);
		m_pCharacter->SetDescriptorHeap(m_pResourceManager);
	}
}

void Renderer::initThreadAndEvent()
{
	struct threadwrapper
	{
		static UINT WINAPI thunk(LPVOID lpParameter)
		{
			ThreadParameter* parameter = (ThreadParameter*)lpParameter;
			Renderer::Get()->ThreadWork(parameter);
			return 0;
		}
		static UINT WINAPI thunk2(LPVOID lpParameter)
		{
			ThreadParameter* parameter = (ThreadParameter*)lpParameter;
			Renderer::Get()->ThreadWork2(parameter);
			return 0;
		}
	};

	for (int i = 0; i < LIGHT_THREADS; ++i)
	{
		m_hBeginShadowPass[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
		m_hFinishShadowPass[i] = CreateEvent(NULL, FALSE, FALSE, NULL);

		m_LightParameters[i] = { i };
		m_hLightHandles[i] = reinterpret_cast<HANDLE>(_beginthreadex(nullptr,
																	 0,
																	 threadwrapper::thunk2,
																	 reinterpret_cast<LPVOID>(&m_LightParameters[i]),
																	 0,
																	 nullptr));

		_ASSERT(m_hBeginShadowPass[i]);
		_ASSERT(m_hFinishShadowPass[i]);
		_ASSERT(m_hLightHandles[i]);
	}
	for (int i = 0; i < NUM_THREADS; ++i)
	{
		m_hBeginRenderPass[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
		m_hFinishMainRenderPass[i] = CreateEvent(NULL, FALSE, FALSE, NULL);

		m_ThreadParameters[i] = { i };
		m_hThreadHandles[i] = reinterpret_cast<HANDLE>(_beginthreadex(nullptr,
																	  0,
																	  threadwrapper::thunk,
																	  reinterpret_cast<LPVOID>(&m_ThreadParameters[i]),
																	  0,
																	  nullptr));

		_ASSERT(m_hBeginRenderPass[i]);
		_ASSERT(m_hFinishMainRenderPass[i]);
		_ASSERT(m_hThreadHandles[i]);
	}
}

void Renderer::beginRender()
{
	HRESULT hr = S_OK;

	hr = m_pCommandAllocator->Reset();
	BREAK_IF_FAILED(hr);

	hr = m_pCommandList->Reset(m_pCommandAllocator, nullptr);
	BREAK_IF_FAILED(hr);

	ID3D12DescriptorHeap* ppDescriptorHeaps[] =
	{
		m_DynamicDescriptorPool.GetDescriptorHeap(),
		m_pResourceManager->m_pSamplerHeap
	};
	m_pCommandList->SetDescriptorHeaps(2, ppDescriptorHeaps);
}

void Renderer::shadowMapRender()
{
	for (UINT64 i = 0, size = m_Lights.size(); i < size; ++i)
	{
		m_Lights[i].RenderShadowMap(m_pResourceManager, m_RenderObjects, (SkinnedMeshModel*)m_pCharacter, m_pMirror);
	}
}

void Renderer::objectRender()
{
	const UINT RTV_DESCRIPTOR_SIZE = m_pResourceManager->m_RTVDescriptorSize;
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_pResourceManager->m_pRTVHeap->GetCPUDescriptorHandleForHeapStart(), m_MainRenderTargetOffset + m_FrameIndex, RTV_DESCRIPTOR_SIZE);
	CD3DX12_CPU_DESCRIPTOR_HANDLE floatRtvHandle(m_pResourceManager->m_pRTVHeap->GetCPUDescriptorHandleForHeapStart(), m_FloatBufferRTVOffset, RTV_DESCRIPTOR_SIZE);
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_pResourceManager->m_pDSVHeap->GetCPUDescriptorHandleForHeapStart());

	const CD3DX12_RESOURCE_BARRIER RTV_BEFORE_BARRIERS[2] =
	{
		CD3DX12_RESOURCE_BARRIER::Transition(m_pRenderTargets[m_FrameIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET),
		CD3DX12_RESOURCE_BARRIER::Transition(m_pFloatBuffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET),
	};
	m_pCommandList->ResourceBarrier(2, RTV_BEFORE_BARRIERS);

	const float COLOR[] = { 0.0f, 0.0f, 1.0f, 1.0f };
	m_pCommandList->ClearRenderTargetView(rtvHandle, COLOR, 0, nullptr);
	m_pCommandList->ClearRenderTargetView(floatRtvHandle, COLOR, 0, nullptr);
	m_pCommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	m_pCommandList->RSSetViewports(1, &m_ScreenViewport);
	m_pCommandList->RSSetScissorRects(1, &m_ScissorRect);
	m_pCommandList->OMSetRenderTargets(1, &floatRtvHandle, FALSE, &dsvHandle);

	m_pResourceManager->SetCommonState(Skybox);
	m_pSkybox->Render(m_pResourceManager, Skybox);

	m_pResourceManager->SetCommonState(Default);
	for (UINT64 i = 0, size = m_RenderObjects.size(); i < size; ++i)
	{
		Model* pCurModel = m_RenderObjects[i];

		if (pCurModel->bIsVisible)
		{
			pCurModel->Render(m_pResourceManager, Default);
		}
	}

	m_pResourceManager->SetCommonState(Skinned);
	m_pCharacter->Render(m_pResourceManager, Skinned);
}

void Renderer::mirrorRender()
{
	if (!m_pMirror)
	{
		return;
	}

	// 0.5의 투명도를 가진다고 가정.
	D3D12_CPU_DESCRIPTOR_HANDLE defaultDSVHandle = m_pResourceManager->m_pDSVHeap->GetCPUDescriptorHandleForHeapStart();

	// 거울 위치만 StencilBuffer에 1로 표기.
	m_pResourceManager->SetCommonState(StencilMask);
	m_pMirror->Render(m_pResourceManager, StencilMask);

	// 거울 위치에 반사된 물체들을 렌더링.
	m_pResourceManager->SetCommonState(ReflectionDefault);
	m_pCommandList->ClearDepthStencilView(defaultDSVHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	for (UINT64 i = 0, size = m_RenderObjects.size(); i < size; ++i)
	{
		Model* pCurModel = m_RenderObjects[i];
		pCurModel->Render(m_pResourceManager, ReflectionDefault);
	}
	m_pResourceManager->SetCommonState(ReflectionSkinned);
	m_pCharacter->Render(m_pResourceManager, ReflectionSkinned);

	m_pResourceManager->SetCommonState(ReflectionSkybox);
	m_pSkybox->Render(m_pResourceManager, ReflectionSkybox);

	// 거울 렌더링.
	m_pResourceManager->SetCommonState(MirrorBlend);
	m_pMirror->Render(m_pResourceManager, MirrorBlend);
}

void Renderer::postProcess()
{
	const CD3DX12_RESOURCE_BARRIER BARRIER = CD3DX12_RESOURCE_BARRIER::Transition(m_pFloatBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON);
	m_pCommandList->ResourceBarrier(1, &BARRIER);
	m_PostProcessor.Render(m_pResourceManager, m_FrameIndex);
}

void Renderer::endRender()
{
	_ASSERT(m_pCommandList);
	_ASSERT(m_pCommandQueue);

	const CD3DX12_RESOURCE_BARRIER RTV_AFTER_BARRIER = CD3DX12_RESOURCE_BARRIER::Transition(m_pRenderTargets[m_FrameIndex], D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT);
	m_pCommandList->ResourceBarrier(1, &RTV_AFTER_BARRIER);
	m_pCommandList->Close();

	ID3D12CommandList* ppCommandLists[] = { m_pCommandList };
	m_pCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
}

void Renderer::present()
{
	UINT syncInterval = 1;	  // VSync On
	// UINT syncInterval = 0;  // VSync Off
	UINT presentFlags = 0;

	if (!syncInterval)
	{
		presentFlags = DXGI_PRESENT_ALLOW_TEARING;
	}

	HRESULT hr = m_pSwapChain->Present(syncInterval, presentFlags);
	if (hr == DXGI_ERROR_DEVICE_REMOVED)
	{
		__debugbreak();
	}

	// for next frame
	m_FrameIndex = m_pSwapChain->GetCurrentBackBufferIndex();

	fence();
	waitForFenceValue();

	m_DynamicDescriptorPool.Reset();
}

void Renderer::preRender()
{
	m_pResourceManager->ResetCommandLists();
}

void Renderer::midRender()
{
	ID3D12GraphicsCommandList* pCommandList = m_pResourceManager->m_pCommandLists[Mid];

	const UINT RTV_DESCRIPTOR_SIZE = m_pResourceManager->m_RTVDescriptorSize;
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_pResourceManager->m_pRTVHeap->GetCPUDescriptorHandleForHeapStart(), m_MainRenderTargetOffset + m_FrameIndex, RTV_DESCRIPTOR_SIZE);
	CD3DX12_CPU_DESCRIPTOR_HANDLE floatRtvHandle(m_pResourceManager->m_pRTVHeap->GetCPUDescriptorHandleForHeapStart(), m_FloatBufferRTVOffset, RTV_DESCRIPTOR_SIZE);
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_pResourceManager->m_pDSVHeap->GetCPUDescriptorHandleForHeapStart());

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

	pCommandList->Close();
}

void Renderer::postRender()
{
	ID3D12GraphicsCommandList* pCommandList = m_pResourceManager->m_pCommandLists[Post];
	
	// post processing
	const CD3DX12_RESOURCE_BARRIER BARRIER = CD3DX12_RESOURCE_BARRIER::Transition(m_pFloatBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON);
	pCommandList->ResourceBarrier(1, &BARRIER);
	
	ID3D12DescriptorHeap* ppDescriptorHeaps[] =
	{
		m_DynamicDescriptorPool.GetDescriptorHeap(),
		m_pResourceManager->m_pSamplerHeap
	};
	pCommandList->SetDescriptorHeaps(2, ppDescriptorHeaps);
	
	m_PostProcessor.Render(m_pResourceManager, pCommandList, m_FrameIndex);

	// end render
	const CD3DX12_RESOURCE_BARRIER RTV_AFTER_BARRIER = CD3DX12_RESOURCE_BARRIER::Transition(m_pRenderTargets[m_FrameIndex], D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT);
	pCommandList->ResourceBarrier(1, &RTV_AFTER_BARRIER);
	pCommandList->Close();
}

void Renderer::updateGlobalConstants(const float DELTA_TIME)
{
	const Vector3 EYE_WORLD = m_Camera.GetEyePos();
	const Matrix REFLECTION = Matrix::CreateReflection(m_MirrorPlane);
	const Matrix VIEW = m_Camera.GetView();
	const Matrix PROJECTION = m_Camera.GetProjection();

	GlobalConstant* pGlobalData = (GlobalConstant*)m_GlobalConstant.pData;
	GlobalConstant* pReflectGlobalData = (GlobalConstant*)m_ReflectionGlobalConstant.pData;

	pGlobalData->GlobalTime += DELTA_TIME;
	pGlobalData->EyeWorld = EYE_WORLD;
	pGlobalData->View = VIEW.Transpose();
	pGlobalData->Projection = PROJECTION.Transpose();
	pGlobalData->InverseProjection = PROJECTION.Invert().Transpose();
	pGlobalData->ViewProjection = (VIEW * PROJECTION).Transpose();
	pGlobalData->InverseView = VIEW.Invert().Transpose();
	pGlobalData->InverseViewProjection = pGlobalData->ViewProjection.Invert();

	memcpy(pReflectGlobalData, pGlobalData, sizeof(GlobalConstant));
	pReflectGlobalData->View = (REFLECTION * VIEW).Transpose();
	pReflectGlobalData->ViewProjection = (REFLECTION * VIEW * PROJECTION).Transpose();
	pReflectGlobalData->InverseViewProjection = pReflectGlobalData->ViewProjection.Invert();

	m_GlobalConstant.Upload();
	m_ReflectionGlobalConstant.Upload();
}

void Renderer::updateLightConstants(const float DELTA_TIME)
{
	LightConstant* pLightConstData = (LightConstant*)m_LightConstant.pData;

	for (int i = 0; i < MAX_LIGHTS; ++i)
	{
		m_Lights[i].Update(m_pResourceManager, DELTA_TIME, m_Camera);
		m_LightSpheres[i]->UpdateWorld(Matrix::CreateScale(Max(0.01f, m_Lights[i].Property.Radius)) * Matrix::CreateTranslation(m_Lights[i].Property.Position));
		memcpy(&(pLightConstData->Lights[i]), &(m_Lights[i].Property), sizeof(LightProperty));
	}

	m_LightConstant.Upload();
}

void Renderer::updateAnimation(const float DELTA_TIME)
{
	static int s_FrameCount = 0;

	// States
	// 0: idle
	// 1: idle to walk
	// 2: walk forward
	// 3: walk to stop
	static int s_State = 0;
	SkinnedMeshModel* pCharacter = (SkinnedMeshModel*)m_pCharacter;

	switch (s_State)
	{
		case 0:
		{
			if (m_bKeyPressed[VK_UP])
			{
				s_State = 1;
				s_FrameCount = 0;
			}
			else if (s_FrameCount ==
					 pCharacter->AnimData.Clips[s_State].Keys[0].size() ||
					 m_bKeyPressed[VK_UP]) // 재생이 다 끝난다면.
			{
				s_FrameCount = 0; // 상태 변화 없이 반복.
			}
		}
		break;

		case 1:
		{
			if (s_FrameCount == pCharacter->AnimData.Clips[s_State].Keys[0].size())
			{
				s_State = 2;
				s_FrameCount = 0;
			}
		}
		break;

		case 2:
		{
			if (m_bKeyPressed[VK_RIGHT])
			{
				pCharacter->AnimData.AccumulatedRootTransform =
					Matrix::CreateRotationY(DirectX::XM_PI * 60.0f / 180.0f * DELTA_TIME * 2.0f) *
					pCharacter->AnimData.AccumulatedRootTransform;
			}
			if (m_bKeyPressed[VK_LEFT])
			{
				pCharacter->AnimData.AccumulatedRootTransform =
					Matrix::CreateRotationY(-DirectX::XM_PI * 60.0f / 180.0f * DELTA_TIME * 2.0f) *
					pCharacter->AnimData.AccumulatedRootTransform;
			}
			if (s_FrameCount == pCharacter->AnimData.Clips[s_State].Keys[0].size())
			{
				// 방향키를 누르고 있지 않으면 정지. (누르고 있으면 계속 걷기)
				if (!m_bKeyPressed[VK_UP])
				{
					s_State = 3;
				}
				s_FrameCount = 0;
			}
		}
		break;

		case 3:
		{
			if (s_FrameCount == pCharacter->AnimData.Clips[s_State].Keys[0].size())
			{
				// s_State = 4;
				s_State = 0;
				s_FrameCount = 0;
			}
		}
		break;

		default:
			break;
	}

	pCharacter->UpdateAnimation(s_State, s_FrameCount);
	++s_FrameCount;
}

void Renderer::onMouseMove(const int MOUSE_X, const int MOUSE_Y)
{
	m_MouseX = MOUSE_X;
	m_MouseY = MOUSE_Y;

	// 마우스 커서의 위치를 NDC로 변환.
	// 마우스 커서는 좌측 상단 (0, 0), 우측 하단(width-1, height-1).
	// NDC는 좌측 하단이 (-1, -1), 우측 상단(1, 1).
	m_MouseNDCX = (float)MOUSE_X * 2.0f / (float)m_ScreenWidth - 1.0f;
	m_MouseNDCY = (float)(-MOUSE_Y) * 2.0f / (float)m_ScreenHeight + 1.0f;

	// 커서가 화면 밖으로 나갔을 경우 범위 조절.
	/*m_MouseNDCX = Clamp(m_MouseNDCX, -1.0f, 1.0f);
	m_MouseNDCY = Clamp(m_MouseNDCY, -1.0f, 1.0f);*/

	// 카메라 시점 회전.
	m_Camera.UpdateMouse(m_MouseNDCX, m_MouseNDCY);
}

void Renderer::onMouseClick(const int MOUSE_X, const int MOUSE_Y)
{
	m_MouseX = MOUSE_X;
	m_MouseY = MOUSE_Y;

	m_MouseNDCX = (float)MOUSE_X * 2.0f / (float)m_ScreenWidth - 1.0f;
	m_MouseNDCY = (float)(-MOUSE_Y) * 2.0f / (float)m_ScreenHeight + 1.0f;
}

void Renderer::processMouseControl()
{
	static Model* s_pActiveModel = nullptr;
	static float s_PrevRatio = 0.0f;
	static Vector3 s_PrevPos(0.0f);
	static Vector3 s_PrevVector(0.0f);

	// 적용할 회전과 이동 초기화.
	Quaternion dragRotation = Quaternion::CreateFromAxisAngle(Vector3(1.0f, 0.0f, 0.0f), 0.0f);
	Vector3 dragTranslation(0.0f);
	Vector3 pickPoint(0.0f);
	float dist = 0.0f;

	// 사용자가 두 버튼 중 하나만 누른다고 가정.
	if (m_bMouseLeftButton || m_bMouseRightButton)
	{
		const Matrix VIEW = m_Camera.GetView();
		const Matrix PROJECTION = m_Camera.GetProjection();
		const Vector3 NDC_NEAR = Vector3(m_MouseNDCX, m_MouseNDCY, 0.0f);
		const Vector3 NDC_FAR = Vector3(m_MouseNDCX, m_MouseNDCY, 1.0f);
		const Matrix INV_PROJECTION_VIEW = (VIEW * PROJECTION).Invert();
		const Vector3 WORLD_NEAR = Vector3::Transform(NDC_NEAR, INV_PROJECTION_VIEW);
		const Vector3 WORLD_FAR = Vector3::Transform(NDC_FAR, INV_PROJECTION_VIEW);
		Vector3 dir = WORLD_FAR - WORLD_NEAR;
		dir.Normalize();
		const DirectX::SimpleMath::Ray CUR_RAY = DirectX::SimpleMath::Ray(WORLD_NEAR, dir);

		if (!s_pActiveModel) // 이전 프레임에서 아무 물체도 선택되지 않았을 경우에는 새로 선택.
		{
			Model* pSelectedModel = pickClosest(CUR_RAY, &dist);
			if (pSelectedModel)
			{
				OutputDebugStringA("Newly selected model: ");
				OutputDebugStringA(pSelectedModel->Name.c_str());
				OutputDebugStringA("\n");

				s_pActiveModel = pSelectedModel;
				m_pPickedModel = pSelectedModel; // GUI 조작용 포인터.
				pickPoint = CUR_RAY.position + dist * CUR_RAY.direction;
				if (m_bMouseLeftButton) // 왼쪽 버튼 회전 준비.
				{
					s_PrevVector = pickPoint - s_pActiveModel->BoundingSphere.Center;
					s_PrevVector.Normalize();
				}
				else
				{ // 오른쪽 버튼 이동 준비
					m_bMouseDragStartFlag = false;
					s_PrevRatio = dist / (WORLD_FAR - WORLD_NEAR).Length();
					s_PrevPos = pickPoint;
				}
			}
		}
		else // 이미 선택된 물체가 있었던 경우.
		{
			if (m_bMouseLeftButton) // 왼쪽 버튼으로 계속 회전.
			{
				if (CUR_RAY.Intersects(s_pActiveModel->BoundingSphere, dist))
				{
					pickPoint = CUR_RAY.position + dist * CUR_RAY.direction;
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
			else // 오른쪽 버튼으로 계속 이동.
			{
				Vector3 newPos = WORLD_NEAR + s_PrevRatio * (WORLD_FAR - WORLD_NEAR);
				if ((newPos - s_PrevPos).Length() > 1e-3)
				{
					dragTranslation = newPos - s_PrevPos;
					s_PrevPos = newPos;
				}
				pickPoint = newPos; // Cursor sphere 그려질 위치.
			}
		}
	}
	else
	{
		// 버튼에서 손을 땠을 경우에는 움직일 모델은 nullptr로 설정.
		s_pActiveModel = nullptr;
	}

	//if (s_pActiveModel != nullptr)
	//{
	//	Vector3 translation = s_pActiveModel->World.Translation();
	//	s_pActiveModel->World.Translation(Vector3(0.0f));
	//	s_pActiveModel->UpdateWorld(s_pActiveModel->World * Matrix::CreateFromQuaternion(dragRotation) * Matrix::CreateTranslation(dragTranslation + translation));
	//	s_pActiveModel->BoundingSphere.Center = s_pActiveModel->World.Translation();

	//	// 충돌 지점에 작은 구 그리기.
	//	m_pCursorSphere->bIsVisible = true;
	//	m_pCursorSphere->UpdateWorld(Matrix::CreateTranslation(pickPoint));
	//}
	//else
	//{
	//	m_pCursorSphere->bIsVisible = false;
	//}
}

Model* Renderer::pickClosest(const DirectX::SimpleMath::Ray& PICKING_RAY, float* pMinDist)
{
	return nullptr;
}

UINT64 Renderer::fence()
{
	++m_FenceValue;
	m_pCommandQueue->Signal(m_pFence, m_FenceValue);

	return m_FenceValue;
}

void Renderer::waitForFenceValue()
{
	const UINT64 EXPECTED_FENCE_VALUE = m_FenceValue;

	// Wait until the previous frame is finished.
	if (m_pFence->GetCompletedValue() < EXPECTED_FENCE_VALUE)
	{
		m_pFence->SetEventOnCompletion(EXPECTED_FENCE_VALUE, m_hFenceEvent);
		WaitForSingleObject(m_hFenceEvent, INFINITE);
	}
}
