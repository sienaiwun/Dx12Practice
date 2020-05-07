#pragma once
#include "GameCore.h"
#include "GraphicsCore.h"
#include "CommandContext.h"
#include "VoxelVisualizePass.hpp"
#include "VisualizeMesh.hpp"
#include "Texture3D.h"
#include "VoxelRegion.hpp"

using namespace Voxel;
namespace VoxelVisualization
{
    __declspec(align(16)) struct VoxelVisualizationGSBuffer
    {
        U32x3 u_imageMin;
        U32 _;
        U32x3 u_regionMin;
        U32 __;
        U32x3 u_prevRegionMin;
        U32 ___;
        U32x3 u_prevRegionMax;
        U32 ____;
        int u_hasPrevClipmapLevel;
        int u_clipmapResolution;
        int u_clipmapLevel;
        float u_voxelSize;
        int u_hasMultipleFaces;
        int u_numColorComponents;
        Math::Matrix4 u_viewProj;
    };


    __declspec(align(16)) struct VoxelVisualizationPSBuffer
    {
        F32x4 u_borderColor;
        float u_borderWidth;
        float u_alpha;
    };

    void Initialize(void);

    class VoxelVisualize
    {
    public:
        VoxelVisualize() = default;
        ~VoxelVisualize() = default;
        VoxelVisualize(const VoxelVisualize& _) = delete;
        VoxelVisualize& operator = (const VoxelVisualize& _) = delete;
        void DrawClip(GraphicsContext& context, const D3D12_CPU_DESCRIPTOR_HANDLE& texSrv);
        void InitMesh(const size_t res_num);
        void Visualize3DClipmapGS( const VoxelRegion& region, U32 clipmapLevel, const VoxelRegion& prevRegion, const Math::Matrix4& mvp,
            bool hasPrevLevel, bool hasFaces, int numColorComponents);
    private:
        std::unique_ptr<VisualMesh> m_visual_mesh = nullptr;
    };
}