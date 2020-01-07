#pragma region Header
#include "IndirectDraw.hpp"
#include "CompiledShaders/drawQuadVS.h"
#include "CompiledShaders/drawQuadPS.h"
#include "CompiledShaders/fillCS.h"
#pragma endregion

struct IndirectCommand
{
    D3D12_GPU_VIRTUAL_ADDRESS cbv;
    D3D12_DRAW_ARGUMENTS drawArguments;
};


__declspec(align(256)) struct BufferElement
{
    U32x2 StartIndex;
    U32 index;
    U32 _;
    F32x3 color;
    
};

namespace {
    RootSignature  s_ComputeSig;
    RootSignature s_GraphicsSig;
    GraphicsPSO s_graphicsPSO;
    ComputePSO s_ComputePSO;
    uint32_t s_width, s_height;
    D3D12_VIEWPORT s_MainViewport;
    D3D12_RECT s_MainScissor;
    U32x2 s_tileNum;
    U32x3 s_ThreadGroupSize = { 8,8,8 };
    StructuredBuffer s_mappedBuffer;
    StructuredBuffer s_commandBuffer;
    StructuredBuffer s_precessedCommandBuffer;
    CommandSignature s_indirectDrawSig(2);
    BoolVar s_enableGPUCUlling("Application/GPUCulling", true);
    enum ComputeRootParams :unsigned char
    {
        UniformBufferParam,
        UAVParam,
        StructBufferParam,
        NumComputeParams,
    };

    enum GraphicRootParams :unsigned char
    {
        UniformBufferParamCBv,
        PerDrawUniformBuffer,
        NumGraphicsParams,
    };

#pragma warning( disable : 4324 ) // Added padding.

    __declspec(align(16))struct WorldBufferConstants
    {
        F32x2 screen_res;
        F32 time;
        F32 padding;
        U32x2 tile_num;
        U32x2 tile_res;
    } gUniformData;

};


CREATE_APPLICATION(IndirectDraw)

void IndirectDraw::Startup(void)
{

   {
        ID3D12ShaderReflection* d3d12reflection = NULL;
        D3DReflect(SHADER_ARGS(g_pfillCS), IID_PPV_ARGS(&d3d12reflection));
        D3D12_SHADER_DESC shaderDesc;
        d3d12reflection->GetDesc(&shaderDesc);
        d3d12reflection->GetThreadGroupSize(
            &s_ThreadGroupSize[0], &s_ThreadGroupSize[1], &s_ThreadGroupSize[2]);
        s_width = g_SceneColorBuffer.GetWidth();
        s_height = g_SceneColorBuffer.GetHeight();
        s_tileNum = { Math::DivideByMultiple(s_width,s_ThreadGroupSize[0]), Math::DivideByMultiple(s_height,s_ThreadGroupSize[1]) };
        

    }
    {
        s_ComputeSig.Reset(ComputeRootParams::NumComputeParams, 0);
        s_ComputeSig[ComputeRootParams::UniformBufferParam].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_ALL);
        s_ComputeSig[ComputeRootParams::UAVParam].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);
        s_ComputeSig[ComputeRootParams::StructBufferParam].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
        s_ComputeSig.Finalize(L"Compute");
        s_ComputePSO.SetRootSignature(s_ComputeSig);
        s_ComputePSO.SetComputeShader(SHADER_ARGS(g_pfillCS));
        s_ComputePSO.Finalize();
    }
    {
        s_GraphicsSig.Reset(GraphicRootParams::NumGraphicsParams, 0);
        s_GraphicsSig[GraphicRootParams::UniformBufferParamCBv].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_ALL);
        s_GraphicsSig[GraphicRootParams::PerDrawUniformBuffer].InitAsConstantBuffer( 1, D3D12_SHADER_VISIBILITY_ALL);

        //s_GraphicsSig[GraphicRootParams::StructBufferParamSRV].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
        s_GraphicsSig.Finalize(L"Graphic");


        DXGI_FORMAT ColorFormat = g_SceneColorBuffer.GetFormat();

        s_graphicsPSO.SetRootSignature(s_GraphicsSig);
        D3D12_RASTERIZER_DESC resater = RasterizerTwoSided;
        resater.DepthClipEnable = FALSE;
        s_graphicsPSO.SetRasterizerState(resater);
        s_graphicsPSO.SetBlendState(BlendDisable);
        s_graphicsPSO.SetDepthStencilState(DepthStateReadWrite);
        s_graphicsPSO.SetInputLayout(0, nullptr);
        s_graphicsPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
        s_graphicsPSO.SetRenderTargetFormats(1, &ColorFormat, DXGI_FORMAT_UNKNOWN);
        s_graphicsPSO.SetVertexShader(SHADER_ARGS(g_pdrawQuadVS));
        s_graphicsPSO.SetPixelShader(SHADER_ARGS(g_pdrawQuadPS));
        s_graphicsPSO.Finalize();
       

    }
    {
        const uint32_t jobs_num = s_tileNum.product();
        std::vector<BufferElement> bufferElements = std::vector<BufferElement>(jobs_num);
        for (size_t n = 0; n < bufferElements.size(); n++)
        {
            bufferElements[n].index = (U32)n;
            bufferElements[n].StartIndex = { (uint32_t)n % s_tileNum[0], (uint32_t)n / s_tileNum[0] };
            RandomColor((float*)&bufferElements[n].color);
        }
        s_mappedBuffer.Create(L"mapped buffer", jobs_num, sizeof(BufferElement), bufferElements.data());
        const D3D12_GPU_VIRTUAL_ADDRESS first_address = s_mappedBuffer.GetGpuVirtualAddress();
        std::vector<IndirectCommand> commands (s_tileNum.product()) ;
        for (size_t i = 0; i < s_tileNum.product(); i++)
        {
            commands[i].cbv = first_address + i * sizeof(BufferElement);;
            commands[i].drawArguments.VertexCountPerInstance = 4;
            commands[i].drawArguments.InstanceCount = 1;
            commands[i].drawArguments.StartVertexLocation = 0;
            commands[i].drawArguments.StartInstanceLocation = 0;
        }
        s_precessedCommandBuffer.Create(L"Output draw buffer", s_tileNum.product(), sizeof(IndirectCommand), nullptr);
        s_commandBuffer.Create(L"indirect draw buffer", s_tileNum.product(), sizeof(IndirectCommand), commands.data());
        s_indirectDrawSig[0].ConstantBufferView(1);
        s_indirectDrawSig[1].Draw();
        s_indirectDrawSig.Finalize(&s_GraphicsSig);
    }


}

