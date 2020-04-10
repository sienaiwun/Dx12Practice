
#pragma once
#include "VoxelRegion.hpp"
#include "Texture3D.h"

using namespace Math;
using namespace GameCore;

namespace Voxel
{
    class Voxelization
    {
    public:
        explicit Voxelization();

        void init(float extentWorldLevel0, const std::vector<BoundingBox>& clipRegionBBoxes);

        void update(const std::vector<BoundingBox>& bboxs);

        void voxelize(GraphicsContext& context);

        inline const D3D12_CPU_DESCRIPTOR_HANDLE& VoxelOpacitySRV() { return m_voxelOpacity.GetSRV(); }

        inline const D3D12_CPU_DESCRIPTOR_HANDLE& VoxelOpacityUAV() { return m_voxelOpacity.GetUAV(); }

        inline const D3D12_CPU_DESCRIPTOR_HANDLE& VoxelRadianceSRV() { return m_voxelRadiance.GetSRV(); }

        inline const D3D12_CPU_DESCRIPTOR_HANDLE& VoxelRadianceUAV() { return m_voxelRadiance.GetUAV(); }

    private:

        void computeRevoxelizationRegionsClipmap(uint32_t clipmapLevel, const BoundingBox& curBBox);

        glm::ivec3 computeChangeDeltaV(uint32_t clipmapLevel, const BoundingBox& cameraRegionBBox);

        int m_minChange[CLIP_REGION_COUNT] = { 2, 2, 2, 2, 2, 1 };

        std::vector<VoxelRegion> m_clipRegions;
        std::vector<VoxelRegion> m_revoxelizationRegions[CLIP_REGION_COUNT];

        bool m_forceFullRevoxelization{ false };


        Texture3D m_voxelOpacity;
        Texture3D m_voxelRadiance;

    };
}
