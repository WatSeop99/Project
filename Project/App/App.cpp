#include "../pch.h"
#include "../Physics/CustomFilterCallback.h"
#include "../Model/GeometryGenerator.h"
#include "App.h"

void App::Initialize()
{
	Renderer::Initizlie();
	initExternalData();

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
			// float frameChange = frameTime;
			UINT64 curTick = GetTickCount64();

			++s_FrameCount;

			Update(frameTime);
			Render();

			s_PrevUpdateTick = curTick;
			if (curTick - s_PrevFrameCheckTick > 1000)
			{
				s_PrevFrameCheckTick = curTick;

				WCHAR txt[64];
				swprintf_s(txt, 64, L"DX12  %uFPS", s_FrameCount);
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

	int state = 0;
	int frame = 0;
	for (UINT64 i = 0, size = m_Characters.size(); i < size; ++i)
	{
		SkinnedMeshModel* pCharacter = m_Characters[i];
		Vector3 deltaPos;
		updateAnimationState(pCharacter, DELTA_TIME, &state, &frame, &deltaPos);
		simulateCharacterContol(pCharacter, deltaPos, DELTA_TIME, state, frame);
	}

	m_pPhysicsManager->Update(DELTA_TIME);

	for (UINT64 i = 0, size = m_Characters.size(); i < size; ++i)
	{
		SkinnedMeshModel* pCharacter = m_Characters[i];
		SkinnedMeshModel::JointUpdateInfo updateInfo;

		ZeroMemory(&updateInfo, sizeof(SkinnedMeshModel::JointUpdateInfo));
		updateEndEffectorPosition(pCharacter, &updateInfo);

		pCharacter->UpdateWorld(Matrix::CreateTranslation(m_pCharacter->CharacterAnimationData.Position));
		pCharacter->UpdateAnimation(state, frame, DELTA_TIME, &updateInfo);
	}

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

			case RenderObjectType_MirrorType:
				break;

			default:
				break;
		}
	}
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

