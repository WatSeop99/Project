#include "../pch.h"
#include "../Model/GeometryGenerator.h"
#include "App.h"

void App::Initialize()
{
	Renderer::Initizlie();

	UINT64 totalRenderObjectCount = 0;
	initExternalData(&totalRenderObjectCount);

	m_pRenderObjects = &m_RenderObjects;
	m_pLights = &m_Lights;
	m_pLightSpheres = &m_LightSpheres;
	m_pMirrorPlane = &m_MirrorPlane;

	ResourceManager::TextureHandles textureHandles =
	{
		{ m_Lights[0].LightShadowMap.GetSpotLightShadowBufferPtr(),
		m_Lights[1].LightShadowMap.GetPointLightShadowBufferPtr(),
		m_Lights[2].LightShadowMap.GetDirectionalLightShadowBufferPtr() },
		m_pEnvTextureHandle,
		m_pIrradianceTextureHandle,
		m_pSpecularTextureHandle,
		m_pBRDFTextureHandle,
	};
	ResourceManager* pResourceManager = GetResourceManager();
	pResourceManager->SetGlobalTextures(&textureHandles);
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
			Render();

			s_PrevUpdateTick = curTick;
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

	m_pPhysicsManager->Update(DELTA_TIME);

	for (UINT64 i = 0, size = m_RenderObjects.size(); i < size; ++i)
	{
		const Model* pModel = m_RenderObjects[i];

		if (!pModel->bIsVisible)
		{
			continue;
		}

		switch (pModel->ModelType)
		{
			case RenderObjectType_DefaultType:
				break;

			case RenderObjectType_SkinnedType:
			{
				SkinnedMeshModel* pCharacter = (SkinnedMeshModel*)pModel;
				int state = 0;
				int frame = 0;
				Vector3 rightFootPos;
				Vector3 leftFootPos;
				SkinnedMeshModel::JointUpdateInfo updateInfo;

				ZeroMemory(&updateInfo, sizeof(SkinnedMeshModel::JointUpdateInfo));
				updateAnimationState(pCharacter, DELTA_TIME, &state, &frame, &updateInfo);

				pCharacter->UpdateWorld(Matrix::CreateTranslation(m_pCharacter->CharacterAnimationData.Position));
				pCharacter->UpdateAnimation(state, frame, DELTA_TIME, &updateInfo);
			}
			break;

			case RenderObjectType_MirrorType:
				break;

			default:
				break;
		}
	}

	//{
	//	physx::PxTransform characterPos = m_pCharacter->pBoundingCapsule->getGlobalPose();
	//	Vector3 posToVec3(characterPos.p.x, characterPos.p.y, characterPos.p.z);
	//	m_pCharacter->CharacterAnimationData.Position = posToVec3;
	//	// m_pCharacter->CharacterAnimationData.Position.y += 0.4f;
	//}
}

void App::Cleanup()
{
	Fence();
	for (UINT i = 0; i < SWAP_CHAIN_FRAME_COUNT; ++i)
	{
		WaitForFenceValue(m_LastFenceValues[i]);
	}

	for (UINT64 i = 0, size = m_RenderObjects.size(); i < size; ++i)
	{
		Model* pModel = m_RenderObjects[i];
		delete pModel;
	}
	m_RenderObjects.clear();
	m_Lights.clear();
	m_LightSpheres.clear();

	TextureManager* pTextureManager = GetTextureManager();
	if (m_pEnvTextureHandle)
	{
		pTextureManager->DeleteTexture(m_pEnvTextureHandle);
		m_pEnvTextureHandle = nullptr;
	}
	if (m_pIrradianceTextureHandle)
	{
		pTextureManager->DeleteTexture(m_pIrradianceTextureHandle);
		m_pIrradianceTextureHandle = nullptr;
	}
	if (m_pSpecularTextureHandle)
	{
		pTextureManager->DeleteTexture(m_pSpecularTextureHandle);
		m_pSpecularTextureHandle = nullptr;
	}
	if (m_pBRDFTextureHandle)
	{
		pTextureManager->DeleteTexture(m_pBRDFTextureHandle);
		m_pBRDFTextureHandle = nullptr;
	}

	m_pCharacter = nullptr;
	m_pMirror = nullptr;
}

