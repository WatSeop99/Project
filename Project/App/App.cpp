#include "../pch.h"
#include "../Model/GeometryGenerator.h"
#include "App.h"

void App::Initialize()
{
	UINT64 totalRenderObjectCount = 0;

	initMainWidndow();
	initDirect3D();
	initPhysics();
	initExternalData(&totalRenderObjectCount);

	Renderer::InitialData initData =
	{
		&m_RenderObjects,
		&m_Lights,
		&m_LightSpheres,

		&m_EnvTexture, &m_IrradianceTexture, &m_SpecularTexture, &m_BRDFTexture,

		m_pMirror, &m_MirrorPlane,
	};
	Renderer::Initizlie(&initData);
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

			float frameTime = (float)m_Timer.GetElapsedSeconds();
			float frameChange = frameTime;
			UINT64 curTick = GetTickCount64();

			++s_FrameCount;

			Update(frameChange);
			s_PrevUpdateTick = curTick;
			Render();

			if (curTick - s_PrevFrameCheckTick > 1000)
			{
				s_PrevFrameCheckTick = curTick;

				WCHAR txt[64];
				swprintf_s(txt, L"DX12  %uFPS", s_FrameCount);
				SetWindowText(m_hMainWindow, txt);

				s_FrameCount = 0;
			}
		}
	}

	return (int)msg.wParam;
}

void App::Update(const float DELTA_TIME)
{
	Renderer::Update(DELTA_TIME);

	for (UINT64 i = 0, size = m_RenderObjects.size(); i < size; ++i)
	{
		Model* pModel = m_RenderObjects[i];

		switch (pModel->ModelType)
		{
			case RenderObjectType_DefaultType:
			case RenderObjectType_MirrorType:
				pModel->UpdateConstantBuffers();
				break;

			case RenderObjectType_SkinnedType:
			{
				SkinnedMeshModel* pCharacter = (SkinnedMeshModel*)pModel;
				int state = 0;
				int frame = 0;

				updateAnimationState(pCharacter, DELTA_TIME, &state, &frame);
				pCharacter->UpdateWorld(Matrix::CreateTranslation(m_pCharacter->MoveInfo.Position));
				pCharacter->UpdateAnimation(state, frame, DELTA_TIME);
				pCharacter->UpdateConstantBuffers();
			}
			break;

			default:
				break;
		}
	}
}

void App::Cleanup()
{
	fence();
	for (UINT i = 0; i < SWAP_CHAIN_FRAME_COUNT; ++i)
	{
		waitForFenceValue(m_LastFenceValues[i]);
	}

	for (UINT64 i = 0, size = m_RenderObjects.size(); i < size; ++i)
	{
		Model* pModel = m_RenderObjects[i];
		delete pModel;
	}
	m_RenderObjects.clear();
	m_Lights.clear();
	m_LightSpheres.clear();

	m_pCharacter = nullptr;
	m_pMirror = nullptr;
}

