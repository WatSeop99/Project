#pragma once

#include "../Graphics/Camera.h"
#include "CommandListPool.h"
#include "ConstantDataType.h"
#include "ConstantBufferManager.h"
#include "DescriptorAllocator.h"
#include "DynamicDescriptorPool.h"
#include "../Util/KnM.h"
#include "../Graphics/Light.h"
#include "../Model/Model.h"
#include "RenderThread.h"
#include "ResourceManager.h"
#include "../Model/SkinnedMeshModel.h"
#include "../Physics/PhysicsManager.h"
#include "../Graphics/PostProcessor.h"

class Renderer
{
public:
	struct InitialData
	{
		std::vector<Model*>* pRenderObjects;
		std::vector<Light>* pLights;
		std::vector<Model*>* pLightSpheres;

		TextureHandle* pEnvTextureHandle;
		TextureHandle* pIrradianceTextureHandle;
		TextureHandle* pSpecularTextureHandle;
		TextureHandle* pBRDFTextureHandle;

		Model* pMirror;
		DirectX::SimpleMath::Plane* pMirrorPlane;
	};

public:
	Renderer();
	virtual ~Renderer();

	void Initizlie();

	void Update(const float DELTA_TIME);

	void Render();
	void ProcessByThread(UINT threadIndex, ResourceManager* pManager, int renderPass);

	UINT64 Fence();
	void WaitForFenceValue(UINT64 expectedFenceValue);

	void Cleanup();

	LRESULT MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	inline ID3D12Device5* GetD3DDevice() { return m_pDevice; }
	inline ID3D12CommandQueue* GetCommandQueue() { return m_pCommandQueue; }
	inline ID3D12GraphicsCommandList* GetCommandList() { return m_pppCommandListPool[m_FrameIndex][0]->GetCurrentCommandList(); }

	inline ResourceManager* GetResourceManager() { return m_pResourceManager; }
	inline PhysicsManager* GetPhysicsManager() { return m_pPhysicsManager; }
	inline DescriptorAllocator* GetRTVAllocator() { return m_pRTVAllocator; }
	inline DescriptorAllocator* GetDSVAllocator() { return m_pDSVAllocator; }
	inline DescriptorAllocator* GetSRVUAVAllocator() { return m_pSRVUAVAllocator; }
	inline TextureManager* GetTextureManager() { return m_pTextureManager; }
	ConstantBufferManager* GetConstantBufferPool(UINT threadIndex = 0);
	ConstantBufferManager* GetConstantBufferManager(UINT threadIndex = 0);
	DynamicDescriptorPool* GetDynamicDescriptorPool(UINT threadIndex = 0);

	inline HWND GetWindow() { return m_hMainWindow; }

	void SetExternalDatas(InitialData* pInitialData);

protected:
	void initMainWidndow();
	void initDirect3D();
	void initPhysics();
	void initScene();
	void initRenderThreadPool(UINT renderThreadCount);
	void initRenderTargets();
	void initDepthStencils();
	void initShaderResources();

	void cleanRenderTargets();
	void cleanDepthStencils();
	void cleanShaderResources();

	void beginRender();
	void renderShadowmap();
	void renderObject();
	void renderMirror();
	void renderObjectBoundingModel();
	void postProcess();
	void endRender();
	void present();

	void updateGlobalConstants(const float DELTA_TIME);
	void updateLightConstants(const float DELTA_TIME);

	void onMouseMove(const int MOUSE_X, const int MOUSE_Y);
	void onMouseClick(const int MOUSE_X, const int MOUSE_Y);
	void processMouseControl(const float DELTA_TIME);
	Model* pickClosest(const DirectX::SimpleMath::Ray& PICKING_RAY, float* pMinDist, Mesh** ppEndEffector, int* pEndEffectorType);

protected:
	HWND m_hMainWindow = nullptr;

	HANDLE m_hFenceEvent = nullptr;
	ID3D12Fence* m_pFence = nullptr;
	UINT64 m_LastFenceValues[SWAP_CHAIN_FRAME_COUNT] = { 0, };
	UINT64 m_FenceValue = 0;