void App::initExternalData(UINT64* pTotalRenderObjectCount)
{
	_ASSERT(pTotalRenderObjectCount);
	_ASSERT(m_pPhysicsManager);

	physx::PxPhysics* pPhysics = m_pPhysicsManager->GetPhysics();

	m_Lights.resize(MAX_LIGHTS);
	m_LightSpheres.resize(MAX_LIGHTS);

	// 환경맵 텍스쳐 로드.
	TextureManager* pTextureManager = GetTextureManager();
	m_pEnvTextureHandle = pTextureManager->CreateTexturFromDDSFile(L"./Assets/Textures/Cubemaps/HDRI/clear_pureskyEnvHDR.dds", true);
	m_pIrradianceTextureHandle = pTextureManager->CreateTexturFromDDSFile(L"./Assets/Textures/Cubemaps/HDRI/clear_pureskySpecularHDR.dds", true);
	m_pSpecularTextureHandle = pTextureManager->CreateTexturFromDDSFile(L"./Assets/Textures/Cubemaps/HDRI/clear_pureskyDiffuseHDR.dds", true);
	m_pBRDFTextureHandle = pTextureManager->CreateTexturFromDDSFile(L"./Assets/Textures/Cubemaps/HDRI/clear_pureskyBrdf.dds", false);

	// 환경 박스 초기화.
	{
		MeshInfo skyboxMeshInfo = INIT_MESH_INFO;
		MakeBox(&skyboxMeshInfo, 40.0f);

		std::reverse(skyboxMeshInfo.Indices.begin(), skyboxMeshInfo.Indices.end());
		Model* pSkybox = new Model(this, { skyboxMeshInfo });
		pSkybox->Name = "SkyBox";
		pSkybox->ModelType = RenderObjectType_SkyboxType;
		m_RenderObjects.push_back(pSkybox);
	}

	// 조명 설정.
	{
		// 조명 0.
		Light& light0 = m_Lights[0];
		light0.Property.Radiance = Vector3(3.0f);
		light0.Property.FallOffEnd = 10.0f;
		light0.Property.Position = Vector3(0.0f, 0.5f, -0.9f);
		light0.Property.Direction = Vector3(0.0f, 0.0f, 1.0f);
		light0.Property.SpotPower = 3.0f;
		light0.Property.LightType = LIGHT_POINT | LIGHT_SHADOW;
		light0.Property.Radius = 0.04f;
		light0.Initialize(this);

		// 조명 1.
		Light& light1 = m_Lights[1];
		light1.Property.Radiance = Vector3(3.0f);
		light1.Property.FallOffEnd = 10.0f;
		light1.Property.Position = Vector3(1.0f, 1.1f, 2.0f);
		light1.Property.SpotPower = 2.0f;
		light1.Property.Direction = Vector3(0.0f, -0.5f, 1.7f) - m_Lights[1].Property.Position;
		light1.Property.Direction.Normalize();
		light1.Property.LightType = LIGHT_SPOT | LIGHT_SHADOW;
		light1.Property.Radius = 0.02f;
		light1.Initialize(this);

		// 조명 2.
		Light& light2 = m_Lights[2];
		light2.Property.Radiance = Vector3(5.0f);
		light2.Property.Position = Vector3(5.0f, 5.0f, 5.0f);
		light2.Property.Direction = Vector3(-1.0f, -1.0f, -1.0f);
		light2.Property.Direction.Normalize();
		light2.Property.LightType = LIGHT_DIRECTIONAL | LIGHT_SHADOW;
		light2.Property.Radius = 0.05f;
		light2.Initialize(this);
	}

	// 조명 위치 표시.
	for (int i = 0; i < MAX_LIGHTS; ++i)
	{
		MeshInfo sphere = INIT_MESH_INFO;
		MakeSphere(&sphere, 1.0f, 20, 20);

		m_LightSpheres[i] = new Model(this, { sphere });
		m_LightSpheres[i]->UpdateWorld(Matrix::CreateTranslation(m_Lights[i].Property.Position));

		MaterialConstant& sphereMaterialConstantData = m_LightSpheres[i]->Meshes[0]->MaterialConstantData;
		sphereMaterialConstantData.AlbedoFactor = Vector3(0.0f);
		sphereMaterialConstantData.EmissionFactor = Vector3(1.0f, 1.0f, 0.0f);
		m_LightSpheres[i]->bCastShadow = false; // 조명 표시 물체들은 그림자 X.
		for (UINT64 j = 0, size = m_LightSpheres[i]->Meshes.size(); j < size; ++j)
		{
			Mesh* pCurMesh = m_LightSpheres[i]->Meshes[j];
			MaterialConstant& meshMaterialConstantData = pCurMesh->MaterialConstantData;
			meshMaterialConstantData.AlbedoFactor = Vector3(0.0f);
			meshMaterialConstantData.EmissionFactor = Vector3(1.0f, 1.0f, 0.0f);
		}

		m_LightSpheres[i]->bIsVisible = true;
		m_LightSpheres[i]->Name = "LightSphere" + std::to_string(i);
		m_LightSpheres[i]->bIsPickable = false;

		m_RenderObjects.push_back(m_LightSpheres[i]);
	}

	// 공용 global constant 설정.
	m_GlobalConstantData.StrengthIBL = 0.3f;
	for (int i = 0; i < MAX_LIGHTS; ++i)
	{
		memcpy(&m_LightConstantData.Lights[i], &m_Lights[i].Property, sizeof(LightProperty));
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

		pGround = new Model(this, { mesh });

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

		physx::PxRigidStatic* pGroundPlane = physx::PxCreatePlane(*pPhysics, physx::PxPlane(0.0f, 1.0f, 0.0f, 0.0f), *(m_pPhysicsManager->pCommonMaterial));
		{
			physx::PxFilterData groundFilterData;
			groundFilterData.word0 = CollisionGroup_Default;

			physx::PxU32 numShapes = pGroundPlane->getNbShapes();
			std::vector<physx::PxShape*> shapes(numShapes);
			pGroundPlane->getShapes(shapes.data(), numShapes);
			for (physx::PxU32 i = 0; i < numShapes; ++i)
			{
				physx::PxShape* pShape = shapes[i];
				pShape->setSimulationFilterData(groundFilterData);
			}
		}
		m_pPhysicsManager->AddActor(pGroundPlane);
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
			m_pCharacter = new SkinnedMeshModel(this, characterMeshInfo, animationData);
		}
		else
		{
			m_pCharacter = new SkinnedMeshModel(this, characterMeshInfo, characterDefaultAnimData);
		}

		// Vector3 center(0.0f, 0.5f, 2.0f);
		Vector3 center(0.0f, 1.0f, 5.0f);
		for (UINT64 i = 0, size = m_pCharacter->Meshes.size(); i < size; ++i)
		{
			Mesh* pCurMesh = m_pCharacter->Meshes[i];
			MaterialConstant& materialConstantData = pCurMesh->MaterialConstantData;
			materialConstantData.AlbedoFactor = Vector3(1.0f);
			materialConstantData.RoughnessFactor = 0.8f;
			materialConstantData.MetallicFactor = 0.0f;

		}
		m_pCharacter->Name = "MainCharacter";
		m_pCharacter->bIsPickable = true;
		m_pCharacter->UpdateWorld(Matrix::CreateScale(1.0f) * Matrix::CreateTranslation(center));
		m_pCharacter->CharacterAnimationData.Position = center;
		m_RenderObjects.push_back((Model*)m_pCharacter);


		physx::PxControllerManager* pControlManager = m_pPhysicsManager->GetControllerManager();
		physx::PxCapsuleController* pController = nullptr;
		physx::PxCapsuleControllerDesc capsuleDesc;

		// capsule 메시랑 동일하게 설정. 단, 여기서는 y값을 0으로 설정해줘야 함.
		capsuleDesc.height = (m_pCharacter->BoundingSphere.Radius * 1.35f) - 0.5f;
		capsuleDesc.radius = 0.25f;
		capsuleDesc.upDirection = physx::PxVec3(0.0f, 1.0f, 0.0f);
		capsuleDesc.position = physx::PxExtendedVec3(m_pCharacter->CharacterAnimationData.Position.x, m_pCharacter->CharacterAnimationData.Position.y, m_pCharacter->CharacterAnimationData.Position.z);
		capsuleDesc.contactOffset = 0.05f;
		capsuleDesc.material = m_pPhysicsManager->pCommonMaterial;
		capsuleDesc.stepOffset = (capsuleDesc.radius + capsuleDesc.height * 0.5f) * 0.2f;
		capsuleDesc.climbingMode = physx::PxCapsuleClimbingMode::eCONSTRAINED;
		pController = (physx::PxCapsuleController*)pControlManager->createController(capsuleDesc);
		if (!pController)
		{
			__debugbreak();
		}
		{
			physx::PxFilterData controllerFilter = {};
			controllerFilter.word0 = CollisionGroup_KinematicBody;

			physx::PxRigidDynamic* pCapsuleActor = pController->getActor();
			physx::PxU32 numShapes = pCapsuleActor->getNbShapes();
			std::vector<physx::PxShape*> capsuleShapes(numShapes);
			pCapsuleActor->getShapes(capsuleShapes.data(), numShapes);

			for (physx::PxU32 i = 0; i < numShapes; ++i)
			{
				physx::PxShape* pShape = capsuleShapes[i];
				pShape->setSimulationFilterData(controllerFilter);
				pShape->setQueryFilterData(controllerFilter);
			}
		}


		// end-effector 충돌체 설정.
		physx::PxRigidDynamic* pRightFoot = nullptr;
		physx::PxRigidDynamic* pLeftFoot = nullptr;
		physx::PxBoxGeometry boundingBoxGeom(0.2f, 0.2f, 0.2f);
		physx::PxShape* pBoxShape = pPhysics->createShape(boundingBoxGeom, *(m_pPhysicsManager->pCommonMaterial));
		if (!pBoxShape)
		{
			__debugbreak();
		}

		physx::PxFilterData endEffectorFilter = {};
		endEffectorFilter.word0 = CollisionGroup_EndEffector;
		pBoxShape->setSimulationFilterData(endEffectorFilter);
		pBoxShape->setQueryFilterData(endEffectorFilter);

		pRightFoot = pPhysics->createRigidDynamic(physx::PxTransform(0.0f, 0.0f, 3.0f));
		if (!pRightFoot)
		{
			__debugbreak();
		}
		pRightFoot->attachShape(*pBoxShape);

		pLeftFoot = pPhysics->createRigidDynamic(physx::PxTransform(0.0f, 0.0f, 3.0f));
		if (!pLeftFoot)
		{
			__debugbreak();
		}
		pLeftFoot->attachShape(*pBoxShape);

		m_pPhysicsManager->AddActor(pRightFoot);
		m_pPhysicsManager->AddActor(pLeftFoot);
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

		pSlope = new Model(this, { mesh });

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
			14, 16, 17, 15, 14, 17, // 뒷면
		};

		m_pPhysicsManager->CookingStaticTriangleMesh(&mesh.Vertices, &mesh.Indices, pSlope->World);
	}
}