void App::initExternalData(UINT64* pTotalRenderObjectCount)
{
	_ASSERT(pTotalRenderObjectCount);

	Renderer* pRenderer = this;
	physx::PxPhysics* pPhysics = m_PhysicsManager.GetPhysics();

	m_Lights.resize(MAX_LIGHTS);
	m_LightSpheres.resize(MAX_LIGHTS);

	// 환경맵 텍스쳐 로드.
	{
		m_EnvTexture.InitializeWithDDS(pRenderer, L"./Assets/Textures/Cubemaps/HDRI/clear_pureskyEnvHDR.dds");
		m_IrradianceTexture.InitializeWithDDS(pRenderer, L"./Assets/Textures/Cubemaps/HDRI/clear_pureskyEnvHDR.dds");
		m_SpecularTexture.InitializeWithDDS(pRenderer, L"./Assets/Textures/Cubemaps/HDRI/clear_pureskyEnvHDR.dds");
		m_BRDFTexture.InitializeWithDDS(pRenderer, L"./Assets/Textures/Cubemaps/HDRI/clear_pureskyEnvHDR.dds");
	}

	// 환경 박스 초기화.
	{
		MeshInfo skyboxMeshInfo = INIT_MESH_INFO;
		MakeBox(&skyboxMeshInfo, 40.0f);

		std::reverse(skyboxMeshInfo.Indices.begin(), skyboxMeshInfo.Indices.end());
		Model* pSkybox = new Model(pRenderer, { skyboxMeshInfo });
		pSkybox->Name = "SkyBox";
		pSkybox->ModelType = RenderObjectType_SkyboxType;
		m_RenderObjects.push_back(pSkybox);
	}

	// 조명 설정.
	{
		// 조명 0.
		m_Lights[0].Property.Radiance = Vector3(3.0f);
		m_Lights[0].Property.FallOffEnd = 10.0f;
		m_Lights[0].Property.Position = Vector3(0.0f, 0.5f, -0.9f);
		m_Lights[0].Property.Direction = Vector3(0.0f, 0.0f, 1.0f);
		m_Lights[0].Property.SpotPower = 3.0f;
		m_Lights[0].Property.LightType = LIGHT_POINT | LIGHT_SHADOW;
		m_Lights[0].Property.Radius = 0.04f;
		m_Lights[0].Initialize(pRenderer);

		// 조명 1.
		m_Lights[1].Property.Radiance = Vector3(3.0f);
		m_Lights[1].Property.FallOffEnd = 10.0f;
		m_Lights[1].Property.Position = Vector3(1.0f, 1.1f, 2.0f);
		m_Lights[1].Property.SpotPower = 2.0f;
		m_Lights[1].Property.Direction = Vector3(0.0f, -0.5f, 1.7f) - m_Lights[1].Property.Position;
		m_Lights[1].Property.Direction.Normalize();
		m_Lights[1].Property.LightType = LIGHT_SPOT | LIGHT_SHADOW;
		m_Lights[1].Property.Radius = 0.02f;
		m_Lights[1].Initialize(pRenderer);

		// 조명 2.
		m_Lights[2].Property.Radiance = Vector3(5.0f);
		m_Lights[2].Property.Position = Vector3(5.0f, 5.0f, 5.0f);
		m_Lights[2].Property.Direction = Vector3(-1.0f, -1.0f, -1.0f);
		m_Lights[2].Property.Direction.Normalize();
		m_Lights[2].Property.LightType = LIGHT_DIRECTIONAL | LIGHT_SHADOW;
		m_Lights[2].Property.Radius = 0.05f;
		m_Lights[2].Initialize(pRenderer);
	}

	// 조명 위치 표시.
	{
		for (int i = 0; i < MAX_LIGHTS; ++i)
		{
			MeshInfo sphere = INIT_MESH_INFO;
			MakeSphere(&sphere, 1.0f, 20, 20);

			m_LightSpheres[i] = new Model(pRenderer, { sphere });
			m_LightSpheres[i]->UpdateWorld(Matrix::CreateTranslation(m_Lights[i].Property.Position));

			/*MaterialConstant* pSphereMaterialConst = (MaterialConstant*)m_LightSpheres[i]->Meshes[0]->MaterialConstant.pData;
			pSphereMaterialConst->AlbedoFactor = Vector3(0.0f);
			pSphereMaterialConst->EmissionFactor = Vector3(1.0f, 1.0f, 0.0f);*/
			MaterialConstant& sphereMaterialConstantData = m_LightSpheres[i]->Meshes[0]->MaterialConstantData;
			sphereMaterialConstantData.AlbedoFactor = Vector3(0.0f);
			sphereMaterialConstantData.EmissionFactor = Vector3(1.0f, 1.0f, 0.0f);
			m_LightSpheres[i]->bCastShadow = false; // 조명 표시 물체들은 그림자 X.
			for (UINT64 j = 0, size = m_LightSpheres[i]->Meshes.size(); j < size; ++j)
			{
				Mesh* pCurMesh = m_LightSpheres[i]->Meshes[j];
				/*MaterialConstant* pMeshMaterialConst = (MaterialConstant*)pCurMesh->MaterialConstant.pData;
				pMeshMaterialConst->AlbedoFactor = Vector3(0.0f);
				pMeshMaterialConst->EmissionFactor = Vector3(1.0f, 1.0f, 0.0f);*/
				MaterialConstant& meshMaterialConstantData = pCurMesh->MaterialConstantData;
				meshMaterialConstantData.AlbedoFactor = Vector3(0.0f);
				meshMaterialConstantData.EmissionFactor = Vector3(1.0f, 1.0f, 0.0f);
			}

			m_LightSpheres[i]->bIsVisible = true;
			m_LightSpheres[i]->Name = "LightSphere" + std::to_string(i);
			m_LightSpheres[i]->bIsPickable = false;

			m_RenderObjects.push_back(m_LightSpheres[i]);
		}
	}

	// 바닥(거울).
	{
		Model* pGround = nullptr;
		MeshInfo mesh = INIT_MESH_INFO;
		MakeSquare(&mesh, 10.0f);

		std::wstring path = L"./Assets/Textures/PBR/stringy-marble-ue/";
		mesh.szAlbedoTextureFileName = path + L"stringy_marble_albedo.png";
		mesh.szEmissiveTextureFileName = L"";
		mesh.szAOTextureFileName = path + L"stringy_marble_ao.png";
		mesh.szMetallicTextureFileName = path + L"stringy_marble_Metallic.png";
		mesh.szNormalTextureFileName = path + L"stringy_marble_Normal-dx.png";
		mesh.szRoughnessTextureFileName = path + L"stringy_marble_Roughness.png";

		pGround = new Model(pRenderer, { mesh });

		/*MaterialConstant* pGroundMaterialConst = (MaterialConstant*)pGround->Meshes[0]->MaterialConstant.pData;
		pGroundMaterialConst->AlbedoFactor = Vector3(0.7f);
		pGroundMaterialConst->EmissionFactor = Vector3(0.0f);
		pGroundMaterialConst->MetallicFactor = 0.5f;
		pGroundMaterialConst->RoughnessFactor = 0.3f;*/
		MaterialConstant& groundMaterialConstantData = pGround->Meshes[0]->MaterialConstantData;
		groundMaterialConstantData.AlbedoFactor = Vector3(0.7f);
		groundMaterialConstantData.EmissionFactor = Vector3(0.0f);
		groundMaterialConstantData.MetallicFactor = 0.5f;
		groundMaterialConstantData.RoughnessFactor = 0.3f;

		// Vector3 position = Vector3(0.0f, -1.0f, 0.0f);
		// Vector3 position = Vector3(0.0f, -0.5f, 0.0f);
		Vector3 position = Vector3(0.0f);
		pGround->UpdateWorld(Matrix::CreateRotationX(DirectX::XM_PI * 0.5f) * Matrix::CreateTranslation(position));
		pGround->bCastShadow = false; // 바닥은 그림자 만들기 생략.

		m_MirrorPlane = DirectX::SimpleMath::Plane(position, Vector3(0.0f, 1.0f, 0.0f));
		m_pMirror = pGround; // 바닥에 거울처럼 반사 구현.
		pGround->ModelType = RenderObjectType_MirrorType;
		m_RenderObjects.push_back(pGround);


		physx::PxRigidStatic* pGroundPlane = physx::PxCreatePlane(*pPhysics, physx::PxPlane(0.0f, 1.0f, 0.0f, 0.0f), *(m_PhysicsManager.pCommonMaterial));
		m_PhysicsManager.AddActor(pGroundPlane);
	}

	// Main Object.
	{
		std::wstring path = L"./Assets/";
		std::vector<std::wstring> clipNames =
		{
			L"CatwalkIdleTwistL.fbx", L"CatwalkIdleToWalkForward.fbx",
			L"CatwalkWalkForward.fbx", L"CatwalkWalkStopTwistL.fbx",
		};
		AnimationData animationData;

		std::wstring filename = L"Remy.fbx";
		std::vector<MeshInfo> characterMeshInfo;
		AnimationData characterDefaultAnimData;
		ReadAnimationFromFile(characterMeshInfo, characterDefaultAnimData, path, filename);

		// 애니메이션 클립들.
		for (UINT64 i = 0, size = clipNames.size(); i < size; ++i)
		{
			std::wstring& name = clipNames[i];
			std::vector<MeshInfo> animationMeshInfo;
			AnimationData animDataInClip;
			ReadAnimationFromFile(animationMeshInfo, animDataInClip, path, name);

			if (animationData.Clips.empty())
			{
				animationData = animDataInClip;
			}
			else
			{
				animationData.Clips.push_back(animDataInClip.Clips[0]);
			}
		}

		if (animationData.Clips.size() > 1)
		{
			m_pCharacter = new SkinnedMeshModel(pRenderer, characterMeshInfo, animationData);
		}
		else
		{
			m_pCharacter = new SkinnedMeshModel(pRenderer, characterMeshInfo, characterDefaultAnimData);
		}

		// Vector3 center(0.0f, 0.5f, 2.0f);
		Vector3 center(0.0f, 1.0f, 5.0f);
		for (UINT64 i = 0, size = m_pCharacter->Meshes.size(); i < size; ++i)
		{
			Mesh* pCurMesh = m_pCharacter->Meshes[i];
			/*MaterialConstant* pMaterialConst = (MaterialConstant*)pCurMesh->MaterialConstant.pData;

			pMaterialConst->AlbedoFactor = Vector3(1.0f);
			pMaterialConst->RoughnessFactor = 0.8f;
			pMaterialConst->MetallicFactor = 0.0f;*/
			MaterialConstant& materialConstantData = pCurMesh->MaterialConstantData;
			materialConstantData.AlbedoFactor = Vector3(1.0f);
			materialConstantData.RoughnessFactor = 0.8f;
			materialConstantData.MetallicFactor = 0.0f;

		}
		m_pCharacter->Name = "MainCharacter";
		m_pCharacter->bIsPickable = true;
		m_pCharacter->UpdateWorld(Matrix::CreateScale(1.0f) * Matrix::CreateTranslation(center));
		m_pCharacter->MoveInfo.Position = center;
		m_RenderObjects.push_back((Model*)m_pCharacter);


		physx::PxControllerManager* pControlManager = m_PhysicsManager.GetControllerManager();
		physx::PxCapsuleControllerDesc capsuleDesc;
		physx::PxController* pController = nullptr;

		// capsule 메시랑 동일하게 설정. 단, 여기서는 y값을 0으로 설정해줘야 함.
		capsuleDesc.height = (m_pCharacter->BoundingSphere.Radius * 1.35f) - 0.5f;
		capsuleDesc.radius = 0.25f;
		capsuleDesc.upDirection = physx::PxVec3(0.0f, 1.0f, 0.0f);
		// capsuleDesc.position = physx::PxExtendedVec3(m_pCharacter->MoveInfo.Position.x, m_pCharacter->MoveInfo.Position.y - 0.6f, m_pCharacter->MoveInfo.Position.z);
		capsuleDesc.position = physx::PxExtendedVec3(m_pCharacter->MoveInfo.Position.x, m_pCharacter->MoveInfo.Position.y, m_pCharacter->MoveInfo.Position.z);
		capsuleDesc.material = m_PhysicsManager.pCommonMaterial;
		capsuleDesc.stepOffset = (capsuleDesc.radius + capsuleDesc.height * 0.5f) * 0.5f;
		capsuleDesc.climbingMode = physx::PxCapsuleClimbingMode::eCONSTRAINED;
		pController = pControlManager->createController(capsuleDesc);
		if (!pController)
		{
			__debugbreak();
		}
		m_pCharacter->pController = pController;
	}

	// 경사면
	{
		Model* pSlope = nullptr;
		MeshInfo mesh = INIT_MESH_INFO;
		MakeSlope(&mesh, 20.0f, 1.5f);

		std::wstring path = L"./Assets/Textures/PBR/stringy-marble-ue/";
		mesh.szAlbedoTextureFileName = path + L"stringy_marble_albedo.png";
		mesh.szEmissiveTextureFileName = L"";
		mesh.szAOTextureFileName = path + L"stringy_marble_ao.png";
		mesh.szMetallicTextureFileName = path + L"stringy_marble_Metallic.png";
		mesh.szNormalTextureFileName = path + L"stringy_marble_Normal-dx.png";
		mesh.szRoughnessTextureFileName = path + L"stringy_marble_Roughness.png";

		pSlope = new Model(pRenderer, { mesh });

		/*MaterialConstant* pGroundMaterialConst = (MaterialConstant*)pSlope->Meshes[0]->MaterialConstant.pData;
		pGroundMaterialConst->AlbedoFactor = Vector3(0.7f);
		pGroundMaterialConst->EmissionFactor = Vector3(0.0f);
		pGroundMaterialConst->MetallicFactor = 0.5f;
		pGroundMaterialConst->RoughnessFactor = 0.3f;*/
		MaterialConstant& groundMaterialConstantData = pSlope->Meshes[0]->MaterialConstantData;
		groundMaterialConstantData.AlbedoFactor = Vector3(0.7f);
		groundMaterialConstantData.EmissionFactor = Vector3(0.0f);
		groundMaterialConstantData.MetallicFactor = 0.5f;
		groundMaterialConstantData.RoughnessFactor = 0.3f;

		// Vector3 position = Vector3(0.0f, -1.0f, 0.0f);
		Vector3 position(0.0f);
		Matrix newWorld = Matrix::CreateRotationY(90.0f * DirectX::XM_PI / 180.0f) * Matrix::CreateTranslation(position);
		pSlope->UpdateWorld(newWorld);

		pSlope->ModelType = RenderObjectType_DefaultType;
		m_RenderObjects.push_back(pSlope);

		// mesh.indices ==> right-hand coordinates에 맞춰 변경.
		mesh.Indices =
		{
			0, 2, 1, 1, 2, 3, // 하단면
			4, 5, 6, 5, 7, 6, // 상단면
			8, 10, 9, 11, 13, 12, // 양쪽면
			14, 17, 16, 14, 16, 15, // 뒷면
		};

		m_PhysicsManager.CookingStaticTriangleMesh(&mesh.Vertices, &mesh.Indices, pSlope->World);
	}
}