void App::initExternalData()
{
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
		
		Model* pSkybox = new Model;
		pSkybox->Initialize(this, { skyboxMeshInfo });
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

		m_LightSpheres[i] = new Model;
		m_LightSpheres[i]->Initialize(this, { sphere });
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
		/*std::wstring path = L"./Assets/Textures/PBR/patterned_wooden_wall_panels_48_05_8K/";
		mesh.szAlbedoTextureFileName = path + L"patterned_wooden_wall_panels_48_05_diffuse.jpg";
		mesh.szEmissiveTextureFileName = L"";
		mesh.szAOTextureFileName = path + L"patterned_wooden_wall_panels_48_05_ao.jpg";
		mesh.szMetallicTextureFileName = path + L"patterned_wooden_wall_panels_48_05_metallic.jpg";
		mesh.szNormalTextureFileName = path + L"patterned_wooden_wall_panels_48_05_normal.jpg";
		mesh.szRoughnessTextureFileName = path + L"patterned_wooden_wall_panels_48_05_roughness.jpg";*/

		pGround = new Model;
		pGround->Initialize(this, { mesh });

		MaterialConstant& groundMaterialConstantData = pGround->Meshes[0]->MaterialConstantData;
		groundMaterialConstantData.AlbedoFactor = Vector3(0.7f);
		groundMaterialConstantData.EmissionFactor = Vector3(0.0f);
		groundMaterialConstantData.MetallicFactor = 0.5f;
		groundMaterialConstantData.RoughnessFactor = 0.3f;

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
		eCollisionGroup type = CollisionGroup_Default;
		pGroundPlane->userData = malloc(sizeof(eCollisionGroup));
		memcpy(pGroundPlane->userData, &type, sizeof(eCollisionGroup));
		m_pPhysicsManager->AddActor(pGroundPlane);
	}

	// Main Object.
	{
		std::wstring path = L"./Assets/";
		// std::wstring path = L"./Assets/other2/";
		std::vector<std::wstring> clipNames =
		{
			L"CatwalkIdleTwistL.fbx", L"CatwalkIdleToWalkForward.fbx",
			L"CatwalkWalkForward.fbx", L"CatwalkWalkStopTwistL.fbx",
		};
		AnimationData animationData;

		std::wstring filename = L"Remy.fbx";
		// std::wstring filename = L"character.fbx";
		std::vector<MeshInfo> characterMeshInfo;
		AnimationData characterDefaultAnimData;
		ReadAnimationFromFile(characterMeshInfo, characterDefaultAnimData, path, filename);
		// characterDefaultAnimData.Clips[0].Name = "DefaultClip";

		// 애니메이션 클립들.
		for (UINT64 i = 0, size = clipNames.size(); i < size; ++i)
		{
			std::wstring& name = clipNames[i];
			std::vector<MeshInfo> animationMeshInfo;
			AnimationData animDataInClip;
			ReadAnimationFromFile(animationMeshInfo, animDataInClip, path, name);
			animDataInClip.Clips[0].Name.assign(name.begin(), name.end());
			if (animationData.Clips.empty())
			{
				animationData = animDataInClip;
			}
			else
			{
				animationData.Clips.push_back(animDataInClip.Clips[0]);
			}
		}

		m_pCharacter = new SkinnedMeshModel;
		if (animationData.Clips.size() > 1)
		{
			m_pCharacter->Initialize(this, characterMeshInfo, animationData);
		}
		else
		{
			m_pCharacter->Initialize(this, characterMeshInfo, characterDefaultAnimData);
		}


		// Vector3 position(0.0f, 0.5f, 2.0f);
		Vector3 position(0.0f, 0.5f, 5.0f);
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
		m_pCharacter->UpdateWorld(Matrix::CreateTranslation(position));
		m_pCharacter->CharacterAnimationData.Position = position;
		m_RenderObjects.push_back((Model*)m_pCharacter);
		m_Characters.push_back(m_pCharacter);


		physx::PxControllerManager* pControlManager = m_pPhysicsManager->GetControllerManager();
		physx::PxCapsuleController* pController = nullptr;

		// capsule 메시랑 동일하게 설정.
		physx::PxCapsuleControllerDesc capsuleDesc;
		capsuleDesc.height = (m_pCharacter->BoundingSphere.Radius * 1.3f) - 0.4f;
		capsuleDesc.radius = 0.2f;
		capsuleDesc.upDirection = physx::PxVec3(0.0f, 1.0f, 0.0f);
		capsuleDesc.position = physx::PxExtendedVec3(m_pCharacter->CharacterAnimationData.Position.x, m_pCharacter->CharacterAnimationData.Position.y, m_pCharacter->CharacterAnimationData.Position.z);
		// capsuleDesc.position = physx::PxExtendedVec3(m_pCharacter->CharacterAnimationData.Position.x, 2.0f * (capsuleDesc.height + capsuleDesc.radius) - center.y - 0.15f, m_pCharacter->CharacterAnimationData.Position.z);
		capsuleDesc.contactOffset = 0.0001f;
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

			eCollisionGroup type = CollisionGroup_KinematicBody;
			pCapsuleActor->userData = malloc(sizeof(eCollisionGroup));
			memcpy(pCapsuleActor->userData, &type, sizeof(eCollisionGroup));
		}


		// end-effector 충돌체 설정.
		physx::PxRigidDynamic* pRightFoot = nullptr;
		physx::PxRigidDynamic* pLeftFoot = nullptr;
		physx::PxBoxGeometry boundingBoxGeom(0.025f, 0.025f, 0.025f);
		physx::PxShape* pBoxShape = pPhysics->createShape(boundingBoxGeom, *(m_pPhysicsManager->pCommonMaterial));
		if (!pBoxShape)
		{
			__debugbreak();
		}

		eCollisionGroup type = CollisionGroup_EndEffector;
		physx::PxFilterData endEffectorFilter = {};
		endEffectorFilter.word0 = type;
		pBoxShape->setSimulationFilterData(endEffectorFilter);
		pBoxShape->setQueryFilterData(endEffectorFilter);

		physx::PxTransform transform1;
		physx::PxTransform transform2;
		{
			// Matrix correction = Matrix::CreateTranslation(Vector3(-0.23f, 0.0f, 0.0f));

			// Matrix forTransform1 = (correction * m_pCharacter->RightLeg.BodyChain[3].Correction * m_pCharacter->CharacterAnimationData.Get(m_pCharacter->RightLeg.BodyChain[3].BoneID) * m_pCharacter->World);
			// Matrix forTransform2 = (correction * m_pCharacter->LeftLeg.BodyChain[3].Correction * m_pCharacter->CharacterAnimationData.Get(m_pCharacter->LeftLeg.BodyChain[3].BoneID) * m_pCharacter->World);
			Matrix forTransform1 = (m_pCharacter->CharacterAnimationData.Get(m_pCharacter->RightLeg.BodyChain[3].BoneID) * m_pCharacter->World);
			Matrix forTransform2 = (m_pCharacter->CharacterAnimationData.Get(m_pCharacter->LeftLeg.BodyChain[3].BoneID) * m_pCharacter->World);

			Vector3 transform1Pos = forTransform1.Translation();
			Vector3 transform2Pos = forTransform2.Translation();
			Quaternion transform1Quat = Quaternion::CreateFromRotationMatrix(forTransform1);
			Quaternion transform2Quat = Quaternion::CreateFromRotationMatrix(forTransform2);

			transform1 = physx::PxTransform(physx::PxVec3(transform1Pos.x, transform1Pos.y, transform1Pos.z), physx::PxQuat(transform1Quat.x, transform1Quat.y, transform1Quat.z, transform1Quat.w));
			transform2 = physx::PxTransform(physx::PxVec3(transform2Pos.x, transform2Pos.y, transform2Pos.z), physx::PxQuat(transform2Quat.x, transform2Quat.y, transform2Quat.z, transform2Quat.w));
		}
		pRightFoot = pPhysics->createRigidDynamic(transform1);
		if (!pRightFoot)
		{
			__debugbreak();
		}
		pRightFoot->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, true);
		pRightFoot->attachShape(*pBoxShape);
		pRightFoot->userData = malloc(sizeof(eCollisionGroup));
		memcpy(pRightFoot->userData, &type, sizeof(eCollisionGroup));

		pLeftFoot = pPhysics->createRigidDynamic(transform2);
		if (!pLeftFoot)
		{
			__debugbreak();
		}
		pLeftFoot->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, true);
		pLeftFoot->attachShape(*pBoxShape);
		pLeftFoot->userData = malloc(sizeof(eCollisionGroup));
		memcpy(pLeftFoot->userData, &type, sizeof(eCollisionGroup));

		m_pPhysicsManager->AddActor(pRightFoot);
		m_pPhysicsManager->AddActor(pLeftFoot);
		m_pCharacter->pController = pController;
		m_pCharacter->pRightFoot = pRightFoot;
		m_pCharacter->pLeftFoot = pLeftFoot;
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

		pSlope = new Model;
		pSlope->Initialize(this, { mesh });

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
			0, 2, 1, 1, 2, 3,		// 하단면
			4, 5, 6, 5, 7, 6,		// 상단면
			8, 10, 9, 11, 13, 12,	// 양쪽면
			14, 16, 17, 15, 14, 17, // 뒷면
		};

		Matrix world = Matrix::CreateRotationY(-90.0f * DirectX::XM_PI / 180.0f) * Matrix::CreateTranslation(position);
		m_pPhysicsManager->CookingStaticTriangleMesh(&mesh.Vertices, &mesh.Indices, world);
	}

	// 계단
	{
		Model* pStair = nullptr;
		MeshInfo mesh = INIT_MESH_INFO;
		MakeStair(&mesh, 5, 1.0f, 0.1f, 0.2f);

		std::wstring path = L"./Assets/Textures/PBR/stringy-marble-ue/";
		mesh.szAlbedoTextureFileName = path + L"stringy_marble_albedo.png";
		mesh.szEmissiveTextureFileName = L"";
		mesh.szAOTextureFileName = path + L"stringy_marble_ao.png";
		mesh.szMetallicTextureFileName = path + L"stringy_marble_Metallic.png";
		mesh.szNormalTextureFileName = path + L"stringy_marble_Normal-dx.png";
		mesh.szRoughnessTextureFileName = path + L"stringy_marble_Roughness.png";

		pStair = new Model;
		pStair->Initialize(this, { mesh });

		MaterialConstant& groundMaterialConstantData = pStair->Meshes[0]->MaterialConstantData;
		groundMaterialConstantData.AlbedoFactor = Vector3(0.7f);
		groundMaterialConstantData.EmissionFactor = Vector3(0.0f);
		groundMaterialConstantData.MetallicFactor = 0.5f;
		groundMaterialConstantData.RoughnessFactor = 0.3f;

		// Vector3 position = Vector3(0.0f, -1.0f, 0.0f);
		Vector3 position(0.0f, 0.0f, -3.0f);
		Matrix newWorld = Matrix::CreateRotationY(90.0f * DirectX::XM_PI / 180.0f) * Matrix::CreateTranslation(position);
		pStair->UpdateWorld(newWorld);

		pStair->ModelType = RenderObjectType_DefaultType;
		m_RenderObjects.push_back(pStair);


		mesh.Indices =
		{
			0, 2, 1, 1, 2, 3,		// 하단면
			4, 5, 6, 5, 7, 6,		// 상단면
			8, 10, 9, 11, 13, 12,	// 양쪽면
			14, 16, 17, 15, 14, 17, // 뒷면
		};

		mesh.Indices.clear();
		for (int i = 0; i < 10; ++i)
		{
			UINT baseIndex = i * 24;
			UINT inds[36] = 
			{
				baseIndex,		baseIndex + 2,	baseIndex + 1,  baseIndex,		baseIndex + 3,  baseIndex + 2,	// 윗면
				baseIndex + 4,  baseIndex + 6,	baseIndex + 5,  baseIndex + 4,  baseIndex + 7,	baseIndex + 6,	// 아랫면
				baseIndex + 8,  baseIndex + 10, baseIndex + 9,	baseIndex + 8,  baseIndex + 11, baseIndex + 10, // 앞면
				baseIndex + 12, baseIndex + 14, baseIndex + 13, baseIndex + 12, baseIndex + 15, baseIndex + 14, // 뒷면
				baseIndex + 16, baseIndex + 18, baseIndex + 17, baseIndex + 16, baseIndex + 19, baseIndex + 18, // 왼쪽
				baseIndex + 20, baseIndex + 22, baseIndex + 21, baseIndex + 20, baseIndex + 23, baseIndex + 22  // 오른쪽
			};

			for (int j = 0; j < 36; ++j)
			{
				mesh.Indices.push_back(inds[j]);
			}
		}

		Matrix world = Matrix::CreateRotationY(-90.0f * DirectX::XM_PI / 180.0f) * Matrix::CreateTranslation(position);
		m_pPhysicsManager->CookingStaticTriangleMesh(&mesh.Vertices, &mesh.Indices, world);
	}
}

