#pragma once

#include "../Graphics/Camera.h"
#include "ConstantDataType.h"
#include "ConstantBufferManager.h"
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

		Texture* pEnvTexture;
		Texture* pIrradianceTexture;
		Texture* pSpecularTexture;
		Texture* pBRDFTexture;

		Model* pMirror;
		DirectX::SimpleMath::Plane* pMirrorPlane;
	};

public:
	Renderer();
	virtual ~Renderer();

	void Initizlie(InitialData* pIntialData);

	void Update(const float DELTA_TIME);

	void Render();
	void ProcessByThread(UINT threadIndex, ResourceManager* pManager, int renderPass);

	void Cleanup();

	LRESULT MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	inline ResourceManager* GetResourceManager() { return m_pResourceManager; }
	inline PhysicsManager* GetPhysicsManager() { return &m_PhysicsManager; }
	inline HWND GetWindow() { return m_hMainWindow; }

protected:
	void initMainWidndow();
	void initDirect3D();
	void initPhysics();
	void initScene();
	void initDescriptorHeap(Texture* pEnvTexture, Texture* pIrradianceTexture, Texture* pSpecularTexture, Texture* pBRDFTexture);
	void initRenderThreadPool(UINT renderThreadCount);

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

	UINT64 fence();
	void waitForFenceValue(UINT64 expectedFenceValue);

protected:
	HWND m_hMainWindow = nullptr;

	HANDLE m_hFenceEvent = nullptr;
	ID3D12Fence* m_pFence = nullptr;
	UINT64 m_LastFenceValues[SWAP_CHAIN_FRAME_COUNT] = { 0, };
	UINT64 m_FenceValue = 0;

	PhysicsManager m_PhysicsManager;
	PostProcessor m_PostProcessor;

	Keyboard m_Keyboard;
	Mouse m_Mouse;

	// external data
	ConstantBuffer m_GlobalConstant;
	ConstantBuffer m_LightConstant;
	ConstantBuffer m_ReflectionGlobalConstant;

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
	ResourceManager* m_pResourceManager = nullptr;

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
	ID3D12CommandAllocator* m_ppCommandAllocator[SWAP_CHAIN_FRAME_COUNT] = { nullptr, };
	ID3D12GraphicsCommandList* m_ppCommandList[SWAP_CHAIN_FRAME_COUNT] = { nullptr, };

	// for multi-thread ////////////////////////
	// ID3D12CommandQueue* m_ppCommandQueue[RenderPass_RenderPassCount] = { nullptr, };
	RenderQueue* m_pppRenderQueue[RenderPass_RenderPassCount][MAX_RENDER_THREAD_COUNT] = { nullptr, };
	CommandListPool* m_pppCommandListPool[SWAP_CHAIN_FRAME_COUNT][MAX_RENDER_THREAD_COUNT] = { nullptr, };
	DynamicDescriptorPool* m_pppDescriptorPool[SWAP_CHAIN_FRAME_COUNT][MAX_RENDER_THREAD_COUNT] = { nullptr, };
	ConstantBufferManager* m_pppConstantBufferManager[SWAP_CHAIN_FRAME_COUNT][MAX_RENDER_THREAD_COUNT] = { nullptr, };
	RenderThreadDesc* m_pThreadDescList = nullptr;
	UINT m_RenderThreadCount = 0;
	UINT m_CurThreadIndex = 0; // render queue에 균등하게 넣기 위한 인덱스.

	long volatile m_pActiveThreadCounts[RenderPass_RenderPassCount] = { 0, };
	HANDLE m_phCompletedEvents[RenderPass_RenderPassCount] = { nullptr, };
	/////////////////////////////////////////////

	// main resources.
	DynamicDescriptorPool m_DynamicDescriptorPool;
	ConstantBufferManager m_ConstantBufferManager; // when multi-thread used, need to be create more manager.

	ID3D12Resource* m_pRenderTargets[SWAP_CHAIN_FRAME_COUNT] = { nullptr, };
	ID3D12Resource* m_pFloatBuffer = nullptr;
	ID3D12Resource* m_pPrevBuffer = nullptr;
	ID3D12Resource* m_pDefaultDepthStencil = nullptr;
	UINT m_MainRenderTargetOffset = 0xffffffff;
	UINT m_FloatBufferRTVOffset = 0xffffffff;
	UINT m_FloatBufferSRVOffset = 0xffffffff;
	UINT m_PrevBufferSRVOffset = 0xffffffff;
	UINT m_FrameIndex = 0;

	// control.
	Camera m_Camera;
};
