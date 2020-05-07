
#pragma once
#include "VoxelRegion.hpp"
#include "Texture3D.h"
#include "VoxelVisualizePass.hpp"

using namespace Math;
using namespace GameCore;

namespace Voxel
{
    class Voxelization
    {
    public:
        explicit Voxelization();

        void Init(float extentWorldLevel0, const std::vector<BoundingBox>& clipRegionBBoxes);

        void Update(const std::vector<BoundingBox>& bboxs);

        void Voxelize(GraphicsContext& context);

        void Visualize(GraphicsContext& context, const Math::Matrix4& mvpMatrix);

        inline void ToggleVisualization(bool flag) noexcept { m_visualize = flag; }

        inline bool GetVisualization() noexcept { return m_visualize; } 

        inline const D3D12_CPU_DESCRIPTOR_HANDLE& VoxelOpacitySRV() { return m_voxelOpacity.GetSRV(); }

        inline const D3D12_CPU_DESCRIPTOR_HANDLE& VoxelOpacityUAV() { return m_voxelOpacity.GetUAV(); }

        inline const D3D12_CPU_DESCRIPTOR_HANDLE& VoxelRadianceSRV() { return m_voxelRadiance.GetSRV(); }

        inline const D3D12_CPU_DESCRIPTOR_HANDLE& VoxelRadianceUAV() { return m_voxelRadiance.GetUAV(); }

        inline std::vector<VoxelRegion>& GetClieRegions() { return m_clipRegions; }

        inline std::vector<VoxelRegion>& GetRevoxelRegions(const int i) { ASSERT(i >= 0 && i < CLIP_REGION_COUNT); return m_revoxelizationRegions[i]; }

    private:

        void computeRevoxelizationRegionsClipmap(uint32_t clipmapLevel, const BoundingBox& curBBox);

        glm::ivec3 computeChangeDeltaV(uint32_t clipmapLevel, const BoundingBox& cameraRegionBBox);

        int m_minChange[CLIP_REGION_COUNT] = { 2, 2, 2, 2, 2, 1 };

        std::vector<VoxelRegion> m_clipRegions;

        std::vector<VoxelRegion> m_revoxelizationRegions[CLIP_REGION_COUNT];

        bool m_forceFullRevoxelization{ false };

        Texture3D m_voxelOpacity;

        Texture3D m_voxelRadiance;

        VoxelVisualization::VoxelVisualize m_voxelvisualize;

        bool m_visualize;

    };
}
