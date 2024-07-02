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

int App::Run()
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
			static HWND s_hMainWindow = m_pRenderer->GetWindow();

			float frameTime = (float)m_Timer.GetElapsedSeconds();
			float frameChange = 2.0f * frameTime;
			UINT64 curTick = GetTickCount64();

			++s_FrameCount;

			Update(frameChange);
			s_PrevUpdateTick = curTick;
			m_pRenderer->Render();

			if (curTick - s_PrevFrameCheckTick > 1000)
			{
				s_PrevFrameCheckTick = curTick;

				WCHAR txt[64];
				swprintf_s(txt, L"DX12  %uFPS", s_FrameCount);
				SetWindowText(s_hMainWindow, txt);

				s_FrameCount = 0;
			}
		}
	}

	return (int)msg.wParam;
}

void App::Update(const float DELTA_TIME)
{
	m_pRenderer->Update(DELTA_TIME);
	if (m_pMirror)
	{
		m_pMirror->UpdateConstantBuffers();
	}

	Model* pModel = (Model*)m_pRenderObjectsHead->pItem;
	while (pModel)
	{
		pModel->UpdateConstantBuffers();
		pModel = (Model*)pModel->LinkInGame.pNext->pItem;
	}

	m_pCharacter->UpdateConstantBuffers();
	updateAnimation(DELTA_TIME);
}

void App::Clear()
{
	if (m_Lights)
	{
		delete m_Lights;
		m_Lights = nullptr;
	}
	if (m_LightSpheres)
	{
		delete m_LightSpheres;
		m_LightSpheres = nullptr;
	}

	while (m_pRenderObjectsHead)
	{
		Model* pElem = (Model*)m_pRenderObjectsHead->pItem;
		UnLinkElemFromList(&m_pRenderObjectsHead, &m_pRenderObjectsTail, &pElem->LinkInGame);
		delete pElem;
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
			pLightSphereData[i]->UpdateWorld(Matrix::CreateTranslation(pLightData[i].Property.Position));

			MaterialConstant* pSphereMaterialConst = (MaterialConstant*)pLightSphereData[i]->Meshes[0]->MaterialConstant.pData;
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

			LinkElemIntoListFIFO(&m_pRenderObjectsHead, &m_pRenderObjectsTail, &pLightSphereData[i]->LinkInGame);
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
	}
}

void App::updateAnimation(const float DELTA_TIME)
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