void App::updateAnimationState(SkinnedMeshModel* pCharacter, const float DELTA_TIME, int* pState, int* pFrame)
{
	// States
	// 0: idle
	// 1: idle to walk
	// 2: walk forward
	// 3: walk to stop
	static int s_State = 0;
	static int s_FrameCount = 0;

	// 별도의 애니메이션 클립이 없을 경우.
	/*if (m_pCharacter->CharacterAnimationData.Clips.size() == 1)
	{
		goto LB_IK_PROCESS;
	}*/

	_ASSERT(pCharacter);

	const UINT64 ANIMATION_CLIP_SIZE = pCharacter->CharacterAnimationData.Clips[s_State].Keys[0].size();
	switch (s_State)
	{
		case 0:
			if (m_Keyboard.bPressed[VK_UP])
			{
				// reset all update rot.
				pCharacter->CharacterAnimationData.ResetAllUpdateRotationInClip(s_State);

				s_State = 1;
				s_FrameCount = 0;
			}
			else if (s_FrameCount == ANIMATION_CLIP_SIZE || m_Keyboard.bPressed[VK_UP]) // 재생이 다 끝난다면.
			{
				s_FrameCount = 0; // 상태 변화 없이 반복.
			}
			break;

		case 1:
		{
			Vector3 deltaPos = pCharacter->MoveInfo.Direction * pCharacter->MoveInfo.Velocity * 0.5f * DELTA_TIME;
			pCharacter->MoveInfo.Position += deltaPos;
			deltaPos.y -= 0.4f;
			simulateCharacterContol(pCharacter, deltaPos, DELTA_TIME);

			if (s_FrameCount == ANIMATION_CLIP_SIZE)
			{
				// reset all update rot.
				pCharacter->CharacterAnimationData.ResetAllUpdateRotationInClip(s_State);

				s_State = 2;
				s_FrameCount = 0;
			}
		}
		break;

		case 2:
		{
			Vector3 deltaPos;

			// moveinfo.direction과 moveinfo.rotation을 오른쪽으로 같이 회전.
			if (m_Keyboard.bPressed[VK_RIGHT])
			{
				Quaternion newRot = Quaternion::CreateFromYawPitchRoll(DirectX::XM_PI * 60.0f / 180.0f * DELTA_TIME * 2.0f, 0.0f, 0.0f);
				pCharacter->MoveInfo.Direction = Vector3::TransformNormal(pCharacter->MoveInfo.Direction, Matrix::CreateFromQuaternion(newRot));
				pCharacter->MoveInfo.Rotation = Quaternion::Concatenate(pCharacter->MoveInfo.Rotation, newRot);

				/*pCharacter->CharacterAnimationData.AccumulatedRootTransform =
					Matrix::CreateRotationY(DirectX::XM_PI * 60.0f / 180.0f * DELTA_TIME * 2.0f) *
					pCharacter->CharacterAnimationData.AccumulatedRootTransform;*/
			}
			// moveinfo.direction과 moveinfo.rotation을 왼쪽으로 같이 회전.
			if (m_Keyboard.bPressed[VK_LEFT])
			{
				Quaternion newRot = Quaternion::CreateFromYawPitchRoll(-DirectX::XM_PI * 60.0f / 180.0f * DELTA_TIME * 2.0f, 0.0f, 0.0f);
				pCharacter->MoveInfo.Direction = Vector3::TransformNormal(pCharacter->MoveInfo.Direction, Matrix::CreateFromQuaternion(newRot));
				pCharacter->MoveInfo.Rotation = Quaternion::Concatenate(pCharacter->MoveInfo.Rotation, newRot);

				/*pCharacter->CharacterAnimationData.AccumulatedRootTransform =
					Matrix::CreateRotationY(-DirectX::XM_PI * 60.0f / 180.0f * DELTA_TIME * 2.0f) *
					pCharacter->CharacterAnimationData.AccumulatedRootTransform;*/
			}

			deltaPos = pCharacter->MoveInfo.Direction * pCharacter->MoveInfo.Velocity * DELTA_TIME;
			m_pCharacter->MoveInfo.Position += deltaPos;
			deltaPos.y -= 0.4f;
			simulateCharacterContol(pCharacter, deltaPos, DELTA_TIME);

			if (s_FrameCount == ANIMATION_CLIP_SIZE)
			{
				// 방향키를 누르고 있지 않으면 정지. (누르고 있으면 계속 걷기)
				if (!m_Keyboard.bPressed[VK_UP])
				{
					// reset all update rot.
					m_pCharacter->CharacterAnimationData.ResetAllUpdateRotationInClip(s_State);

					s_State = 3;
				}
				s_FrameCount = 0;
			}
		}
		break;

		case 3:
		{
			Vector3 deltaPos = pCharacter->MoveInfo.Direction * pCharacter->MoveInfo.Velocity * 0.5f * DELTA_TIME;
			pCharacter->MoveInfo.Position += deltaPos;
			deltaPos.y -= 0.4f;
			simulateCharacterContol(pCharacter, deltaPos, DELTA_TIME);

			if (s_FrameCount == ANIMATION_CLIP_SIZE)
			{
				// reset all update rot.
				pCharacter->CharacterAnimationData.ResetAllUpdateRotationInClip(s_State);

				s_State = 0;
				s_FrameCount = 0;
			}
		}
		break;

		default:
			break;
	}

	*pState = s_State;
	*pFrame = s_FrameCount;

	//LB_IK_PROCESS:
	//	if (!m_pPickedEndEffector)
	//	{
	//		goto LB_UPDATE;
	//	}
	//	m_pCharacter->UpdateCharacterIK(m_PickedTranslation, m_PickedEndEffectorType, s_State, s_FrameCount, DELTA_TIME);

	++s_FrameCount;
}

void App::simulateCharacterContol(SkinnedMeshModel* pCharacter, const Vector3& DELTA_POS, const float DELTA_TIME)
{
	_ASSERT(pCharacter);

	// 위치 변위.
	physx::PxVec3 displacement = physx::PxVec3(DELTA_POS.x, DELTA_POS.y, DELTA_POS.z);

	// physx 상에서 캐릭터 이동.
	physx::PxControllerCollisionFlags flags = pCharacter->pController->move(displacement, 0.001f, DELTA_TIME, physx::PxControllerFilters());

	// physx 상에서의 캐릭터 위치 받아오기.
	physx::PxExtendedVec3 nextPos = pCharacter->pController->getPosition();
	Vector3 nextPosVec((float)nextPos.x, (float)nextPos.y, (float)nextPos.z);

	// 받아온 위치 기반 캐릭터 위치 갱신.
	pCharacter->MoveInfo.Position = nextPosVec;
	pCharacter->MoveInfo.Position.y += 0.4f;
}
