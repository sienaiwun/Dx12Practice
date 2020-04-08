
#pragma once
#include "VoxelRegion.hpp"

using namespace Math;
using namespace GameCore;

namespace Voxel
{
    class Voxelization
    {
    public:
        explicit Voxelization();

        void init(float extentWorldLevel0);

    private:

        std::vector<VoxelRegion> m_clipRegions;

        bool m_forceFullRevoxelization{ false };
    };
}
