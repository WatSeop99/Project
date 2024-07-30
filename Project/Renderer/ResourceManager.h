#pragma once

#include <ctype.h>
#include "CommandListPool.h"
#include "DynamicDescriptorPool.h"
#include "RenderQueue.h"
#include "TextureManager.h"

struct TextureHandle;
class ConstantBuffer;
class TextureManager;
class Renderer;

static const UINT SWAP_CHAIN_FRAME_COUNT = 2;
static const UINT MAX_RENDER_THREAD_COUNT = 6;
static const UINT MAX_DESCRIPTOR_NUM = 1024;

class ResourceManager
{
public:
	struct TextureHandles
	{
		TextureHandle* ppLightShadowMaps[MAX_LIGHTS];
		TextureHandle* pEnvTexture;
		TextureHandle* pIrradianceTexture;
		TextureHandle* pSpecularTexture;
		TextureHandle* pBRDFTetxure;
	};

public:
	ResourceManager() = default;
	~ResourceManager() { Cleanup(); }

	void Initialize(Renderer* pRenderer);

	HRESULT CreateVertexBuffer(UINT sizePerVertex, UINT numVertex, D3D12_VERTEX_BUFFER_VIEW* pOutVertexBufferView, ID3D12Resource** ppOutBuffer, void* pInitData);
	HRESULT CreateIndexBuffer(UINT sizePerIndex, UINT numIndex, D3D12_INDEX_BUFFER_VIEW* pOutIndexBufferView, ID3D12Resource** ppOutBuffer, void* pInitData);
	
	HRESULT CreateTextureFromFile(ID3D12Resource** ppOutResource, D3D12_RESOURCE_DESC* pOutDesc, const WCHAR* pszFileName, bool bUseSRGB);
	HRESULT CreateTextureCubeFromFile(ID3D12Resource** ppOutResource, D3D12_RESOURCE_DESC* pOutDesc, const WCHAR* pszFileName);
	HRESULT CreateTexturePair(ID3D12Resource** ppOutResource, ID3D12Resource** ppOutUploadBuffer, UINT width, UINT height, DXGI_FORMAT format);
	HRESULT CreateTexture(ID3D12Resource** ppOutResource, UINT width, UINT height, DXGI_FORMAT format, const BYTE* pInitImage);
	HRESULT CreateNonImageUploadTexture(ID3D12Resource** ppOutResource, UINT numElement, UINT elementSize);

	HRESULT UpdateTexture(ID3D12Resource* pDestResource, ID3D12Resource* pSrcResource, D3D12_RESOURCE_STATES* pOriginalState);

	void Cleanup();

	void SetGlobalConstants(GlobalConstant* pGlobal, LightConstant* pLight, GlobalConstant* pReflection);
	void SetGlobalTextures(TextureHandles* pHandles);
	void SetCommonState(eRenderPSOType psoState);
	void SetCommonState(UINT threadIndex, ID3D12GraphicsCommandList* pCommandList, DynamicDescriptorPool* pDescriptorPool, ConstantBufferManager* pConstantBufferManager, int psoState);

protected:
	void initSamplers();
	void initRasterizerStateDescs();
	void initBlendStateDescs();
	void initDepthStencilStateDescs();
	void initPipelineStates();
	void initShaders();

public:
	D3D12_CPU_DESCRIPTOR_HANDLE m_NullSRVDescriptor = { 0xffffffff, };

	ID3D12DescriptorHeap* m_pSamplerHeap = nullptr;

	UINT m_RTVDescriptorSize = 0;
	UINT m_DSVDescriptorSize = 0;
	UINT m_CBVSRVUAVDescriptorSize = 0;
	UINT m_SamplerDescriptorSize = 0;

	UINT m_RTVHeapSize = 0;
	UINT m_DSVHeapSize = 0;
	UINT m_CBVSRVUAVHeapSize = 0;
	UINT m_SamplerHeapSize = 0;

	// descriptor set ���Ǹ� ���� offset ���� �뵵.
	UINT m_GlobalShaderResourceViewStartOffset = 0xffffffff; // t8 ~ t16

private:
	Renderer* m_pRenderer = nullptr;

	ID3D12Device5* m_pDevice = nullptr;
	ID3D12CommandQueue* m_pCommandQueue = nullptr;
	ID3D12CommandAllocator* m_pCommandAllocator = nullptr;
	ID3D12GraphicsCommandList* m_pCommandList = nullptr;

	// root signature.
	ID3D12RootSignature* m_pDefaultRootSignature = nullptr;
	ID3D12RootSignature* m_pSkinnedRootSignature = nullptr;
	ID3D12RootSignature* m_pDepthOnlyRootSignature = nullptr;
	ID3D12RootSignature* m_pDepthOnlySkinnedRootSignature = nullptr;
	ID3D12RootSignature* m_pDepthOnlyAroundRootSignature = nullptr;
	ID3D12RootSignature* m_pDepthOnlyAroundSkinnedRootSignature = nullptr;
	ID3D12RootSignature* m_pSamplingRootSignature = nullptr;
	ID3D12RootSignature* m_pCombineRootSignature = nullptr;
	ID3D12RootSignature* m_pDefaultWireRootSignature = nullptr;

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

	ID3D12PipelineState* m_pDefaultWirePSO = nullptr;

	// rasterizer state.
	D3D12_RASTERIZER_DESC m_RasterizerSolidDesc = {};
	D3D12_RASTERIZER_DESC m_RasterizerSolidCcwDesc = {};
	D3D12_RASTERIZER_DESC m_RasterizerPostProcessDesc = {};
	D3D12_RASTERIZER_DESC m_RasterizerWireDesc = {};

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
	ID3DBlob* m_pColorPS = nullptr;

	ID3DBlob* m_pDepthOnlyCubeGS = nullptr;
	ID3DBlob* m_pDepthOnlyCascadeGS = nullptr;

	// blend state.
	D3D12_BLEND_DESC m_BlendMirrorDesc = {};
	D3D12_BLEND_DESC m_BlendAccumulateDesc = {};
	D3D12_BLEND_DESC m_BlendAlphaDesc = {};

	// Global Constant Buffers.
	GlobalConstant* m_pGlobalConstantData = nullptr;
	LightConstant* m_pLightConstantData = nullptr;
	GlobalConstant* m_pReflectionConstantData = nullptr;

	// Global Textures.
	TextureHandle* m_ppLightShadowMaps[MAX_LIGHTS] = { nullptr, };
	TextureHandle* m_pEnvTexture = nullptr;
	TextureHandle* m_pIrradianceTexture = nullptr;
	TextureHandle* m_pSpecularTexture = nullptr;
	TextureHandle* m_pBRDFTexture = nullptr;
};