	PhysicsManager* m_pPhysicsManager = nullptr;
	PostProcessor* m_pPostProcessor = nullptr;

	Keyboard m_Keyboard;
	Mouse m_Mouse;

	// external data
	GlobalConstant m_GlobalConstantData;
	LightConstant m_LightConstantData;
	GlobalConstant m_ReflectionGlobalConstantData;

	TextureHandle* m_pEnvTextureHandle = nullptr;
	TextureHandle* m_pIrradianceTextureHandle = nullptr;
	TextureHandle* m_pSpecularTextureHandle = nullptr;
	TextureHandle* m_pBRDFTextureHandle = nullptr;

	std::vector<Model*>* m_pRenderObjects = nullptr;
	std::vector<Light>* m_pLights = nullptr;
	std::vector<Model*>* m_pLightSpheres = nullptr;

	Model* m_pMirror = nullptr;
	Model* m_pPickedModel = nullptr;
	Mesh* m_pPickedEndEffector = nullptr;
	Vector3 m_PickedTranslation;
	int m_PickedEndEffectorType = -1;
	DirectX::SimpleMath::Plane* m_pMirrorPlane = nullptr;

private:
	D3D_FEATURE_LEVEL m_FeatureLevel;
	DXGI_FORMAT m_BackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	D3D12_VIEWPORT m_ScreenViewport = { 0, };
	D3D12_RECT m_ScissorRect = { 0, };
	DXGI_ADAPTER_DESC2 m_AdapterDesc = { 0, };
	UINT m_ScreenWidth = 1280;
	UINT m_ScreenHeight = 720;

	ID3D12Device5* m_pDevice = nullptr;
	IDXGISwapChain4* m_pSwapChain = nullptr;
	ID3D12CommandQueue* m_pCommandQueue = nullptr;

	// for multi-thread ////////////////////////
	RenderQueue* m_pppRenderQueue[RenderPass_RenderPassCount][MAX_RENDER_THREAD_COUNT] = { nullptr, };
	CommandListPool* m_pppCommandListPool[SWAP_CHAIN_FRAME_COUNT][MAX_RENDER_THREAD_COUNT] = { nullptr, };
	DynamicDescriptorPool* m_pppDescriptorPool[SWAP_CHAIN_FRAME_COUNT][MAX_RENDER_THREAD_COUNT] = { nullptr, };
	ConstantBufferManager* m_pppConstantBufferManager[SWAP_CHAIN_FRAME_COUNT][MAX_RENDER_THREAD_COUNT] = { nullptr, };
	RenderThreadDesc* m_pThreadDescList = nullptr;
	UINT m_RenderThreadCount = 0;
	UINT m_CurThreadIndex = 0; // render queue�� �յ��ϰ� �ֱ� ���� �ε���.

	long volatile m_pActiveThreadCounts[RenderPass_RenderPassCount] = { 0, };
	HANDLE m_phCompletedEvents[RenderPass_RenderPassCount] = { nullptr, };
	/////////////////////////////////////////////

	// main resources.
	ResourceManager* m_pResourceManager = nullptr;
	TextureManager* m_pTextureManager = nullptr;
	DescriptorAllocator* m_pRTVAllocator = nullptr;
	DescriptorAllocator* m_pDSVAllocator = nullptr;
	DescriptorAllocator* m_pSRVUAVAllocator = nullptr;

	ID3D12Resource* m_pRenderTargets[SWAP_CHAIN_FRAME_COUNT] = { nullptr, };
	ID3D12Resource* m_pFloatBuffer = nullptr;
	ID3D12Resource* m_pPrevBuffer = nullptr;
	ID3D12Resource* m_pDefaultDepthStencil = nullptr;
	UINT m_MainRenderTargetOffset = 0xffffffff;
	UINT m_FloatBufferRTVOffset = 0xffffffff;
	UINT m_DefaultDepthStencilOffset = 0xffffffff;
	UINT m_FloatBufferSRVOffset = 0xffffffff;
	UINT m_PrevBufferSRVOffset = 0xffffffff;
	UINT m_FrameIndex = 0;

	// control.
	Camera m_Camera;
};
