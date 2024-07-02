#include "Common.hlsli"
#include "DiskSamples.hlsli"

struct PixelShaderOutput
{
	float4 PixelColor : SV_TARGET0;
};

#define NEAR_PLANE 0.01f
#define FAR_PLANE 50.0f
#define LIGHT_FRUSTUM_WIDTH 0.34641f // <- 계산해서 찾은 값

static const float3 F_DIELECTRIC = 0.04f; // 비금속(Dielectric) 재질의 F0.

// 메쉬 재질 텍스춰들 t0 부터 시작.
Texture2D g_AlbedoTex : register(t0);
Texture2D g_EmissiveTex : register(t1);
Texture2D g_NormalTex : register(t2);
Texture2D g_AmbientOcclusionTex : register(t3);
Texture2D g_MetallicTex : register(t4);
Texture2D g_RoughnessTex : register(t5);

float3 SchlickFresnel(float3 F0, float NdotH)
{
	return F0 + (1.0f - F0) * pow(2.0f, (-5.55473f * NdotH - 6.98316f) * NdotH);
}

float3 GetNormal(PixelShaderInput input)
{
	float3 normalWorld = normalize(input.WorldNormal);

	if (bUseNormalMap)
	{
		float3 normal = g_NormalTex.SampleLevel(g_LinearWrapSampler, input.Texcoord, g_LODBias).rgb;
		normal = 2.0f * normal - 1.0f; // 범위 조절 [-1.0, 1.0]

		// OpenGL 용 노멀맵일 경우에는 y 방향을 뒤집어줌.
		normal.y = (bInvertNormalMapY ? -normal.y : normal.y);

		float3 N = normalWorld;
		float3 T = normalize(input.WorldTangent - dot(input.WorldTangent, N) * N);
		float3 B = cross(N, T);

		// matrix는 float4x4, 여기서는 벡터 변환용이라서 3x3 사용.
		float3x3 TBN = float3x3(T, B, N);
		normalWorld = normalize(mul(normal, TBN));
	}

	return normalWorld;
}

float3 DiffuseIBL(float3 albedo, float3 normalWorld, float3 pixelToEye, float metallic)
{
	float3 F0 = lerp(F_DIELECTRIC, albedo, metallic);
	float3 F = SchlickFresnel(F0, max(0.0f, dot(normalWorld, pixelToEye)));
	float3 kd = lerp(1.0f - F, 0.0f, metallic);
	float3 irradiance = g_IrradianceIBLTex.SampleLevel(g_LinearWrapSampler, normalWorld, 0.0f).rgb;

	return kd * albedo * irradiance;
}

float3 SpecularIBL(float3 albedo, float3 normalWorld, float3 pixelToEye, float metallic, float roughness)
{
	float2 specularBRDF = g_BRDFTex.SampleLevel(g_LinearClampSampler, float2(dot(normalWorld, pixelToEye), 1.0 - roughness), 0.0f).rg;
	float3 specularIrradiance = g_SpecularIBLTex.SampleLevel(g_LinearWrapSampler, reflect(-pixelToEye, normalWorld), 2.0f + roughness * 5.0f).rgb;
	const float3 Fdielectric = 0.04f; // 비금속(Dielectric) 재질의 F0
	float3 F0 = lerp(Fdielectric, albedo, metallic);

	return (F0 * specularBRDF.x + specularBRDF.y) * specularIrradiance;
}

float3 AmbientLightingByIBL(float3 albedo, float3 normalW, float3 pixelToEye, float ao, float metallic, float roughness)
{
	float3 diffuseIBL = DiffuseIBL(albedo, normalW, pixelToEye, metallic);
	float3 specularIBL = SpecularIBL(albedo, normalW, pixelToEye, metallic, roughness);

	return (diffuseIBL + specularIBL) * ao;
}

float NdfGGX(float NdotH, float roughness, float alphaPrime)
{
	float alpha = roughness * roughness;
	float alphaSq = alpha * alpha;
	float denom = (NdotH * NdotH) * (alphaSq - 1.0f) + 1.0f;
	return alphaPrime * alphaPrime / (3.141592f * denom * denom);
}

float SchlickG1(float NdotV, float k)
{
	return NdotV / (NdotV * (1.0f - k) + k);
}

float SchlickGGX(float NdotI, float NdotO, float roughness)
{
	float r = roughness + 1.0f;
	float k = (r * r) / 8.0f;
	return SchlickG1(NdotI, k) * SchlickG1(NdotO, k);
}

