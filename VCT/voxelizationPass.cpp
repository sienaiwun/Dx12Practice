#include "voxelizationPass.hpp"
#include "World.hpp"
#include "CompiledShaders/ConservertiveVoxelPassVS.h"
#include "CompiledShaders/ConservertiveVoxelPassGS.h"
#include "CompiledShaders/ConservertiveVoxelPassPS.h"


namespace VoxelizationPass
{
    RootSignature s_RootSignature;
    GraphicsPSO s_voxelPSO;

    void Initialize(void)
    {
        
    }

    void Render(GraphicsContext& context, const VoxelRegion& region)
    {
        SceneView::World::Get()->ForEach([&](Model &model)
        {
            uint32_t VertexStride = model.m_VertexStride;
            context.SetRootSignature(s_RootSignature);
            context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            context.SetIndexBuffer(model.m_IndexBuffer.IndexBufferView());
            context.SetVertexBuffer(0, model.m_VertexBuffer.VertexBufferView());
            for (uint32_t meshIndex = 0; meshIndex < model.m_Header.meshCount; meshIndex++)
            {
                const Model::Mesh& mesh = model.m_pMesh[meshIndex];
                uint32_t indexCount = mesh.indexCount;
                uint32_t startIndex = mesh.indexDataByteOffset / sizeof(uint16_t);
                uint32_t baseVertex = mesh.vertexDataByteOffset / VertexStride;
                context.DrawIndexed(indexCount, startIndex, baseVertex);
            }
        });
    }


}