#pragma once

#include <ctype.h>
#include <physx/PxPhysicsAPI.h>
#include "../Graphics/ConstantBuffer.h"
#include "DynamicDescriptorPool.h"
#include "../Graphics/Texture.h"

class ConstantBuffer;

using namespace physx;

enum ePipelineStateSetting
{
	None = 0,
	Default,
	Skinned,
	Skybox,
	StencilMask,
	MirrorBlend,
	ReflectionDefault,
	ReflectionSkinned,
	ReflectionSkybox,
	DepthOnlyDefault,
	DepthOnlySkinned,
	DepthOnlyCubeDefault,
	DepthOnlyCubeSkinned,
	DepthOnlyCascadeDefault,
	DepthOnlyCascadeSkinned,
	Sampling,
	BloomDown,
	BloomUp,
	Combine,
};
enum eCommonContext
{
	// Pre = 0,
	Mid = 0,
	Post = 1
};

static const int NUM_THREADS = 6;
static const int LIGHT_THREADS = 3;
static const int COMMON_COMMANDLIST_COUNT = 2;
const UINT MAX_DESCRIPTOR_NUM = 256;

class ResourceManager
{
public:
	ResourceManager() = default;
	~ResourceManager() { Clear(); }

	void Initialize(ID3D12Device5* pDevice, ID3D12CommandQueue* pCommandQueue, ID3D12CommandAllocator* pCommandAllocator, ID3D12GraphicsCommandList* pCommandList, DynamicDescriptorPool* pDynamicDescriptorPool);
	void InitRTVDescriptorHeap(UINT maxDescriptorNum);
	void InitDSVDescriptorHeap(UINT maxDescriptorNum);
	void InitCBVSRVUAVDescriptorHeap(UINT maxDescriptorNum);

	HRESULT CreateVertexBuffer(UINT sizePerVertex, UINT numVertex, D3D12_VERTEX_BUFFER_VIEW* pOutVertexBufferView, ID3D12Resource** ppOutBuffer, void* pInitData);
	HRESULT CreateIndexBuffer(UINT sizePerIndex, UINT numIndex, D3D12_INDEX_BUFFER_VIEW* pOutIndexBufferView, ID3D12Resource** ppOutBuffer, void* pInitData);
	
	HRESULT UpdateTexture(ID3D12Resource* pDestResource, ID3D12Resource* pSrcResource, D3D12_RESOURCE_STATES* originalState);

	void Clear();

	void ResetCommandLists();

	void SetGlobalConstants(ConstantBuffer* pGlobal, ConstantBuffer* pLight, ConstantBuffer* pReflection);
	void SetCommonState(ePipelineStateSetting psoState);
	void SetCommonState(ID3D12GraphicsCommandList* pCommandList, ePipelineStateSetting psoState);

protected:
	void initSamplers();
	void initRasterizerStateDescs();
	void initBlendStateDescs();
	void initDepthStencilStateDescs();
	void initPipelineStates();
	void initShaders();
	void initCommandLists();
	void initPhysics(bool interactive);

	UINT64 fence();
	void waitForGPU();

public:
	ID3D12Device5* m_pDevice = nullptr;
	ID3D12CommandQueue* m_pCommandQueue = nullptr;
	ID3D12CommandAllocator* m_pSingleCommandAllocator = nullptr;
	ID3D12GraphicsCommandList* m_pSingleCommandList = nullptr;
	
	ID3D12CommandList* m_pBatchSubmits[COMMON_COMMANDLIST_COUNT + LIGHT_THREADS + NUM_THREADS * 2] = { nullptr, };