float random(float3 seed, int i)
{
	float4 seed4 = float4(seed, i);
	float dotProduct = dot(seed4, float4(12.9898f, 78.233f, 45.164f, 94.673f));

	return frac(sin(dotProduct) * 43758.5453f);
}

float N2V(float ndcDepth, matrix invProj)
{
	// return invProj[3][2] / (ndcDepth - invProj[2][2]);
	float4 pointView = mul(float4(0.0f, 0.0f, ndcDepth, 1.0f), invProj);
	return pointView.z / pointView.w;
}

float PCFFilterSpotLight(int shadowMapIndex, float2 uv, float zReceiverNDC, float filterRadiusUV)
{
	float sum = 0.0f;
	for (int i = 0; i < 64; ++i)
	{
		float2 offset = diskSamples64[i] * filterRadiusUV;
		sum += g_ShadowMaps[shadowMapIndex].SampleCmpLevelZero(g_ShadowCompareSampler, uv + offset, zReceiverNDC);
	}
	return sum / 64.0f;
}

float PCFFilterDirectionalLight(int shadowMapIndex, float2 uv, float zReceiverNDC, float filterRadiusUV)
{
	float sum = 0.0f;
	for (int i = 0; i < 64; ++i)
	{
		float2 offset = diskSamples64[i] * filterRadiusUV;
		sum += g_CascadeShadowMap.SampleCmpLevelZero(g_ShadowCompareSampler, float3(uv + offset, shadowMapIndex), zReceiverNDC);
	}
	return sum / 64.0f;
}

float PCFFilterPointLight(float3 uvw, float zReceiverNDC, float filterRadiusUV)
{
	float sum = 0.0f;
	for (int i = 0; i < 64; ++i)
	{
		float3 offset = float3(diskSamples64[i], 0.0f) * filterRadiusUV;
		sum += g_PointLightShadowMap.SampleCmpLevelZero(g_ShadowCompareSampler, uvw + offset, zReceiverNDC);
	}
	return sum / 64.0f;
}

void FindBlockerInSpotLight(out float avgBlockerDepthView, out float numBlockers, int shadowMapIndex, float2 uv, float zReceiverView, matrix inverseProjection, float lightRadiusWorld)
{
	float lightRadiusUV = lightRadiusWorld / LIGHT_FRUSTUM_WIDTH;
	float searchRadius = lightRadiusUV * (zReceiverView - NEAR_PLANE) / zReceiverView;

	float blockerSum = 0.0f;
	numBlockers = 0.0f;
	for (int i = 0; i < 64; ++i)
	{
		float shadowMapDepth = g_ShadowMaps[shadowMapIndex].SampleLevel(g_ShadowPointSampler, float2(uv + diskSamples64[i] * searchRadius), 0.0f).r;
		shadowMapDepth = N2V(shadowMapDepth, inverseProjection);

		if (shadowMapDepth < zReceiverView)
		{
			blockerSum += shadowMapDepth;
			++numBlockers;
		}
	}

	avgBlockerDepthView = blockerSum / numBlockers;
}

void FindBlockerInDirectionalLight(out float avgBlockerDepthView, out float numBlockers, float2 uv, float zReceiverView, int shadowMapIndex, matrix invProj, float lightRadiusWorld)
{
	float lightRadiusUV = lightRadiusWorld / LIGHT_FRUSTUM_WIDTH;
	float searchRadius = lightRadiusUV * (zReceiverView - NEAR_PLANE) / zReceiverView;

	float blockerSum = 0.0f;
	numBlockers = 0.0f;
	for (int i = 0; i < 64; ++i)
	{
		float shadowMapDepth = g_CascadeShadowMap.SampleLevel(g_ShadowPointSampler, float3(uv + diskSamples64[i] * searchRadius, shadowMapIndex), 0.0f).r;
		shadowMapDepth = N2V(shadowMapDepth, invProj);

		if (shadowMapDepth < zReceiverView)
		{
			blockerSum += shadowMapDepth;
			++numBlockers;
		}
	}

	avgBlockerDepthView = blockerSum / numBlockers;
}

