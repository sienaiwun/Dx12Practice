#include "voxelClear.hpp"
#include "CompiledShaders/ClearClipMapCS.h"

namespace VoxelClear
{
    RootSignature s_RootSignature;
    ComputePSO s_ClearVoxelCS;

    ConstantBuffer cbv;

    D3D12_CPU_DESCRIPTOR_HANDLE handle;

    void Initialize(void)
    {
        s_RootSignature.Reset(2, 0);
        s_RootSignature[0].InitAsConstantBuffer(0);
        s_RootSignature[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);
        s_RootSignature.Finalize(L"Reset Voxel");
        s_ClearVoxelCS.SetRootSignature(s_RootSignature);
        s_ClearVoxelCS.SetComputeShader(SHADER_ARGS(g_pClearClipMapCS));
        s_ClearVoxelCS.Finalize();
    }

    void Apply(ComputeContext& context)
    {
        context.SetRootSignature(s_RootSignature);
        context.SetPipelineState(s_ClearVoxelCS);
        context.SetDynamicConstantBufferView(0, sizeof(cbv), &cbv);
        context.SetDynamicDescriptor(1, 0, handle);
        context.Dispatch3D(VOXEL_RESOLUTION, VOXEL_RESOLUTION, VOXEL_RESOLUTION,8,8,8);
    }

    void SetConstantBuffer(ConstantBuffer buffer)
    {
        cbv = buffer;
    }

    void SetUAV(D3D12_CPU_DESCRIPTOR_HANDLE _handle)
    {
        handle = _handle;
    }

}