	ID3D12CommandAllocator* m_pCommandAllocators[COMMON_COMMANDLIST_COUNT] = { nullptr, };
	ID3D12CommandAllocator* m_pShadowCommandAllocators[LIGHT_THREADS] = { nullptr, };
	ID3D12CommandAllocator* m_pRenderCommandAllocators[NUM_THREADS] = { nullptr, };
	ID3D12CommandAllocator* m_pMirrorCommandAllocators[NUM_THREADS] = { nullptr, };
	ID3D12GraphicsCommandList* m_pCommandLists[COMMON_COMMANDLIST_COUNT] = { nullptr, };
	ID3D12GraphicsCommandList* m_pShadowCommandLists[LIGHT_THREADS] = { nullptr, };
	ID3D12GraphicsCommandList* m_pRenderCommandLists[NUM_THREADS] = { nullptr, };
	ID3D12GraphicsCommandList* m_pMirrorCommandLists[NUM_THREADS] = { nullptr, };

	ID3D12DescriptorHeap* m_pRTVHeap = nullptr;
	ID3D12DescriptorHeap* m_pDSVHeap = nullptr;
	ID3D12DescriptorHeap* m_pCBVSRVUAVHeap = nullptr;
	ID3D12DescriptorHeap* m_pSamplerHeap = nullptr;
	DynamicDescriptorPool* m_pDynamicDescriptorPool = nullptr;

	UINT m_RTVDescriptorSize = 0;
	UINT m_DSVDescriptorSize = 0;
	UINT m_CBVSRVUAVDescriptorSize = 0;
	UINT m_SamplerDescriptorSize = 0;

	UINT m_RTVHeapSize = 0;
	UINT m_DSVHeapSize = 0;
	UINT m_CBVSRVUAVHeapSize = 0;
	UINT m_SamplerHeapSize = 0;

	// descriptor set 편의를 위한 offset 저장 용도.
	UINT m_GlobalConstantViewStartOffset = 0xffffffff; // b0, b1
	UINT m_GlobalShaderResourceViewStartOffset = 0xffffffff; // t8 ~ t16

private:
	HANDLE m_hFenceEvent = nullptr;
	ID3D12Fence* m_pFence = nullptr;
	UINT64 m_FenceValue = 0;

	// physx 관련.
	PxDefaultAllocator m_PhysxAllocator;
	PxDefaultErrorCallback m_ErrorCallback;
	PxFoundation* m_pFoundation = nullptr;
	PxPhysics* m_pPhysics = nullptr;
	PxDefaultCpuDispatcher* m_pDispatcher = nullptr;
	PxScene* m_pScene = nullptr;
	PxMaterial* m_pMaterial = nullptr;
	PxPvd* m_pPvd = nullptr;

	// root signature.
	ID3D12RootSignature* m_pDefaultRootSignature = nullptr;
	ID3D12RootSignature* m_pSkinnedRootSignature = nullptr;
	ID3D12RootSignature* m_pDepthOnlyRootSignature = nullptr;
	ID3D12RootSignature* m_pDepthOnlySkinnedRootSignature = nullptr;
	ID3D12RootSignature* m_pDepthOnlyAroundRootSignature = nullptr;
	ID3D12RootSignature* m_pDepthOnlyAroundSkinnedRootSignature = nullptr;
	ID3D12RootSignature* m_pSamplingRootSignature = nullptr;
	ID3D12RootSignature* m_pCombineRootSignature = nullptr;

	// pipeline state.
	ID3D12PipelineState* m_pDefaultSolidPSO = nullptr;
	ID3D12PipelineState* m_pSkinnedSolidPSO = nullptr;
	ID3D12PipelineState* m_pSkyboxSolidPSO = nullptr;

	ID3D12PipelineState* m_pStencilMaskPSO = nullptr;
	ID3D12PipelineState* m_pMirrorBlendSolidPSO = nullptr;
	ID3D12PipelineState* m_pReflectDefaultSolidPSO = nullptr;
	ID3D12PipelineState* m_pReflectSkinnedSolidPSO = nullptr;
	ID3D12PipelineState* m_pReflectSkyboxSolidPSO = nullptr;

