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

    ExpVar s_voxelVisualizationAlpha("Voxel/Voxel Alpha", 1.0f, 0.0f, 1.0f);
    ExpVar s_borderWidth("Voxel/Border Width", 0.05f, 0.0f, 1.0f);

    enum class VoxelVisualizationParams: U8
    {
        Texture3d,
        GSParam,
        PSParam,
        Count
    };


    VoxelVisualizationGSBuffer s_gsbuffer;
    VoxelVisualizationPSBuffer s_psbuffer;

    

    void Initialize(void)
    {
        s_RootSignature.Reset((UINT)VoxelVisualizationParams::Count, 0);
        s_RootSignature[(UINT)VoxelVisualizationParams::Texture3d].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
        s_RootSignature[(UINT)VoxelVisualizationParams::GSParam].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_GEOMETRY);
        s_RootSignature[(UINT)VoxelVisualizationParams::PSParam].InitAsConstantBuffer(1, D3D12_SHADER_VISIBILITY_PIXEL);
        s_RootSignature.Finalize(L"Voxel Visualize", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
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
        s_voxel_visualPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT);
        DXGI_FORMAT gBufferFormats[] = { g_GBufferColorBuffer.GetFormat(), g_GBufferNormalBuffer.GetFormat(), g_GBufferMaterialBuffer.GetFormat() };
        s_voxel_visualPSO.SetRenderTargetFormats(3, gBufferFormats, Graphics::g_SceneDepthBuffer.GetFormat());
        s_voxel_visualPSO.SetBlendState(Graphics::BlendDisable);
        s_voxel_visualPSO.SetDepthStencilState(Graphics::DepthStateGreatEqual);
        s_voxel_visualPSO.Finalize();
    }

    void VoxelVisualize::InitMesh(const size_t res_num)
    {
        assert(m_visual_mesh == nullptr);
        m_visual_mesh = std::make_unique< VisualMesh>();
        m_visual_mesh->Create(res_num);
    }

    void VoxelVisualize::DrawClip(GraphicsContext& context, const D3D12_CPU_DESCRIPTOR_HANDLE& texSrv)
    {
        assert(m_visual_mesh != nullptr);
        context.SetRootSignature(s_RootSignature);
        context.SetPipelineState(s_voxel_visualPSO);
        context.SetDynamicDescriptor((UINT)VoxelVisualizationParams::Texture3d, 0, texSrv);
        context.SetDynamicConstantBufferView((UINT)VoxelVisualizationParams::GSParam, sizeof(s_gsbuffer), &s_gsbuffer);
        context.SetDynamicConstantBufferView((UINT)VoxelVisualizationParams::PSParam, sizeof(s_psbuffer), &s_psbuffer);
        m_visual_mesh->Draw(context);
    }

    U32x3 convertFormat(glm::ivec3 v)
    {
        return U32x3{ (U32)v.x, (U32)v.y, (U32)v.z };
    }

    void VoxelVisualize::Visualize3DClipmapGS( const VoxelRegion& region, U32 clipmapLevel, const VoxelRegion& prevRegion, const Math::Matrix4& mvp,
        bool hasPrevLevel, bool hasFaces, int numColorComponents)
    {
        s_gsbuffer.u_clipmapResolution = int(VOXEL_RESOLUTION);
        s_gsbuffer.u_clipmapLevel = int(clipmapLevel);
        s_gsbuffer.u_voxelSize = region.voxelSize;
        s_gsbuffer.u_viewProj = mvp;
        s_gsbuffer.u_imageMin = convertFormat(region.getMinPosImage(region.extent));
        s_gsbuffer.u_regionMin = convertFormat(region.minPos);
        s_gsbuffer.u_prevRegionMin = convertFormat(prevRegion.minPos / 2);
        s_gsbuffer.u_prevRegionMax = convertFormat(prevRegion.getMaxPos() / 2);
        s_psbuffer.u_alpha = s_voxelVisualizationAlpha;
        s_psbuffer.u_borderWidth = s_borderWidth;

        s_gsbuffer.u_hasPrevClipmapLevel = hasPrevLevel;
        s_gsbuffer.u_hasMultipleFaces = hasFaces;
        s_gsbuffer.u_numColorComponents = numColorComponents;
    }
}

