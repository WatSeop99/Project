#include "../pch.h"
#include "../Model/GeometryGenerator.h"
#include "App.h"

void App::Initialize()
{
	m_pRenderer = new Renderer;
	if (m_pRenderer == nullptr)
	{
		__debugbreak();
	}

	m_pRenderer->Initizlie();


}

void App::Run()
{
}

void App::Clear()
{
	if (m_Lights)
	{
		delete m_Lights;
		m_Lights = nullptr;
	}
}

void App::initScene()
{
	ResourceManager* pResourceManager = m_pRenderer->GetResourceManager();

	ULONGLONG elemCount = MAX_LIGHTS;
	ULONGLONG memSize = MAX_LIGHTS * sizeof(Light);
	m_Lights = (Container*)malloc(memSize);
	m_Lights->ElemCount = elemCount;
	m_Lights->MemSize = memSize;

	elemCount = MAX_LIGHTS;
	memSize = MAX_LIGHTS * sizeof(Model*);
	m_LightSpheres = (Container*)malloc(memSize);
	m_LightSpheres->ElemCount = elemCount;
	m_LightSpheres->MemSize = memSize;

	Light* pLightData = (Light*)m_Lights->Data;
	Model** pLightSphereData = (Model**)m_Lights->Data;

	{
		m_pRenderObjectsHead = new ListElem;
		m_pRenderObjectsTail = new ListElem;
		if (m_pRenderObjectsHead == nullptr)
		{
			__debugbreak();
		}
		if (m_pRenderObjectsTail == nullptr)
		{
			__debugbreak();
		}
		memset(m_pRenderObjectsHead, 0, sizeof(ListElem));
		memset(m_pRenderObjectsTail, 0, sizeof(ListElem));

		m_pRenderObjectsHead->pNext = m_pRenderObjectsTail;
		m_pRenderObjectsTail->pPrev = m_pRenderObjectsHead;
	}

	// 환경맵 텍스쳐 로드.
	{
		m_EnvTexture.InitializeWithDDS(pResourceManager, L"./Assets/Textures/Cubemaps/HDRI/clear_pureskyEnvHDR.dds");
		m_IrradianceTexture.InitializeWithDDS(pResourceManager, L"./Assets/Textures/Cubemaps/HDRI/clear_pureskyEnvHDR.dds");
		m_SpecularTexture.InitializeWithDDS(pResourceManager, L"./Assets/Textures/Cubemaps/HDRI/clear_pureskyEnvHDR.dds");
		m_BRDFTexture.InitializeWithDDS(pResourceManager, L"./Assets/Textures/Cubemaps/HDRI/clear_pureskyEnvHDR.dds");
	}

	// 조명 설정.
	{

		// 조명 0.
		pLightData[0].Property.Radiance = Vector3(3.0f);
		pLightData[0].Property.FallOffEnd = 10.0f;
		pLightData[0].Property.Position = Vector3(0.0f, 0.0f, 0.0f);
		pLightData[0].Property.Direction = Vector3(0.0f, 0.0f, 1.0f);
		pLightData[0].Property.SpotPower = 3.0f;
		pLightData[0].Property.LightType = LIGHT_POINT | LIGHT_SHADOW;
		pLightData[0].Property.Radius = 0.03f;
		pLightData[0].Initialize(pResourceManager);

		// 조명 1.
		pLightData[1].Property.Radiance = Vector3(3.0f);
		pLightData[1].Property.FallOffEnd = 10.0f;
		pLightData[1].Property.Position = Vector3(1.0f, 1.1f, 2.0f);
		pLightData[1].Property.SpotPower = 2.0f;
		pLightData[1].Property.Direction = Vector3(0.0f, -0.5f, 1.7f) - pLightData[1].Property.Position;
		pLightData[1].Property.Direction.Normalize();
		pLightData[1].Property.LightType = LIGHT_SPOT | LIGHT_SHADOW;
		pLightData[1].Property.Radius = 0.03f;
		pLightData[1].Initialize(pResourceManager);

		// 조명 2.
		pLightData[2].Property.Radiance = Vector3(5.0f);
		pLightData[2].Property.Position = Vector3(5.0f, 5.0f, 5.0f);
		pLightData[2].Property.Direction = Vector3(-1.0f, -1.0f, -1.0f);
		pLightData[2].Property.Direction.Normalize();
		pLightData[2].Property.LightType = LIGHT_DIRECTIONAL | LIGHT_SHADOW;
		pLightData[2].Property.Radius = 0.05f;
		pLightData[2].Initialize(pResourceManager);
	}

	// 조명 위치 표시.
	{
		// m_LightSpheres.resize(MAX_LIGHTS);

		for (int i = 0; i < MAX_LIGHTS; ++i)
		{
			MeshInfo sphere = INIT_MESH_INFO;
			MakeSphere(&sphere, 1.0f, 20, 20);

			pLightSphereData[i] = new Model(pResourceManager, { sphere });
			pLightSphereData[i]->UpdateWorld(Matrix::CreateTranslation(m_Lights[i].Property.Position));

			MaterialConstant* pSphereMaterialConst = (MaterialConstant*)m_LightSpheres[i]->Meshes[0]->MaterialConstant.pData;
			pSphereMaterialConst->AlbedoFactor = Vector3(0.0f);
			pSphereMaterialConst->EmissionFactor = Vector3(1.0f, 1.0f, 0.0f);
			pLightSphereData[i]->bCastShadow = false; // 조명 표시 물체들은 그림자 X.
			for (UINT64 j = 0, size = pLightSphereData[i]->Meshes.size(); j < size; ++j)
			{
				Mesh* pCurMesh = pLightSphereData[i]->Meshes[j];
				MaterialConstant* pMeshMaterialConst = (MaterialConstant*)(pCurMesh->MaterialConstant.pData);
				pMeshMaterialConst->AlbedoFactor = Vector3(0.0f);
				pMeshMaterialConst->EmissionFactor = Vector3(1.0f, 1.0f, 0.0f);
			}

			pLightSphereData[i]->bIsVisible = true;
			pLightSphereData[i]->Name = "LightSphere" + std::to_string(i);
			pLightSphereData[i]->bIsPickable = false;

			m_RenderObjects.push_back(m_LightSpheres[i]); // 리스트에 등록.
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

		m_pGround = new Model(pResourceManager, { mesh });

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
		m_pSkybox = new Model(pResourceManager, { skyboxMeshInfo });
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
		m_pCharacter = new SkinnedMeshModel(pResourceManager, characterMeshInfo, aniData);
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
