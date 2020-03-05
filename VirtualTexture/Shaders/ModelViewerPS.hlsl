//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author(s):    James Stanard
//                Alex Nankervis
//
// Thanks to Michal Drobot for his feedback.

#include "ModelViewerRS.hlsli"

// outdated warning about for-loop variable scope
#pragma warning (disable: 3078)
// single-iteration loop
#pragma warning (disable: 3557)

struct VSOutput
{
    sample float4 position : SV_Position;
    sample float3 worldPos : WorldPos;
    sample float2 uv : TexCoord0;
    sample float3 viewDir : TexCoord1;
    sample float3 shadowCoord : TexCoord2;
    sample float3 normal : Normal;
    sample float3 tangent : Tangent;
    sample float3 bitangent : Bitangent;
};

Texture2D<float3> texDiffuse        : register(t0);
Texture2D<float3> texSpecular        : register(t1);
Texture2D<float4> texTiled        : register(t2);
Texture2D<float3> texNormal            : register(t3);
//Texture2D<float4> texLightmap        : register(t4);
//Texture2D<float4> texReflection    : register(t5);
RWStructuredBuffer<uint> VisibilityBuffer : register(u1);
cbuffer TexlInfo: register(b1)
{
    uint maxLod;
    uint active_mip;
    uint Size;
    uint pageSize;
};


SamplerState sampler0 : register(s0);


[RootSignature(ModelViewer_RootSig)]
float3 main(VSOutput vsOutput) : SV_Target0
{
     float4 color = float4(0.0, 0.0, 0.0, 0.0);
     float LOD = texTiled.CalculateLevelOfDetailUnclamped(sampler0, vsOutput.uv);
    LOD = clamp(LOD, 0.0f, maxLod);
    uint residencyCode = 0;
    uint minLod = floor(LOD);
    minLod = clamp(minLod, 0.0f, maxLod);
    float localMinLod = minLod;
   
    [loop]
    do
    {
        color = texTiled.Sample(sampler0, vsOutput.uv, 0, floor(min(localMinLod, maxLod)), residencyCode);
        localMinLod += 1.0f;

        if (localMinLod > maxLod)
            break;

    } while (!CheckAccessFullyMapped(residencyCode));
   uint pageOffset = 0;

   uint pageCount = Size / pageSize;
   for (int i = 1; i <= int(minLod); i++)
   {
       pageOffset += pageCount * pageCount;

       pageCount = pageCount / 2;
   }

   pageOffset += uint(float(pageCount) *  vsOutput.uv.x) + uint(float(pageCount) *  vsOutput.uv.y) * pageCount;
   VisibilityBuffer[pageOffset] = 1;
   //color = texTiled.SampleLevel(sampler0, vsOutput.uv, active_mip);

    return color.xyz;
}