	ID3D12PipelineState* m_pDepthOnlyPSO = nullptr;
	ID3D12PipelineState* m_pDepthOnlySkinnedPSO = nullptr;
	ID3D12PipelineState* m_pDepthOnlyCubePSO = nullptr;
	ID3D12PipelineState* m_pDepthOnlyCubeSkinnedPSO = nullptr;
	ID3D12PipelineState* m_pDepthOnlyCascadePSO = nullptr;
	ID3D12PipelineState* m_pDepthOnlyCascadeSkinnedPSO = nullptr;

	ID3D12PipelineState* m_pSamplingPSO = nullptr;
	ID3D12PipelineState* m_pBloomDownPSO = nullptr;
	ID3D12PipelineState* m_pBloomUpPSO = nullptr;
	ID3D12PipelineState* m_pCombinePSO = nullptr;

	// rasterizer state.
	D3D12_RASTERIZER_DESC m_RasterizerSolidDesc = {};
	D3D12_RASTERIZER_DESC m_RasterizerSolidCcwDesc = {};
	D3D12_RASTERIZER_DESC m_RasterizerPostProcessDesc = {};

	// depthstencil state.
	D3D12_DEPTH_STENCIL_DESC m_DepthStencilDrawDesc = {};
	D3D12_DEPTH_STENCIL_DESC m_DepthStencilMaskDesc = {};
	D3D12_DEPTH_STENCIL_DESC m_DepthStencilDrawMaskedDesc = {};

	// inputlayouts.
	D3D12_INPUT_ELEMENT_DESC m_InputLayoutBasicDescs[4] = {};
	D3D12_INPUT_ELEMENT_DESC m_InputLayoutSkinnedDescs[8] = {};
	D3D12_INPUT_ELEMENT_DESC m_InputLayoutSkyboxDescs[4] = {};
	D3D12_INPUT_ELEMENT_DESC m_InputLayoutSamplingDescs[3] = {};

	// shaders.
	ID3DBlob* m_pBasicVS = nullptr;
	ID3DBlob* m_pSkinnedVS = nullptr;
	ID3DBlob* m_pSkyboxVS = nullptr;
	ID3DBlob* m_pDepthOnlyVS = nullptr;
	ID3DBlob* m_pDepthOnlySkinnedVS = nullptr;
	ID3DBlob* m_pDepthOnlyCubeVS = nullptr;
	ID3DBlob* m_pDepthOnlyCubeSkinnedVS = nullptr;
	ID3DBlob* m_pDepthOnlyCascadeVS = nullptr;
	ID3DBlob* m_pDepthOnlyCascadeSkinnedVS = nullptr;
	ID3DBlob* m_pSamplingVS = nullptr;

	ID3DBlob* m_pBasicPS = nullptr;
	ID3DBlob* m_pSkyboxPS = nullptr;
	ID3DBlob* m_pDepthOnlyPS = nullptr;
	ID3DBlob* m_pDepthOnlyCubePS = nullptr;
	ID3DBlob* m_pDepthOnlyCascadePS = nullptr;
	ID3DBlob* m_pSamplingPS = nullptr;
	ID3DBlob* m_pCombinePS = nullptr;
	ID3DBlob* m_pBloomDownPS = nullptr;
	ID3DBlob* m_pBloomUpPS = nullptr;

	ID3DBlob* m_pDepthOnlyCubeGS = nullptr;
	ID3DBlob* m_pDepthOnlyCascadeGS = nullptr;

	// blend state.
	D3D12_BLEND_DESC m_BlendMirrorDesc = {};
	D3D12_BLEND_DESC m_BlendAccumulateDesc = {};
	D3D12_BLEND_DESC m_BlendAlphaDesc = {};

	// Global Constant Buffers.
	ConstantBuffer* m_pGlobalConstant = nullptr;
	ConstantBuffer* m_pLightConstant = nullptr;
	ConstantBuffer* m_pReflectionConstant = nullptr;
};
