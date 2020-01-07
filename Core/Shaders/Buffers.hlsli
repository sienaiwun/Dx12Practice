#ifndef BUFFERS_HLSL
#define BUFFERS_HLSL
#include "../hlsl.hpp"

CBUFFER(CameraConstant, SLOT_CBUFFER_CAMERA)
{
    float4x4 modelToProjection;
};

CBUFFER(WorldConstant, SLOT_CBUFFER_WORLD)
{
    float4x4 g_projection_to_camera		: packoffset(c0);
    float4x4 g_camera_to_world			: packoffset(c4);
    float4x4 g_projection_to_world		: packoffset(c8);
    float4x4 g_model_to_shadow			: packoffset(c12);
    float4  g_inv_viewport				: packoffset(c16);
    float3 g_viewer_pos				    : packoffset(c17);
    float g_scene_lengh                 : packoffset(c17.w);
};

CBUFFER(LightConstant, SLOT_CBUFFER_LIGHT)
{
    float3 SunDirection;
    float3 SunColor;
    float3 AmbientColor;
    float4 ShadowTexelSize;

    float4 InvTileDim;
    uint4 TileCount;
    uint4 FirstLightIndex;
}



CBUFFER(GameConstant, SLOT_CBUFFER_GAME)
{
    float3 wireframe_color;
}
#endif