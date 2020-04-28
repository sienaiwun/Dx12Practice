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
        s_RootSignature.Reset(3, 0);
        s_RootSignature[0].InitAsConstantBuffer(0,D3D12_SHADER_VISIBILITY_GEOMETRY);
        s_RootSignature[1].InitAsConstantBuffer(1, D3D12_SHADER_VISIBILITY_PIXEL);
        s_RootSignature[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1, D3D12_SHADER_VISIBILITY_PIXEL);
        s_RootSignature.Finalize(L"Voxelization", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
        s_voxelPSO.SetRootSignature(s_RootSignature);
        s_voxelPSO.SetVertexShader(SHADER_ARGS(g_pConservertiveVoxelPassVS));
        s_voxelPSO.SetGeometryShader(SHADER_ARGS(g_pConservertiveVoxelPassGS));
        s_voxelPSO.SetPixelShader(SHADER_ARGS(g_pConservertiveVoxelPassPS));


        D3D12_INPUT_ELEMENT_DESC vertElem[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };
        gsl::span<const D3D12_INPUT_ELEMENT_DESC> input_element_decs = gsl::make_span(vertElem);

        s_voxelPSO.SetRasterizerState(Graphics::RasterizerDefault);
        s_voxelPSO.SetInputLayout(input_element_decs.size(), input_element_decs.data());
        s_voxelPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
        s_voxelPSO.SetRenderTargetFormats(0, nullptr, DXGI_FORMAT_UNKNOWN);

        s_voxelPSO.Finalize();
    }

    voxelizationGSCBuffer s_gsbuffer;
    voxelizationPSCBuffer s_psbuffer;

    void setViewports(GraphicsContext& context, const Dagon::Vec3f viewportSize)
    {
        F32x2 viewportSizes[3];
        viewportSizes[0] = F32x2{ viewportSize.z, viewportSize.y };
        viewportSizes[1] = F32x2{ viewportSize.x, viewportSize.z };
        viewportSizes[2] = F32x2{ viewportSize.x, viewportSize.y };
        D3D12_VIEWPORT viewports[3];
        D3D12_RECT rects[3];
        for (int i = 0; i < 3; i++)
        {
            viewports[i].Width = viewportSizes[i][0];
            viewports[i].Height = viewportSizes[i][1];
            viewports[i].MinDepth = 0.0f;
            viewports[i].MaxDepth = 1.0f;
            viewports[i].TopLeftX = 0.0f;
            viewports[i].TopLeftY = 0.0f;
            rects[i].left = 0;
            rects[i].top = 0;
            rects[i].right = (LONG)viewportSizes[i][0];
            rects[i].bottom = (LONG)viewportSizes[i][1];
        }
        context.SetViewportAndScissors(3, viewports, rects);
        memcpy(s_gsbuffer.u_viewportSizes, viewportSizes, 3 * sizeof(F32x2));
    }
    
   
    Math::Matrix4 LookAt(Dagon::Vec3f Eye, Dagon::Vec3f LookAt, Dagon::Vec3f LookUp)
    {
        XMVECTOR EyeV = XMVectorSet(Eye.x, Eye.y, Eye.z, 0.0f);
        XMVECTOR LookAtV = XMVectorSet(LookAt.x, LookAt.y, LookAt.z, 0.0f);
        XMVECTOR Up = XMVectorSet(LookUp.x, LookUp.y, LookUp.z, 0.0f);
        XMMATRIX View = XMMatrixLookAtRH(EyeV, LookAtV, Up);
        return Math::Matrix4(View);
    }

    void SetViewProjectionMatris(const VoxelRegion& voxelRegion)
    {
        Dagon::Vec3f size = voxelRegion.getExtentWorld();
        Matrix4 proj[3];

        proj[0] = Matrix4(XMMatrixOrthographicOffCenterRH(-size.z, 0.0f, 0.0f, size.y, 0.0f, size.x));
        proj[1] = Matrix4(XMMatrixOrthographicOffCenterRH(0.0f, size.x, 0.0f, size.z, 0.0f, size.y));
        proj[2] = Matrix4(XMMatrixOrthographicOffCenterRH(0.0f, size.x, 0.0f, size.y, 0.0f, size.z));

        Matrix4 viewProj[3];
        Matrix4 viewProjInv[3];

        Dagon::Vec3f xyStart = voxelRegion.getMinPosWorld() + glm::vec3(0.0f, 0.0f, size.z);

        viewProj[0] = LookAt(xyStart, xyStart + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        viewProj[1] = LookAt(xyStart, xyStart + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f));
        viewProj[2] = LookAt(voxelRegion.getMinPosWorld(), voxelRegion.getMinPosWorld() + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        for (int i = 0; i < 3; ++i)
        {
            viewProj[i] = proj[i] * viewProj[i];
            viewProjInv[i] = Math::Invert(viewProj[i]);
        }
        memcpy(s_gsbuffer.u_viewProj, viewProj, 3 * sizeof(Matrix4));
        memcpy(s_gsbuffer.u_viewProjInv, viewProjInv, 3 * sizeof(Matrix4));
    }
    
    F32x3 convertFormat(Dagon::Vec3f v)
    {
        return F32x3{v.x, v.y, v.z };
    }

    void Render(GraphicsContext& context, const VoxelRegion& voxelRegion, const int clip_level)
    {
        SceneView::World * world = SceneView::World::Get();
        std::vector<VoxelRegion>& clipRegions = world->GetVoxelization().GetClieRegions();
        Dagon::Vec3f regionMinWorld = voxelRegion.getMinPosWorld() - Dagon::EPSILON5;
        Dagon::Vec3f regionMaxWorld = voxelRegion.getMaxPosWorld() + Dagon::EPSILON5;

        VoxelRegion extendedRegion = voxelRegion;
        extendedRegion.extent = voxelRegion.extent + 2;
        extendedRegion.minPos -= 1;

        setViewports(context, Dagon::Vec3f(extendedRegion.extent));
        SetViewProjectionMatris( extendedRegion);

        s_psbuffer.u_regionMin = convertFormat(regionMinWorld);
        s_psbuffer.u_regionMax = convertFormat(regionMaxWorld);
        s_psbuffer.u_clipmapLevel = clip_level;
        s_psbuffer.u_maxExtent = clipRegions[clip_level].getExtentWorld().x;
        s_psbuffer.u_voxelSize = clipRegions[clip_level].voxelSize;

        if (clip_level > 0)
        {
            s_psbuffer.u_prevRegionMin = convertFormat(clipRegions[clip_level - 1].getMinPosWorld());
            s_psbuffer.u_prevRegionMax = convertFormat(clipRegions[clip_level - 1].getMaxPosWorld());
      
        }
        context.SetRootSignature(s_RootSignature);
        context.SetPipelineState(s_voxelPSO);
        context.SetDynamicConstantBufferView(0, sizeof(s_gsbuffer), &s_gsbuffer);
        context.SetDynamicConstantBufferView(1, sizeof(s_psbuffer), &s_psbuffer);
        context.SetDynamicDescriptor(2, 0, world->GetVoxelization().VoxelOpacityUAV());

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