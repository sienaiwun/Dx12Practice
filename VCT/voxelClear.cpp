#include "voxelClear.hpp"
#include "CompiledShaders/ClearClipMapCS.h"

namespace VoxelClear
{
    RootSignature s_RootSignature;
    ComputePSO s_ClearVoxelCS;

    ConstantBuffer cbv;

    Texture3D* p_tex = nullptr;

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
        ASSERT(p_tex);
        ScopedTimer _prof(L"Voxel", context);
        context.SetRootSignature(s_RootSignature);
        context.SetPipelineState(s_ClearVoxelCS);
        context.SetDynamicConstantBufferView(0, sizeof(cbv), &cbv);
        context.TransitionResource(*p_tex, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        context.SetDynamicDescriptor(1, 0, p_tex->GetUAV());
        context.Dispatch3D(VOXEL_RESOLUTION, VOXEL_RESOLUTION, VOXEL_RESOLUTION,8,8,8);
    }

    void SetConstantBuffer(ConstantBuffer buffer)
    {
        cbv = buffer;
    }

    void SetTexture3D(Texture3D* _tex)
    {
        p_tex = _tex;
    }
}