void App::updateAnimationState(SkinnedMeshModel* pCharacter, const float DELTA_TIME, int* pState, int* pFrame, Vector3* pDeltaPos)
{
	// States
	// 0: idle
	// 1: idle to walk
	// 2: walk forward
	// 3: walk to stop
	static int s_State = 0;
	static int s_FrameCount = 0;

	_ASSERT(pCharacter);
	_ASSERT(pDeltaPos);

	const UINT64 ANIMATION_CLIP_SIZE = pCharacter->CharacterAnimationData.Clips[s_State].Keys[0].size();
	const Vector3 GRAVITY(0.0f, -9.81f, 0.0f);
	switch (s_State)
	{
		case 0:
			if (m_Keyboard.bPressed[VK_UP])
			{
				// reset all update rot.
				// pCharacter->CharacterAnimationData.ResetAllUpdateRotationInClip(s_State);

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
			*pDeltaPos = (pCharacter->CharacterAnimationData.Direction + GRAVITY) * pCharacter->CharacterAnimationData.Velocity * DELTA_TIME;
			pCharacter->CharacterAnimationData.Position += *pDeltaPos;
			// (*pDeltaPos).y -= 0.5f;

			if (s_FrameCount == ANIMATION_CLIP_SIZE)
			{
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

			*pDeltaPos = (pCharacter->CharacterAnimationData.Direction + GRAVITY) * pCharacter->CharacterAnimationData.Velocity * DELTA_TIME;
			m_pCharacter->CharacterAnimationData.Position += *pDeltaPos;
			// (*pDeltaPos).y -= 0.5f;

			if (s_FrameCount == ANIMATION_CLIP_SIZE)
			{
				// 방향키를 누르고 있지 않으면 정지. (누르고 있으면 계속 걷기)
				if (!m_Keyboard.bPressed[VK_UP])
				{
					s_State = 3;
				}
				s_FrameCount = 0;
			}
		}
		break;

		case 3:
		{
			*pDeltaPos = (pCharacter->CharacterAnimationData.Direction + GRAVITY) * pCharacter->CharacterAnimationData.Velocity * DELTA_TIME;
			pCharacter->CharacterAnimationData.Position += *pDeltaPos;
			// (*pDeltaPos).y -= 0.5f;

			if (s_FrameCount == ANIMATION_CLIP_SIZE)
			{
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

void App::updateEndEffectorPosition(SkinnedMeshModel* pCharacter, SkinnedMeshModel::JointUpdateInfo* pUpdateInfo)
{
	_ASSERT(pCharacter);
	_ASSERT(pUpdateInfo);

	// Matrix correction = Matrix::CreateTranslation(Vector3(0.23f, 0.0f, 0.0f));

	physx::PxRaycastBuffer hit;
	physx::PxTransform rightFootTargetPos = pCharacter->pRightFoot->getGlobalPose();
	physx::PxTransform leftFootTargetPos = pCharacter->pLeftFoot->getGlobalPose();

	// physx::PxVec3 rayOrigin1 = physx::PxVec3(rightFootPos.x, rightFootPos.y - 0.5f, rightFootPos.z);
	// physx::PxVec3 rayOrigin2 = physx::PxVec3(leftFootPos.x, leftFootPos.y - 0.5f, leftFootPos.z);
	physx::PxVec3 rayOrigin1 = physx::PxVec3(rightFootTargetPos.p.x, rightFootTargetPos.p.y, rightFootTargetPos.p.z);
	physx::PxVec3 rayOrigin2 = physx::PxVec3(leftFootTargetPos.p.x, leftFootTargetPos.p.y, leftFootTargetPos.p.z);
	physx::PxVec3 rayDir(0.0f, -1.0f, 0.0f);

	physx::PxScene* pScene = GetPhysicsManager()->GetScene();
	if (pScene->raycast(rayOrigin1, rayDir, 1.0f, hit))
	{
		rightFootTargetPos.p.y = hit.block.position.y;
	}
	if (pScene->raycast(rayOrigin2, rayDir, 1.0f, hit))
	{
		leftFootTargetPos.p.y = hit.block.position.y;
	}

	Vector3 rightFootPosVec(rightFootTargetPos.p.x, rightFootTargetPos.p.y, rightFootTargetPos.p.z);
	Vector3 leftFootPosVec(leftFootTargetPos.p.x, leftFootTargetPos.p.y, leftFootTargetPos.p.z);

	// rightFootPosVec = Vector3::Transform(rightFootPosVec, correction);
	// leftFootPosVec = Vector3::Transform(leftFootPosVec, correction);
	/*{
		char szDebugString[256];
		sprintf_s(szDebugString, 256, "rightFootPos: %f, %f, %f  leftFootPos: %f, %f, %f\n", rightFootPosVec.x, rightFootPosVec.y, rightFootPosVec.z, leftFootPosVec.x, leftFootPosVec.y, leftFootPosVec.z);
		OutputDebugStringA(szDebugString);
	}*/

	pUpdateInfo->bUpdatedJointParts[SkinnedMeshModel::JointPart_RightLeg] = true;
	pUpdateInfo->bUpdatedJointParts[SkinnedMeshModel::JointPart_LeftLeg] = true;
	pUpdateInfo->EndEffectorTargetPoses[SkinnedMeshModel::JointPart_RightLeg] = rightFootPosVec;
	pUpdateInfo->EndEffectorTargetPoses[SkinnedMeshModel::JointPart_LeftLeg] = leftFootPosVec;
}

void App::simulateCharacterContol(SkinnedMeshModel* pCharacter, const Vector3& DELTA_POS, const float DELTA_TIME, int clipID, int frame)
{
	_ASSERT(pCharacter);

	const Vector3 GRAVITY(0.0f, -9.81f, 0.0f);

	// 위치 변위.
	physx::PxVec3 displacement = physx::PxVec3(DELTA_POS.x, DELTA_POS.y, DELTA_POS.z);

	// physx 상에서 캐릭터 이동.
	physx::PxFilterData filterData;
	filterData.word0 = CollisionGroup_Default;

	CustomFilterCallback filterCallback;

	physx::PxControllerFilters filters;
	filters.mFilterData = &filterData;
	filters.mFilterCallback = &filterCallback;

	physx::PxControllerCollisionFlags flags = pCharacter->pController->move(displacement, 0.001f, DELTA_TIME, filters);
	pCharacter->CharacterAnimationData.Update(clipID, frame);

	// 말단위치(end-effector) 이동.
	physx::PxRigidDynamic* pRightFoot = pCharacter->pRightFoot;
	physx::PxRigidDynamic* pLeftFoot = pCharacter->pLeftFoot;

	// Matrix correction = Matrix::CreateTranslation(Vector3(-0.24f, 0.0f, 0.0f));

	const Matrix CHARACTER_WORLD = Matrix::CreateTranslation(pCharacter->CharacterAnimationData.Position);
	// Vector3 rightFootPos = (correction * pCharacter->RightLeg.BodyChain[3].Correction * pCharacter->CharacterAnimationData.Get(pCharacter->RightLeg.BodyChain[3].BoneID) * pCharacter->World).Translation();
	// Vector3 leftFootPos = (correction * pCharacter->LeftLeg.BodyChain[3].Correction * pCharacter->CharacterAnimationData.Get(pCharacter->LeftLeg.BodyChain[3].BoneID) * pCharacter->World).Translation();
	Vector3 rightFootPos = (pCharacter->CharacterAnimationData.GetGlobalBonePositionMatix(clipID, frame, pCharacter->RightLeg.BodyChain[3].BoneID) * CHARACTER_WORLD).Translation();
	Vector3 leftFootPos = (pCharacter->CharacterAnimationData.GetGlobalBonePositionMatix(clipID, frame, pCharacter->LeftLeg.BodyChain[3].BoneID) * CHARACTER_WORLD).Translation();
	/*Vector3 rightFootPos = pCharacter->RightLeg.BodyChain[3].Position;
	Vector3 leftFootPos = pCharacter->LeftLeg.BodyChain[3].Position;*/

	/*{
		char szDebugString[256];
		sprintf_s(szDebugString, 256, "rightFootPos: %f, %f, %f  leftFootPos: %f, %f, %f\n", rightFootPos.x, rightFootPos.y, rightFootPos.z, leftFootPos.x, leftFootPos.y, leftFootPos.z);
		OutputDebugStringA(szDebugString);
	}*/

	physx::PxTransform rightFootTranslation(physx::PxVec3(rightFootPos.x, rightFootPos.y, rightFootPos.z));
	physx::PxTransform leftFootTranslation(physx::PxVec3(leftFootPos.x, leftFootPos.y, leftFootPos.z));
	pRightFoot->setKinematicTarget(rightFootTranslation);
	pLeftFoot->setKinematicTarget(leftFootTranslation);


	physx::PxExtendedVec3 nextPos = pCharacter->pController->getPosition();
	Vector3 nextPosVec((float)nextPos.x, (float)nextPos.y, (float)nextPos.z);
	
	// 받아온 위치 기반 캐릭터 위치 갱신.
	pCharacter->CharacterAnimationData.Position = nextPosVec;
	// pCharacter->CharacterAnimationData.Position.y += 0.5f;
}
