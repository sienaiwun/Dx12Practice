
#pragma once
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

    void Render(GraphicsContext& context, const VoxelRegion& region);

}