void IndirectDraw::Cleanup(void)
{
}

void IndirectDraw::Update(float )
{
}

void IndirectDraw::RenderScene(void)
{
    
    gUniformData.tile_res = { s_ThreadGroupSize[0], s_ThreadGroupSize[1] };
    gUniformData.tile_num = s_tileNum;
    gUniformData.time = GetFrameCount()*GetFrameTime();
    gUniformData.screen_res = { (float)s_width,(float)s_height };
    
    GraphicsContext& gfxContext = GraphicsContext::Begin(L"Compute");
    {
    ComputeContext& computeContext = gfxContext.GetComputeContext();
    computeContext.SetRootSignature(s_ComputeSig);
    computeContext.SetPipelineState(s_ComputePSO);
    static __declspec(align(16)) UINT zero = 0;
    gfxContext.TransitionResource(s_precessedCommandBuffer, D3D12_RESOURCE_STATE_COPY_DEST);
    computeContext.WriteBuffer(s_precessedCommandBuffer.GetCounterBuffer(), 0, &zero, sizeof(zero));//reset counter
    computeContext.TransitionResource(s_commandBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    computeContext.TransitionResource(s_precessedCommandBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);
    computeContext.SetDynamicDescriptor(ComputeRootParams::UAVParam, 0, s_precessedCommandBuffer.GetUAV());
    computeContext.SetDynamicDescriptor(ComputeRootParams::StructBufferParam, 0, s_commandBuffer.GetSRV());
    computeContext.SetDynamicConstantBufferView(ComputeRootParams::UniformBufferParam, sizeof(gUniformData), &gUniformData);
    computeContext.Dispatch2D(g_SceneColorBuffer.GetWidth(), g_SceneColorBuffer.GetHeight(), s_ThreadGroupSize[0], s_ThreadGroupSize[1]);
    }


   
    gfxContext.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
    gfxContext.SetRootSignature(s_GraphicsSig);
    gfxContext.SetPipelineState(s_graphicsPSO);
    gfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    gfxContext.ClearColor(g_SceneColorBuffer);
    gfxContext.SetRenderTarget(g_SceneColorBuffer.GetRTV());

    gfxContext.SetDynamicConstantBufferView(GraphicRootParams::UniformBufferParamCBv, sizeof(gUniformData), &gUniformData);

    s_MainViewport.Width = (float)g_SceneColorBuffer.GetWidth();
    s_MainViewport.Height = (float)g_SceneColorBuffer.GetHeight();
    s_MainViewport.MinDepth = 0.0f;
    s_MainViewport.MaxDepth = 1.0f;
    s_MainViewport.TopLeftX = 0.0f;
    s_MainViewport.TopLeftY = 0.0f;

    s_MainScissor.left = 0;
    s_MainScissor.top = 0;
    s_MainScissor.right = (LONG)g_SceneColorBuffer.GetWidth();
    s_MainScissor.bottom = (LONG)g_SceneColorBuffer.GetHeight();

    gfxContext.SetViewportAndScissor(s_MainViewport, s_MainScissor);
    if(!s_enableGPUCUlling)
    {
        gfxContext.TransitionResource(s_commandBuffer, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
        gfxContext.ExecuteIndirect(s_indirectDrawSig, s_commandBuffer, 0, s_tileNum.product());
    }
    else
    {
        gfxContext.TransitionResource(s_precessedCommandBuffer, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
        gfxContext.ExecuteIndirect(s_indirectDrawSig, s_precessedCommandBuffer, 0, s_tileNum.product(), &(s_precessedCommandBuffer.GetCounterBuffer()));
    }
    gfxContext.Finish();


}