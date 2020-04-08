#pragma region 
#include "GameCore.h"
#include "Voxelization.hpp"

#pragma endregion

using namespace Voxel;


Voxelization::Voxelization()
{
    m_clipRegions.resize(CLIP_REGION_COUNT);

}

void Voxelization::init(float extentWorldLevel0)
{
    int extent = VOXEL_RESOLUTION;
    int halfExtent = extent / 2;
    for (size_t i = 0; i < m_clipRegions.size(); ++i)
    {
        m_clipRegions[i].minPos = glm::ivec3(-halfExtent);
        m_clipRegions[i].extent = glm::ivec3(extent);
        m_clipRegions[i].voxelSize = (extentWorldLevel0 * std::exp2f(static_cast<float>(i))) / extent;
    }
    for (uint32_t clipmapLevel = 0; clipmapLevel < CLIP_REGION_COUNT; ++clipmapLevel)
    {
        auto& clipRegion = m_clipRegions[clipmapLevel];

        // Compute the closest delta in voxel coordinates
       // glm::ivec3 delta = computeChangeDeltaV(clipmapLevel, clipRegionBBoxes->at(clipmapLevel));

      //  clipRegion.minPos += delta;
    }

    m_forceFullRevoxelization = true;

}

