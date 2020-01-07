#pragma region 
#include "World.hpp"
#pragma endregion

namespace SceneView
{
	World* World::s_world = nullptr;

	World::World()
	:m_models(), 
	m_CameraController (std::make_unique <CameraController>(m_Camera, Vector3(kYUnitVector))),
	m_lighting(std::make_unique <Lighting>())
	{
		s_world = this;
	}

	void World::AddModel(const std::string& filename)
	{
		AssimpModel model;;
		ASSERT(model.Load(filename.c_str()), "Failed to load model:" );
		model.PrintInfo();
		m_models.emplace_back(std::move(model));

	}

	void World::Create()
	{
#if 1
		AddModel("Models/box.obj");
		AddModel("Models/sphere.obj");
		AddModel("Models/capsule.obj");
		AddModel("Models/plane.obj");
		AddModel("Models/sponza.h3d");
#else
		AddModel("Models/sponza.h3d");
#endif
		CaculateBoundingBox();
		//lights 
		m_lighting->InitializeResources();
		m_lighting->CreateRandomLights(GetBoundingBox().min, GetBoundingBox().max);
		//camera
		float modelRadius = Length(GetBoundingBox().max - GetBoundingBox().min) * .5f;
		const Vector3 eye = (GetBoundingBox().min + GetBoundingBox().max) * .5f + Vector3(modelRadius * .5f, 0.0f, 0.0f);
		m_Camera.SetEyeAtUp(eye, Vector3(kZero), Vector3(kYUnitVector));
		m_Camera.SetZRange(1.0f, 10000.0f);
		m_CameraController.reset(new CameraController(m_Camera, Vector3(kYUnitVector)));
        std::shared_ptr<const ManagedTexture> sky_texture(TextureManager::LoadFromFile(L"sky/sky", true));
        m_Camera.GetSky().SetTexture(std::move(sky_texture));
	}

	void World::Update(const float deltaT)
	{
		m_CameraController->Update(deltaT);
	}

	void World::Clear()
	{
		m_lighting->Shutdown();
	}

	void World::CaculateBoundingBox()
	{
		ForEach([&](Model& model) {
			m_boundingbox.min = Min(m_boundingbox.min, model.GetBoundingBox().min);
			m_boundingbox.max = Max(m_boundingbox.max, model.GetBoundingBox().max);
		});
	}
}