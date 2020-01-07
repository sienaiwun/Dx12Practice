#include "SkyRS.hlsli"
TextureCube<float3> g_sky:register(t0);
SamplerState sampler0 : register(s0);
SamplerComparisonState shadowSampler : register(s1);

struct PSInputWorldPosition {
    float4 p            : SV_POSITION;
    float3 p_world      : POSITION0;
};


[RootSignature(Sky_RootSig)]
float4 main(PSInputWorldPosition input) : SV_Target{
    return float4(g_sky.Sample(sampler0, input.p_world), 1.0f);
}