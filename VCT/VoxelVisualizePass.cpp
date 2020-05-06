#include "VoxelVisualizePass.hpp"
#include "BufferManager.h"
#include "CompiledShaders/VoxelVisualizeGS.h"
#include "CompiledShaders/VoxelVisualizeVS.h"
#include "CompiledShaders/VoxelVisualizePS.h"

using namespace Graphics;
namespace VoxelVisualization
{
    RootSignature s_RootSignature;
    GraphicsPSO s_voxel_visualPSO;

    enum class VoxelVisualizationParams: U8
    {
        Texture3d,
        GSParam,
        PSParam,
        Count
    };


    void Initialize(void)
    {
        s_RootSignature.Reset((UINT)VoxelVisualizationParams::Count, 0);
        s_RootSignature[(UINT)VoxelVisualizationParams::Texture3d].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
        s_RootSignature[(UINT)VoxelVisualizationParams::GSParam].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_GEOMETRY);
        s_RootSignature[(UINT)VoxelVisualizationParams::PSParam].InitAsConstantBuffer(1, D3D12_SHADER_VISIBILITY_PIXEL);
        s_voxel_visualPSO.SetRootSignature(s_RootSignature);
        s_voxel_visualPSO.SetVertexShader(SHADER_ARGS(g_pVoxelVisualizeVS));
        s_voxel_visualPSO.SetGeometryShader(SHADER_ARGS(g_pVoxelVisualizeGS));
        s_voxel_visualPSO.SetPixelShader(SHADER_ARGS(g_pVoxelVisualizePS));
        D3D12_INPUT_ELEMENT_DESC vertElem[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };
        gsl::span<const D3D12_INPUT_ELEMENT_DESC> input_element_decs = gsl::make_span(vertElem);
        s_voxel_visualPSO.SetRasterizerState(Graphics::RasterizerTwoSided);
        s_voxel_visualPSO.SetInputLayout(input_element_decs.size(), input_element_decs.data());
        s_voxel_visualPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
        s_voxel_visualPSO.SetRenderTargetFormats(1, &Graphics::g_SceneColorBuffer.GetFormat(), Graphics::g_SceneDepthBuffer.GetFormat());
        s_voxel_visualPSO.SetBlendState(Graphics::BlendTraditional);
        s_voxel_visualPSO.SetDepthStencilState(Graphics::DepthStateDisabled);
        s_voxel_visualPSO.Finalize();
    }

    void VoxelVisualize::InitMesh(const size_t res_num)
    {
        assert(m_visual_mesh == nullptr);
        m_visual_mesh = std::make_unique< VisualMesh>();
        m_visual_mesh->Create(res_num);
    }

    void VoxelVisualize::Visualize(GraphicsContext& context, const Texture3D& tex)
    {
        assert(m_visual_mesh != nullptr);
        context.SetRootSignature(s_RootSignature);
        context.SetPipelineState(s_voxel_visualPSO);
        context.SetDynamicDescriptor(0, 0, tex.GetSRV());
        m_visual_mesh->Draw(context);
    }
}