void FindBlockerInPointLight(out float avgBlockerDepthView, out float numBlockers, float3 uvw, float zReceiverView, matrix invProj, float lightRadiusWorld)
{
	float lightRadiusUV = lightRadiusWorld / LIGHT_FRUSTUM_WIDTH;
	float searchRadius = lightRadiusUV * (zReceiverView - NEAR_PLANE) / zReceiverView;

	float blockerSum = 0.0f;
	numBlockers = 0.0f;
	for (int i = 0; i < 64; ++i)
	{
		float shadowMapDepth = g_PointLightShadowMap.SampleLevel(g_ShadowPointSampler, uvw + float3(diskSamples64[i], 0.0f) * searchRadius, 0.0f).r;
		shadowMapDepth = N2V(shadowMapDepth, invProj);

		if (shadowMapDepth < zReceiverView)
		{
			blockerSum += shadowMapDepth;
			++numBlockers;
		}
	}

	avgBlockerDepthView = blockerSum / numBlockers;
}

float PCSSForSpotLight(int shadowMapIndex, float2 uv, float zReceiverNDC, matrix inverseProjection, float lightRadiusWorld)
{
	float lightRadiusUV = lightRadiusWorld / LIGHT_FRUSTUM_WIDTH;
	float zReceiverView = N2V(zReceiverNDC, inverseProjection);

	// STEP 1: blocker search.
	float avgBlockerDepthView = 0;
	float numBlockers = 0;

	FindBlockerInSpotLight(avgBlockerDepthView, numBlockers, shadowMapIndex, uv, zReceiverView, inverseProjection, lightRadiusWorld);

	if (numBlockers < 1)
	{
		// There are no occluders so early out(this saves filtering).
		return 1.0f;
	}
	else
	{
		// STEP 2: penumbra size.
		float penumbraRatio = (zReceiverView - avgBlockerDepthView) / avgBlockerDepthView;
		float filterRadiusUV = penumbraRatio * lightRadiusUV * NEAR_PLANE / zReceiverView;

		// STEP 3: filtering.
		return PCFFilterSpotLight(shadowMapIndex, uv, zReceiverNDC, filterRadiusUV);
	}
}

float PCSSForDirectionalLight(int shadowMapIndex, float2 uv, float zReceiverNDC, matrix inverseProjection, float lightRadiusWorld)
{
	float lightRadiusUV = lightRadiusWorld / LIGHT_FRUSTUM_WIDTH;
	float zReceiverView = N2V(zReceiverNDC, inverseProjection);

	// STEP 1: blocker search.
	float avgBlockerDepthView = 0;
	float numBlockers = 0;

	FindBlockerInDirectionalLight(avgBlockerDepthView, numBlockers, uv, zReceiverView, shadowMapIndex, inverseProjection, lightRadiusWorld);

	if (numBlockers < 1)
	{
		// There are no occluders so early out(this saves filtering).
		return 1.0f;
	}
	else
	{
		// STEP 2: penumbra size.
		float penumbraRatio = (zReceiverView - avgBlockerDepthView) / avgBlockerDepthView;
		float filterRadiusUV = penumbraRatio * lightRadiusUV * NEAR_PLANE / zReceiverView;

		// STEP 3: filtering.
		return PCFFilterDirectionalLight(shadowMapIndex, uv, zReceiverNDC, filterRadiusUV);
	}
}

float PCSSForPointLight(float3 uvw, float zReceiverNDC, matrix inverseProjection, float lightRadiusWorld)
{
	float lightRadiusUV = lightRadiusWorld / LIGHT_FRUSTUM_WIDTH;
	float zReceiverView = N2V(zReceiverNDC, inverseProjection);
	// float zReceiverView = length(uvw);

	// STEP 1: blocker search.
	float avgBlockerDepthView = 0;
	float numBlockers = 0;

	FindBlockerInPointLight(avgBlockerDepthView, numBlockers, uvw, zReceiverView, inverseProjection, lightRadiusWorld);

	if (numBlockers < 1)
	{
		// There are no occluders so early out(this saves filtering).
		return 1.0f;
	}
	else
	{
		// STEP 2: penumbra size.
		float penumbraRatio = (zReceiverView - avgBlockerDepthView) / avgBlockerDepthView;
		float filterRadiusUV = penumbraRatio * lightRadiusUV * NEAR_PLANE / zReceiverView;

		// STEP 3: filtering.
		return PCFFilterPointLight(uvw, zReceiverNDC, filterRadiusUV);
	}
}

