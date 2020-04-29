
#pragma once
#include "Math/Vector.h"
#include "Math/Matrix4.h"
#include "GameCore.h"
#include "GraphicsCore.h"
#include "CommandContext.h"
#include "VoxelRegion.hpp"

using namespace Math;
using namespace GameCore;
using namespace Voxel;

namespace VoxelizationPass
{
    void Initialize(void);

    void Render(GraphicsContext& context, const VoxelRegion& region, const int clip_level);

    __declspec(align(16)) struct voxelizationGSCBuffer
    {
        Matrix4 u_viewProj[3];
        Matrix4 u_viewProjInv[3];
        F32x2   u_viewportSizes[3]; 
    };

    __declspec(align(16)) struct voxelizationPSCBuffer
    {
        int u_clipmapLevel;
        int u_clipmapResolution;
        int u_clipmapResolutionWithBorder;
        int _____;
        F32x3 u_regionMin;
        float _;
        F32x3 u_regionMax;
        float __;
        F32x3 u_prevRegionMin;
        float ___;
        F32x3 u_prevRegionMax;
        float ____;
        float u_downsampleTransitionRegionSize;
        float u_maxExtent;
        float u_voxelSize;
    };

}
