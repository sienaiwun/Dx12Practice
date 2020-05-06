#include "VisualizeMesh.hpp"

namespace Voxel
{
    VisualMesh::VisualMesh():
       m_vertexCount(0)
    {  }

    void VisualMesh::Create(const size_t resolution)
    {
        m_vertexCount = size_t(resolution * resolution * resolution );
        std::vector<F32x3> vertices;
        vertices.reserve(m_vertexCount);
        for(size_t x =0;x<resolution;x++)
            for(size_t y =0;y<resolution;y++)
                for(size_t z = 0;z<resolution;z++)
                    vertices.emplace_back(F32x3{(float) x, (float)y, (float)z });
        m_VertexBuffer.Create(L"voxelBuffer", m_vertexCount, sizeof(F32x3), vertices.data());
    }

    VisualMesh::~VisualMesh()
    {
        m_VertexBuffer.Destroy();
    }

    void VisualMesh::Draw(GraphicsContext& context)
    {
        assert(m_vertexCount > 0);
        context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
        context.SetVertexBuffer(0, m_VertexBuffer.VertexBufferView());
        context.Draw(m_vertexCount,0);
    }
}