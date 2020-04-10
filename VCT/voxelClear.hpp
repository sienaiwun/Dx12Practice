
#pragma once
#include "GameCore.h"
#include "GraphicsCore.h"
#include "CommandContext.h"
#include "Texture3D.h"

using namespace Math;
using namespace GameCore;

namespace VoxelClear
{
    void Initialize(void);

    void Apply(ComputeContext& context);

    __declspec(align(16)) struct ConstantBuffer
    {
        glm::ivec3 u_min;
        int u_resolution;
        int u_clipmapLevel;
        glm::ivec3 u_extent;
        int u_borderWidth = 1;
    };

    void SetConstantBuffer(ConstantBuffer buffer);

    void SetTexture3D(Texture3D* tex);
    

}
