
#pragma once
#include "ModelAssimp.h"
#include "GameCore.h"
#include "CameraController.h"
#include "Camera.h"
#include "Light.hpp"

using namespace Math;
using namespace GameCore;

namespace SceneView
{
	class World final
	{
	public:

		static NotNull<World*> Get() noexcept {
			return NotNull<World*>(s_world);
		}


		World();

		void AddModel(const std::string& filename);

		void Create();

		void Clear();

		void Update(const float delta);

		void GenerateLightBuffer(GraphicsContext& gfxContext, const Math::Camera& camera)
		{
			m_lighting->FillLightGrid(gfxContext, camera);
		}

		inline const BoundingBox& GetBoundingBox() const noexcept { return m_boundingbox; }

		inline const Camera& GetMainCamera() const noexcept { return m_Camera; }

		inline Camera& GetMainCamera() noexcept { return m_Camera; }

        [[nodiscard]]
		NotNull<Lighting*> GetLighting() noexcept { return NotNull<Lighting*>(m_lighting.get()); }

		std::vector<AssimpModel> m_models;

		template<typename ActionT >
		void ForEach(ActionT&& action)
		{
			for (auto& model : m_models) 
			{
				action(model);
			}
		}

	private:

		void CaculateBoundingBox();

		Camera m_Camera;

		const std::unique_ptr<Lighting> m_lighting;

		std::unique_ptr<CameraController> m_CameraController;

		BoundingBox m_boundingbox;

		static World* s_world;
	};
}
