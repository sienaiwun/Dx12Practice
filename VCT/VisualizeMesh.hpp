#pragma once
#include "GameCore.h"
#include "GraphicsCore.h"
#include "CommandContext.h"

namespace Voxel
{
    class VisualMesh
    {
    public:
        VisualMesh();

        VisualMesh(const VisualMesh& _) = delete;

        VisualMesh& operator = (const VisualMesh& _) = delete;


        ~VisualMesh();

        void Create(const size_t count);

        void Draw(GraphicsContext& context);

    private:

        StructuredBuffer m_VertexBuffer;

        size_t m_vertexCount;
    };
}