void App::updateAnimationState(SkinnedMeshModel* pCharacter, const float DELTA_TIME, int* pState, int* pFrame, SkinnedMeshModel::JointUpdateInfo* pUpdateInfo)
{
	// States
	// 0: idle
	// 1: idle to walk
	// 2: walk forward
	// 3: walk to stop
	static int s_State = 0;
	static int s_FrameCount = 0;

	_ASSERT(pCharacter);
	_ASSERT(pUpdateInfo);

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
			Vector3 deltaPos = pCharacter->CharacterAnimationData.Direction * pCharacter->CharacterAnimationData.Velocity * DELTA_TIME;
			pCharacter->CharacterAnimationData.Position += deltaPos;
			deltaPos.y -= 0.4f;
			simulateCharacterContol(pCharacter, pUpdateInfo, deltaPos, DELTA_TIME);

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
				pCharacter->CharacterAnimationData.Direction = Vector3::TransformNormal(pCharacter->CharacterAnimationData.Direction, Matrix::CreateFromQuaternion(newRot));
				pCharacter->CharacterAnimationData.Rotation = Quaternion::Concatenate(pCharacter->CharacterAnimationData.Rotation, newRot);
			}
			// moveinfo.direction과 moveinfo.rotation을 왼쪽으로 같이 회전.
			if (m_Keyboard.bPressed[VK_LEFT])
			{
				Quaternion newRot = Quaternion::CreateFromYawPitchRoll(-DirectX::XM_PI * 60.0f / 180.0f * DELTA_TIME * 2.0f, 0.0f, 0.0f);
				pCharacter->CharacterAnimationData.Direction = Vector3::TransformNormal(pCharacter->CharacterAnimationData.Direction, Matrix::CreateFromQuaternion(newRot));
				pCharacter->CharacterAnimationData.Rotation = Quaternion::Concatenate(pCharacter->CharacterAnimationData.Rotation, newRot);
			}

			deltaPos = pCharacter->CharacterAnimationData.Direction * pCharacter->CharacterAnimationData.Velocity * DELTA_TIME;
			m_pCharacter->CharacterAnimationData.Position += deltaPos;
			deltaPos.y -= 0.4f;
			simulateCharacterContol(pCharacter, pUpdateInfo, deltaPos, DELTA_TIME);

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
			Vector3 deltaPos = pCharacter->CharacterAnimationData.Direction * pCharacter->CharacterAnimationData.Velocity * DELTA_TIME;
			pCharacter->CharacterAnimationData.Position += deltaPos;
			deltaPos.y -= 0.4f;
			simulateCharacterContol(pCharacter, pUpdateInfo, deltaPos, DELTA_TIME);

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
	++s_FrameCount;
}

