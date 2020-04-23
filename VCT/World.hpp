
#pragma once
#include "ModelAssimp.h"
#include "GameCore.h"
#include "CameraController.h"
#include "Camera.h"
#include "Light.hpp"
#include "Voxelization.hpp"

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

        World(const World&) = delete;

        World& operator = (const World&) = delete;

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

        inline Voxel::Voxelization& GetVoxelization() { return m_voxelization; }

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

        void voxelize(GraphicsContext& context) { m_voxelization.voxelize(context); }

	private:

        const BoundingBox GetClipBoundingBox(const int level) const;

        void UpdateClipBoundgingBoxs();

		void CaculateBoundingBox();

		Camera m_Camera;

		const std::unique_ptr<Lighting> m_lighting;

		std::unique_ptr<CameraController> m_CameraController;

		BoundingBox m_boundingbox;

		static World* s_world;

        float m_clipRegionBBoxExtentL0{ 16.0f }; 

        std::vector<BoundingBox> m_clip_bboxs;

        Voxel::Voxelization m_voxelization;
	};
}