float3 LightRadiance(Light light, int shadowMapIndex, float3 representativePoint, float3 posWorld, float3 normalWorld)
{
	// Directional light.
	float3 lightVec = (light.Type & LIGHT_DIRECTIONAL ? -light.Direction : representativePoint - posWorld); // light.position - posWorld;
	float lightDist = length(lightVec);
	lightVec /= lightDist;

	// Spot light.
	float spotFator = (light.Type & LIGHT_SPOT ? pow(max(-dot(lightVec, light.Direction), 0.0f), light.SpotPower) : 1.0f);

	// Distance attenuation.
	float att = saturate((light.FallOffEnd - lightDist) / (light.FallOffEnd - light.FallOffStart));

	// Shadow map.
	float shadowFactor = 1.0f;

	if (light.Type & LIGHT_SHADOW)
	{
		float4 lightScreen = float4(0.0f, 0.0f, 0.0f, 0.0f);
		float3 lightTexcoord = float3(0.0f, 0.0f, 0.0f);
		float radiusScale = 0.5f; // 광원의 반지름을 키웠을 때 깨지는 것 방지.

		switch (light.Type & (LIGHT_DIRECTIONAL | LIGHT_POINT | LIGHT_SPOT))
		{
			case LIGHT_DIRECTIONAL:
			{
				int index = -1;
				lightScreen = float4(0.0f, 0.0f, 0.0f, 0.0f);

				for (int i = 0; i < 4; ++i)
				{
					lightScreen = mul(float4(posWorld, 1.0f), light.ViewProjection[i]);
					lightScreen.xyz /= lightScreen.w;
					if (lightScreen.z > 1.0f)
					{
						continue;
					}

					lightTexcoord.xy = float2(lightScreen.x, -lightScreen.y);
					lightTexcoord.xy += 1.0f;
					lightTexcoord.xy *= 0.5f;

					float depth = g_CascadeShadowMap.SampleLevel(g_ShadowPointSampler, float3(lightTexcoord.xy, i), 0.0f).r;
					if (depth <= lightScreen.z - 0.005f)
                    {
                        index = i;
                        break;
                    }
                }

				if (index != -1)
				{
					shadowFactor = PCSSForDirectionalLight(index, lightTexcoord.xy, lightScreen.z - 0.001f, light.InverseProjections[index], light.Radius * radiusScale);
				}
			}
			break;

			case LIGHT_POINT:
			{
				const float3 VIEW_DIRs[6] =
				{
					float3(1.0f, 0.0f, 0.0f), // right
					float3(-1.0f, 0.0f, 0.0f), // left
					float3(0.0f, 1.0f, 0.0f), // up
					float3(0.0f, -1.0f, 0.0f), // down
					float3(0.0f, 0.0f, 1.0f), // front
					float3(0.0f, 0.0f, -1.0f) // back 
				};
				int index = 0;
				float maxDotProduct = -2.0f;
				float3 lightToPos = normalize(posWorld - light.Position);

				for (int i = 0; i < 6; ++i)
				{
					float curDot = dot(lightToPos, VIEW_DIRs[i]);
					if (maxDotProduct < curDot)
					{
						maxDotProduct = curDot;
						index = i;
					}
				}

				lightScreen = mul(float4(posWorld, 1.0f), light.ViewProjection[index]);
				lightScreen.xyz /= lightScreen.w;

				lightTexcoord = lightToPos;

				shadowFactor = PCSSForPointLight(lightTexcoord, lightScreen.z - 0.001f, light.InverseProjections[0], light.Radius * radiusScale);
			}
			break;

			case LIGHT_SPOT:
			{
				// Project posWorld to light screen.  
				lightScreen = mul(float4(posWorld, 1.0f), light.ViewProjection[0]);
				lightScreen.xyz /= lightScreen.w;

				// 카메라(광원)에서 볼 때의 텍스춰 좌표 계산. ([-1, 1], [-1, 1]) ==> ([0, 1], [0, 1])
				lightTexcoord.xy = float2(lightScreen.x, -lightScreen.y);
				lightTexcoord.xy += 1.0f;
				lightTexcoord.xy *= 0.5f;

				shadowFactor = PCSSForSpotLight(shadowMapIndex, lightTexcoord.xy, lightScreen.z - 0.001f, light.InverseProjections[0], light.Radius * radiusScale);
			}
			break;

			default:
				break;
		}
	}

	float3 radiance = light.Radiance * spotFator * att * shadowFactor;
	return radiance;
}