void App::simulateCharacterContol(SkinnedMeshModel* pCharacter, SkinnedMeshModel::JointUpdateInfo* pUpdateInfo, const Vector3& DELTA_POS, const float DELTA_TIME)
{
	_ASSERT(pCharacter);
	_ASSERT(pUpdateInfo);

	// 위치 변위.
	physx::PxVec3 displacement = physx::PxVec3(DELTA_POS.x, DELTA_POS.y, DELTA_POS.z);

	// physx 상에서 캐릭터 이동.
	physx::PxControllerCollisionFlags flags = pCharacter->pController->move(displacement, 0.001f, DELTA_TIME, pCharacter->CharacterControllerFilter);
	/*physx::PxTransform destinationPos(pCharacter->CharacterAnimationData.Position.x, pCharacter->CharacterAnimationData.Position.y, pCharacter->CharacterAnimationData.Position.z);
	{
		char szDebugString[256];
		sprintf_s(szDebugString, 256, "pos: %f, %f, %f\n", pCharacter->CharacterAnimationData.Position.x, pCharacter->CharacterAnimationData.Position.y, pCharacter->CharacterAnimationData.Position.z);
		OutputDebugStringA(szDebugString);
	}*/
	// pCharacter->pBoundingCapsule->setKinematicTarget(destinationPos);
	// pCharacter->pBoundingCapsule->setGlobalPose(destinationPos);

	// physx 상에서의 캐릭터 위치 받아오기.
	const float TO_RADIAN = DirectX::XM_PI / 180.0f;
	Vector3 rotatedRight = Vector3::Transform(pCharacter->CharacterAnimationData.Direction, Matrix::CreateRotationY(89.0f * TO_RADIAN));
	Vector3 rotatedLeft = Vector3::Transform(pCharacter->CharacterAnimationData.Direction, Matrix::CreateRotationY(-89.0f * TO_RADIAN));

	physx::PxExtendedVec3 bottomPos = pCharacter->pController->getFootPosition();
	Vector3 footPos = Vector3((float)bottomPos.x, (float)bottomPos.y, (float)bottomPos.z);
	pUpdateInfo->bUpdatedJointParts[SkinnedMeshModel::JointPart_RightLeg] = true;
	pUpdateInfo->bUpdatedJointParts[SkinnedMeshModel::JointPart_LeftLeg] = true;
	pUpdateInfo->EndEffectorTargetPoses[SkinnedMeshModel::JointPart_RightLeg] = footPos + rotatedRight * 0.11f; // 0.2f == radius;
	pUpdateInfo->EndEffectorTargetPoses[SkinnedMeshModel::JointPart_LeftLeg] = footPos + rotatedLeft * 0.11f; // 0.2f == radius;

	 physx::PxExtendedVec3 nextPos = pCharacter->pController->getPosition();
	 // physx::PxTransform nextPos = pCharacter->pBoundingCapsule->getGlobalPose();
	 Vector3 nextPosVec((float)nextPos.x, (float)nextPos.y, (float)nextPos.z);
	 // Vector3 nextPosVec((float)nextPos.p.x, (float)nextPos.p.y, (float)nextPos.p.z);

	 // 받아온 위치 기반 캐릭터 위치 갱신.
	 pCharacter->CharacterAnimationData.Position = nextPosVec;
	 pCharacter->CharacterAnimationData.Position.y += 0.4f;
}
