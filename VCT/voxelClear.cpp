#include "voxelClear.hpp"
#include "CompiledShaders/ClearClipMapCS.h"

namespace VoxelClear
{
    RootSignature s_RootSignature;
    ComputePSO s_ClearVoxelCS;
    void Initialize(void)
    {
        s_RootSignature.Reset(4, 0);
        s_RootSignature[0].InitAsConstantBuffer(0);
        s_RootSignature[1].InitAsBufferUAV(0);
        s_RootSignature.Finalize(L"Reset Voxel");
        s_ClearVoxelCS.SetRootSignature(s_RootSignature);
        s_ClearVoxelCS.SetComputeShader(SHADER_ARGS(g_pClearClipMapCS));
        s_ClearVoxelCS.Finalize();
    }



    void Apply(ComputeContext context)
    {

        context.SetRootSignature(s_RootSignature);
        context.SetPipelineState(s_ClearVoxelCS);
        context.SetDynamicConstantBufferView(0, sizeof(cbv), &cbv);
        context.TransitionResource(tex, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        context.GetCommandList()->SetComputeRootUnorderedAccessView(1, tex.GetGpuVirtualAddress());
        context.Dispatch3D(VOXEL_RESOLUTION, VOXEL_RESOLUTION, VOXEL_RESOLUTION,8,8,8);
        
    }
}