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

	for (UINT64 i = 0, size = m_RenderObjects.size(); i < size; ++i)
	{
		Model* pModel = m_RenderObjects[i];

		switch (pModel->ModelType)
		{
			case RenderObjectType_DefaultType:
			case RenderObjectType_MirrorType:
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

	// ȯ��� �ؽ��� �ε�.
	m_EnvTexture.InitializeWithDDS(pRenderer, L"./Assets/Textures/Cubemaps/HDRI/clear_pureskyEnvHDR.dds");
	m_IrradianceTexture.InitializeWithDDS(pRenderer, L"./Assets/Textures/Cubemaps/HDRI/clear_pureskyEnvHDR.dds");
	m_SpecularTexture.InitializeWithDDS(pRenderer, L"./Assets/Textures/Cubemaps/HDRI/clear_pureskyEnvHDR.dds");
	m_BRDFTexture.InitializeWithDDS(pRenderer, L"./Assets/Textures/Cubemaps/HDRI/clear_pureskyEnvHDR.dds");



	// ȯ�� �ڽ� �ʱ�ȭ.
	{
		MeshInfo skyboxMeshInfo = INIT_MESH_INFO;
		MakeBox(&skyboxMeshInfo, 40.0f);

		std::reverse(skyboxMeshInfo.Indices.begin(), skyboxMeshInfo.Indices.end());
		Model* pSkybox = new Model(pRenderer, { skyboxMeshInfo });
		pSkybox->Name = "SkyBox";
		pSkybox->ModelType = RenderObjectType_SkyboxType;
		m_RenderObjects.push_back(pSkybox);
	}

	// ���� ����.
	{
		// ���� 0.
		Light& light0 = m_Lights[0];
		light0.Property.Radiance = Vector3(3.0f);
		light0.Property.FallOffEnd = 10.0f;
		light0.Property.Position = Vector3(0.0f, 0.5f, -0.9f);
		light0.Property.Direction = Vector3(0.0f, 0.0f, 1.0f);
		light0.Property.SpotPower = 3.0f;
		light0.Property.LightType = LIGHT_POINT | LIGHT_SHADOW;
		light0.Property.Radius = 0.04f;
		light0.Initialize(pRenderer);

		// ���� 1.
		Light& light1 = m_Lights[1];
		light1.Property.Radiance = Vector3(3.0f);
		light1.Property.FallOffEnd = 10.0f;
		light1.Property.Position = Vector3(1.0f, 1.1f, 2.0f);
		light1.Property.SpotPower = 2.0f;
		light1.Property.Direction = Vector3(0.0f, -0.5f, 1.7f) - m_Lights[1].Property.Position;
		light1.Property.Direction.Normalize();
		light1.Property.LightType = LIGHT_SPOT | LIGHT_SHADOW;
		light1.Property.Radius = 0.02f;
		light1.Initialize(pRenderer);

		// ���� 2.
		Light& light2 = m_Lights[2];
		light2.Property.Radiance = Vector3(5.0f);
		light2.Property.Position = Vector3(5.0f, 5.0f, 5.0f);
		light2.Property.Direction = Vector3(-1.0f, -1.0f, -1.0f);
		light2.Property.Direction.Normalize();
		light2.Property.LightType = LIGHT_DIRECTIONAL | LIGHT_SHADOW;
		light2.Property.Radius = 0.05f;
		light2.Initialize(pRenderer);
	}

	// ���� ��ġ ǥ��.
	for (int i = 0; i < MAX_LIGHTS; ++i)
	{
		MeshInfo sphere = INIT_MESH_INFO;
		MakeSphere(&sphere, 1.0f, 20, 20);

		m_LightSpheres[i] = new Model(pRenderer, { sphere });
		m_LightSpheres[i]->UpdateWorld(Matrix::CreateTranslation(m_Lights[i].Property.Position));

		MaterialConstant& sphereMaterialConstantData = m_LightSpheres[i]->Meshes[0]->MaterialConstantData;
		sphereMaterialConstantData.AlbedoFactor = Vector3(0.0f);
		sphereMaterialConstantData.EmissionFactor = Vector3(1.0f, 1.0f, 0.0f);
		m_LightSpheres[i]->bCastShadow = false; // ���� ǥ�� ��ü���� �׸��� X.
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

	// �ٴ�(�ſ�).
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

		MaterialConstant& groundMaterialConstantData = pGround->Meshes[0]->MaterialConstantData;
		groundMaterialConstantData.AlbedoFactor = Vector3(0.7f);
		groundMaterialConstantData.EmissionFactor = Vector3(0.0f);
		groundMaterialConstantData.MetallicFactor = 0.5f;
		groundMaterialConstantData.RoughnessFactor = 0.3f;

		// Vector3 position = Vector3(0.0f, -1.0f, 0.0f);
		// Vector3 position = Vector3(0.0f, -0.5f, 0.0f);
		Vector3 position = Vector3(0.0f);
		pGround->UpdateWorld(Matrix::CreateRotationX(DirectX::XM_PI * 0.5f) * Matrix::CreateTranslation(position));
		pGround->bCastShadow = false; // �ٴ��� �׸��� ����� ����.

		m_MirrorPlane = DirectX::SimpleMath::Plane(position, Vector3(0.0f, 1.0f, 0.0f));
		m_pMirror = pGround; // �ٴڿ� �ſ�ó�� �ݻ� ����.
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

		// �ִϸ��̼� Ŭ����.
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


		physx::PxControllerManager* pControlManager = m_PhysicsManager.GetControllerManager();
		physx::PxCapsuleControllerDesc capsuleDesc;
		physx::PxController* pController = nullptr;

		// capsule �޽ö� �����ϰ� ����. ��, ���⼭�� y���� 0���� ��������� ��.
		capsuleDesc.height = (m_pCharacter->BoundingSphere.Radius * 1.35f) - 0.5f;
		capsuleDesc.radius = 0.25f;
		capsuleDesc.upDirection = physx::PxVec3(0.0f, 1.0f, 0.0f);
		capsuleDesc.position = physx::PxExtendedVec3(m_pCharacter->CharacterAnimationData.Position.x, m_pCharacter->CharacterAnimationData.Position.y, m_pCharacter->CharacterAnimationData.Position.z);
		capsuleDesc.contactOffset = 0.05f;
		capsuleDesc.material = m_PhysicsManager.pCommonMaterial;
		capsuleDesc.stepOffset = (capsuleDesc.radius + capsuleDesc.height * 0.5f) * 0.3f;
		capsuleDesc.climbingMode = physx::PxCapsuleClimbingMode::eCONSTRAINED;
		pController = pControlManager->createController(capsuleDesc);
		if (!pController)
		{
			__debugbreak();
		}
		m_pCharacter->pController = pController;
	}

	// ����
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

		// mesh.indices ==> right-hand coordinates�� ���� ����.
		mesh.Indices =
		{
			0, 2, 1, 1, 2, 3, // �ϴܸ�
			4, 5, 6, 5, 7, 6, // ��ܸ�
			8, 10, 9, 11, 13, 12, // ���ʸ�
			14, 16, 17, 15, 14, 17, // �޸�
		};

		m_PhysicsManager.CookingStaticTriangleMesh(&mesh.Vertices, &mesh.Indices, pSlope->World);
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
			else if (s_FrameCount == ANIMATION_CLIP_SIZE || m_Keyboard.bPressed[VK_UP]) // ����� �� �����ٸ�.
			{
				s_FrameCount = 0; // ���� ��ȭ ���� �ݺ�.
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

			// moveinfo.direction�� moveinfo.rotation�� ���������� ���� ȸ��.
			if (m_Keyboard.bPressed[VK_RIGHT])
			{
				Quaternion newRot = Quaternion::CreateFromYawPitchRoll(DirectX::XM_PI * 60.0f / 180.0f * DELTA_TIME * 2.0f, 0.0f, 0.0f);
				pCharacter->CharacterAnimationData.Direction = Vector3::TransformNormal(pCharacter->CharacterAnimationData.Direction, Matrix::CreateFromQuaternion(newRot));
				pCharacter->CharacterAnimationData.Rotation = Quaternion::Concatenate(pCharacter->CharacterAnimationData.Rotation, newRot);
			}
			// moveinfo.direction�� moveinfo.rotation�� �������� ���� ȸ��.
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
				// ����Ű�� ������ ���� ������ ����. (������ ������ ��� �ȱ�)
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

	// ��ġ ����.
	physx::PxVec3 displacement = physx::PxVec3(DELTA_POS.x, DELTA_POS.y, DELTA_POS.z);

	// physx �󿡼� ĳ���� �̵�.
	physx::PxControllerCollisionFlags flags = pCharacter->pController->move(displacement, 0.001f, DELTA_TIME, physx::PxControllerFilters());

	// physx �󿡼��� ĳ���� ��ġ �޾ƿ���.
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
	Vector3 nextPosVec((float)nextPos.x, (float)nextPos.y, (float)nextPos.z);

	// �޾ƿ� ��ġ ��� ĳ���� ��ġ ����.
	pCharacter->CharacterAnimationData.Position = nextPosVec;
	pCharacter->CharacterAnimationData.Position.y += 0.4f;

	{
		char szDebugString[256];
		sprintf_s(szDebugString, 256, "character pos: %f, %f, %f\n", pCharacter->CharacterAnimationData.Position.x, pCharacter->CharacterAnimationData.Position.y, pCharacter->CharacterAnimationData.Position.z);
		OutputDebugStringA(szDebugString);

		sprintf_s(szDebugString, 256, "direction: %f, %f, %f\n", pCharacter->CharacterAnimationData.Direction.x, pCharacter->CharacterAnimationData.Direction.y, pCharacter->CharacterAnimationData.Direction.z);
		OutputDebugStringA(szDebugString);
	}
}
