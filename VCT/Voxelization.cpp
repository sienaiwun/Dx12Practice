#pragma region header
#include "GameCore.h"
#include "CommandContext.h"
#include "Voxelization.hpp"
#include "voxelClear.hpp"
#include "voxelizationPass.hpp"
#include <glm/glm.hpp>

#pragma endregion

namespace Voxel
{
    RootSignature s_RootSignature;
    GraphicsPSO s_VoxelizePSO;
}

using namespace Voxel;
Voxelization::Voxelization()
{
    m_clipRegions.resize(CLIP_REGION_COUNT);
}

void Voxelization::init(float extentWorldLevel0, const std::vector<BoundingBox>& clipRegionBBoxes)
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

        glm::ivec3 delta = computeChangeDeltaV(clipmapLevel, clipRegionBBoxes.at(clipmapLevel));

        clipRegion.minPos += delta;
    }

    m_forceFullRevoxelization = true;

    constexpr const uint32_t voxelSizeWithBorder = VOXEL_RESOLUTION + 2;
    m_voxelOpacity.Create(L"voxel Opacity", voxelSizeWithBorder * FACE_COUNT, CLIP_REGION_COUNT * voxelSizeWithBorder, voxelSizeWithBorder, DXGI_FORMAT_R8G8B8A8_UINT);
    m_voxelRadiance.Create(L"voxel Radiance", voxelSizeWithBorder * FACE_COUNT, CLIP_REGION_COUNT * voxelSizeWithBorder, voxelSizeWithBorder, DXGI_FORMAT_R8G8B8A8_UINT);
    VoxelClear::Initialize();

}

glm::ivec3 Voxelization::computeChangeDeltaV(uint32_t level, const BoundingBox& cameraRegionBBox)
{
    const auto & clipRegion = m_clipRegions[level];
    const float voxelSize = clipRegion.voxelSize;
    Dagon::Vec3f deltaW = Dagon::Vec3f(cameraRegionBBox.min) - clipRegion.getMinPosWorld();
    float minChange = voxelSize * m_minChange[level];
    glm::ivec3 delta = glm::ivec3(glm::trunc(deltaW / minChange)) *m_minChange[level];
    return delta;
}

void Voxelization::computeRevoxelizationRegionsClipmap(uint32_t level, const BoundingBox& curBBox)
{
    auto & clipRegion = m_clipRegions[level];
    const float voxelSize = clipRegion.voxelSize;
    const glm::ivec3 extent = clipRegion.extent;
    const glm::ivec3 delta = computeChangeDeltaV(level, curBBox);
    const glm::ivec3 absDelta = glm::abs(delta);
    clipRegion.minPos += delta;
    if (absDelta.x >= extent.x || absDelta.y >= extent.y || absDelta.z >= extent.z)
    {
        m_revoxelizationRegions[level].push_back(clipRegion);
        return;
    }
    glm::ivec3 newMin = clipRegion.minPos;
    glm::ivec3 newMax = clipRegion.getMaxPos();

    if (absDelta.x >= m_minChange[level])
    {
        auto regionExtent = glm::ivec3(absDelta.x, extent.y, extent.z);

        if (delta.x < 0)
            m_revoxelizationRegions[level].push_back(VoxelRegion(newMin, regionExtent, voxelSize));
        else
            m_revoxelizationRegions[level].push_back(VoxelRegion(newMax - regionExtent, regionExtent, voxelSize));
    }

    if (absDelta.y >= m_minChange[level])
    {
        auto regionExtent = glm::ivec3(extent.x, absDelta.y, extent.z);

        if (delta.y < 0)
            m_revoxelizationRegions[level].push_back(VoxelRegion(newMin, regionExtent, voxelSize));
        else
            m_revoxelizationRegions[level].push_back(VoxelRegion(newMax - regionExtent, regionExtent, voxelSize));
    }

    if (absDelta.z >= m_minChange[level])
    {
        auto regionExtent = glm::ivec3(extent.x, extent.y, absDelta.z);

        if (delta.z < 0)
            m_revoxelizationRegions[level].push_back(VoxelRegion(newMin, regionExtent, voxelSize));
        else
            m_revoxelizationRegions[level].push_back(VoxelRegion(newMax - regionExtent, regionExtent, voxelSize));
    }
}


void Voxelization::update(const std::vector<BoundingBox>& bboxs)
{
   
    if (m_forceFullRevoxelization)
    {
        for (uint32_t i = 0; i < CLIP_REGION_COUNT; ++i)
        {
            m_revoxelizationRegions[i].clear();
            m_revoxelizationRegions[i].push_back(m_clipRegions[i]);
        }
        //m_forceFullRevoxelization = false;
    }
    else
    {
        for (uint32_t i = 0; i < CLIP_REGION_COUNT; ++i)
        {
            m_revoxelizationRegions[i].clear();
            computeRevoxelizationRegionsClipmap(i, bboxs.at(i));
        }
    }
    ComputeContext& context = ComputeContext::Begin(L"Voxel Clear", true);
    {
        ScopedTimer _prof(L"Voxel", context);
        context.TransitionResource(m_voxelOpacity, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        for (uint32_t i = 0; i < CLIP_REGION_COUNT; ++i)
        {
            for (auto& region : m_revoxelizationRegions[i])
            {
                VoxelClear::ConstantBuffer cbv;
                cbv.u_clipmapLevel = i;
                cbv.u_extent = region.extent;
                cbv.u_resolution = VOXEL_RESOLUTION;
                cbv.u_min = region.getMinPosImage(m_clipRegions[i].extent);
                VoxelClear::SetConstantBuffer(cbv);
                VoxelClear::Apply(context);
              }
        }
    }
    context.Finish();
}

void Voxelization::voxelize(GraphicsContext& context)
{
    for (int i = 0; i < CLIP_REGION_COUNT; ++i)
    {
        // Voxelize the regions
        for (auto& region : m_revoxelizationRegions[i])
        {
            VoxelizationPass::Render(context, region);
        }
    }
}