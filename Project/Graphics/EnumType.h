#pragma once

enum eRenderPass
{
	RenderPass_Shadow = 0,
	RenderPass_Object,
	RenderPass_Mirror,
	RenderPass_Collider,
	RenderPass_MainRender,
	RenderPass_RenderPassCount,
};
enum eRenderThreadEventType
{
	// RenderThreadEventType_Process,
	RenderThreadEventType_Shadow,
	RenderThreadEventType_Object,
	RenderThreadEventType_Mirror,
	RenderThreadEventType_Collider,
	RenderThreadEventType_MainRender,
	RenderThreadEventType_Desctroy,
	RenderThreadEventType_Count
};
enum eRenderObjectType
{
	RenderObjectType_DefaultType = 0,
	RenderObjectType_SkinnedType,
	RenderObjectType_SkyboxType,
	RenderObjectType_MirrorType,
	RenderObjectType_TotalObjectType
};
enum eRenderPSOType
{
	RenderPSOType_Default,
	RenderPSOType_Skinned,
	RenderPSOType_Skybox,
	RenderPSOType_StencilMask,
	RenderPSOType_MirrorBlend,
	RenderPSOType_ReflectionDefault,
	RenderPSOType_ReflectionSkinned,
	RenderPSOType_ReflectionSkybox,
	RenderPSOType_DepthOnlyDefault,
	RenderPSOType_DepthOnlySkinned,
	RenderPSOType_DepthOnlyCubeDefault,
	RenderPSOType_DepthOnlyCubeSkinned,
	RenderPSOType_DepthOnlyCascadeDefault,
	RenderPSOType_DepthOnlyCascadeSkinned,
	RenderPSOType_Sampling,
	RenderPSOType_BloomDown,
	RenderPSOType_BloomUp,
	RenderPSOType_Combine,
	RenderPSOType_Wire,
	RenderPSOType_PipelineStateCount,
};
enum eConstantBufferType
{
	ConstantBufferType_Mesh = 0,
	ConstantBufferType_Material,
	ConstantBufferType_LightProperty,
	ConstantBufferType_LightConstant,
	ConstantBufferType_ShadowConstant,
	ConstantBufferType_GlobalConstant,
	ConstantBufferType_ImageFilterConstant,
	ConstantBufferType_ConstantTypeCount,
};