PixelShaderOutput main(PixelShaderInput input)
{
	float3 pixelToEye = normalize(g_EyeWorld - input.WorldPosition);
	float3 normalWorld = GetNormal(input);

	float4 albedo = (bUseAlbedoMap ? g_AlbedoTex.SampleLevel(g_LinearWrapSampler, input.Texcoord, g_LODBias) * float4(g_AlbedoFactor, 1.0f) : float4(g_AlbedoFactor, 1.0f));
	clip(albedo.a - 0.5f); // 투명한 부분의 픽셀은 그리지 않음.

	float ao = (bUseAOMap ? g_AmbientOcclusionTex.SampleLevel(g_LinearWrapSampler, input.Texcoord, g_LODBias).r : 1.0f);
	float metallic = (bUseMetallicMap ? g_MetallicTex.SampleLevel(g_LinearWrapSampler, input.Texcoord, g_LODBias).b * g_MetallicFactor : g_MetallicFactor);
	float roughness = (bUseRoughnessMap ? g_RoughnessTex.SampleLevel(g_LinearWrapSampler, input.Texcoord, g_LODBias).g * g_RoughnessFactor : g_RoughnessFactor);
	float3 emission = (bUseEmissiveMap ? g_EmissiveTex.SampleLevel(g_LinearWrapSampler, input.Texcoord, g_LODBias).rgb : g_EmissionFactor);

	float3 ambientLighting = AmbientLightingByIBL(albedo.rgb, normalWorld, pixelToEye, ao, metallic, roughness) * g_StrengthIBL;
	float3 directLighting = float3(0.0f, 0.0f, 0.0f);

	[unroll]
	for (int i = 0; i < MAX_LIGHTS; ++i)
	{
		if (lights[i].Type & LIGHT_OFF)
		{
			continue;
		}

		float3 L = lights[i].Position - input.WorldPosition; // 렌더링 지점에서 광원 중심으로의 벡터.
		float3 r = normalize(reflect(g_EyeWorld - input.WorldPosition, normalWorld)); // 시점의 반사벡터.
		float3 centerToRay = dot(L, r) * r - L; // 광원 중심에서 반사벡터 r로의 벡터.
		float3 representativePoint = L + centerToRay * clamp(lights[i].Radius / length(centerToRay), 0.0f, 1.0f); // 볼륨이 커진 광원과 빛 계산 시 사용하는 광원의 대표 점.
		representativePoint += input.WorldPosition; // world space로 변환을 위해 input.posWorld를 더해줌. 위에서 계산한 벡터들은 input.posWorld를 원점으로 하는 좌표계에서 정의됨.
		float3 lightVec = representativePoint - input.WorldPosition;

		//float3 lightVec = lights[i].Position - input.WorldPosition;
		float lightDist = length(lightVec);
		lightVec /= lightDist;
		float3 halfway = normalize(pixelToEye + lightVec);

		float NdotI = max(0.0f, dot(normalWorld, lightVec));
		float NdotH = max(0.0f, dot(normalWorld, halfway));
		float NdotO = max(0.0f, dot(normalWorld, pixelToEye));

		// const float3 F_DIELECTRIC = 0.04f; // 비금속(Dielectric) 재질의 F0
		float3 F0 = lerp(F_DIELECTRIC, albedo.rgb, metallic);
		float3 F = SchlickFresnel(F0, max(0.0f, dot(halfway, pixelToEye)));
		float3 kd = lerp(float3(1.0f, 1.0f, 1.0f) - F, float3(0.0f, 0.0f, 0.0f), metallic);
		float3 diffuseBRDF = kd * albedo.rgb;

		// Sphere Normalization
		float alpha = roughness * roughness;
		float alphaPrime = saturate(alpha + lights[i].Radius / (2.0f * lightDist));

		float D = NdfGGX(NdotH, roughness, alphaPrime);
		float3 G = SchlickGGX(NdotI, NdotO, roughness);
		float3 specularBRDF = (F * D * G) / max(1e-5, 4.0f * NdotI * NdotO);

		float3 radiance = float3(0.0f, 0.0f, 0.0f);
		radiance = LightRadiance(lights[i], i, representativePoint, input.WorldPosition, normalWorld);

		if (abs(dot(radiance, float3(1.0f, 1.0f, 1.0f))) > 1e-5)
		{
			directLighting += (diffuseBRDF + specularBRDF) * radiance * NdotI;
		}
	}

	PixelShaderOutput output;
	output.PixelColor = float4(ambientLighting + directLighting + emission, 1.0f);
	output.PixelColor = clamp(output.PixelColor, 0.0f, 1000.0f);

	return output;
}
