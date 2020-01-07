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
// Author:  James Stanard 
//
#include "../../Core/hlsl.hpp"
#define ModelViewer_RootSig \
      "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |"\
    "DENY_HULL_SHADER_ROOT_ACCESS|"\
    "DENY_DOMAIN_SHADER_ROOT_ACCESS| "\
    "DENY_GEOMETRY_SHADER_ROOT_ACCESS), " \
    ADD_CBUFFER_VIEW_VISIBILITY(SLOT_CBUFFER_CAMERA, SHADER_VISIBILITY_VERTEX) ", " \
    ADD_CBUFFER_VIEW_VISIBILITY(SLOT_CBUFFER_LIGHT, SHADER_VISIBILITY_PIXEL) ", "  \
    ADD_CBUFFER_VIEW_VISIBILITY(SLOT_CBUFFER_WORLD, SHADER_VISIBILITY_VERTEX) ", " \
	ADD_CBUFFER_VIEW_VISIBILITY(SLOT_CBUFFER_WORLD, SHADER_VISIBILITY_PIXEL) ", " \
	ADD_CBUFFER_VIEW_VISIBILITY(SLOT_CBUFFER_GAME, SHADER_VISIBILITY_PIXEL) ", " \
    "DescriptorTable(SRV(t0, numDescriptors = 6), visibility = SHADER_VISIBILITY_PIXEL)," \
    "DescriptorTable(SRV(t64, numDescriptors = 6), visibility = SHADER_VISIBILITY_PIXEL)," \
	"DescriptorTable(SRV(t32, numDescriptors = 4), visibility = SHADER_VISIBILITY_PIXEL)," \
    "RootConstants(b1, num32BitConstants = 2, visibility = SHADER_VISIBILITY_VERTEX), " \
    "StaticSampler(s0, maxAnisotropy = 8, visibility = SHADER_VISIBILITY_PIXEL)," \
    "StaticSampler(s1, visibility = SHADER_VISIBILITY_PIXEL," \
        "addressU = TEXTURE_ADDRESS_CLAMP," \
        "addressV = TEXTURE_ADDRESS_CLAMP," \
        "addressW = TEXTURE_ADDRESS_CLAMP," \
        "comparisonFunc = COMPARISON_GREATER_EQUAL," \
        "filter = FILTER_MIN_MAG_LINEAR_MIP_POINT)"
