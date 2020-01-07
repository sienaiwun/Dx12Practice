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
// Author(s):  James Stanard
//             Alex Nankervis
//
#include "ModelViewerRS.hlsli"
#include "../../Core/Shaders/Buffers.hlsli"

cbuffer modelInfo: register(b1)
{
	uint basevertex;
	uint materialId;
};

struct VSInput
{
    float3 position : POSITION;
    float2 texcoord0 : TEXCOORD;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
};

struct VSOutput
{
    float4 position : SV_Position;
    float3 worldPos : WorldPos;
    float2 texCoord : TexCoord0;
    float3 viewDir : TexCoord1;
    float3 shadowCoord : TexCoord2;
    float3 normal : Normal;
    float3 tangent : Tangent;
    float3 bitangent : Bitangent;
};

[RootSignature(ModelViewer_RootSig)]
VSOutput main(VSInput vsInput)
{
    VSOutput vsOutput;

    vsOutput.position = mul(modelToProjection, float4(vsInput.position, 1.0));
    vsOutput.worldPos = vsInput.position;
    vsOutput.texCoord = vsInput.texcoord0;
    vsOutput.viewDir = vsInput.position - g_viewer_pos;
    vsOutput.shadowCoord = mul(g_model_to_shadow, float4(vsInput.position, 1.0)).xyz;

    vsOutput.normal = vsInput.normal;
    vsOutput.tangent = vsInput.tangent;
    vsOutput.bitangent = vsInput.bitangent;

    return vsOutput;
}
