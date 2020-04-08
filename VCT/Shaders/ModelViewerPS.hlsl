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
#include "LightGrid.hlsli"

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
//Texture2D<float4> texEmissive        : register(t2);
Texture2D<float3> texNormal            : register(t3);
//Texture2D<float4> texLightmap        : register(t4);
//Texture2D<float4> texReflection    : register(t5);
#include "Lighting.hlsli"


// options for F+ variants and optimizations
#ifdef _WAVE_OP // SM 6.0 (new shader compiler)

// choose one of these:
//# define BIT_MASK
# define BIT_MASK_SORTED
//# define SCALAR_LOOP
//# define SCALAR_BRANCH

// enable to amortize latency of vector read in exchange for additional VGPRs being held
# define LIGHT_GRID_PRELOADING

// configured for 32 sphere lights, 64 cone lights, and 32 cone shadowed lights
# define POINT_LIGHT_GROUPS            1
# define SPOT_LIGHT_GROUPS            2
# define SHADOWED_SPOT_LIGHT_GROUPS    1
# define POINT_LIGHT_GROUPS_TAIL            POINT_LIGHT_GROUPS
# define SPOT_LIGHT_GROUPS_TAIL                POINT_LIGHT_GROUPS_TAIL + SPOT_LIGHT_GROUPS
# define SHADOWED_SPOT_LIGHT_GROUPS_TAIL    SPOT_LIGHT_GROUPS_TAIL + SHADOWED_SPOT_LIGHT_GROUPS


uint GetGroupBits(uint groupIndex, uint tileIndex, uint lightBitMaskGroups[4])
{
#ifdef LIGHT_GRID_PRELOADING
    return lightBitMaskGroups[groupIndex];
#else
    return lightGridBitMask.Load(tileIndex * 16 + groupIndex * 4);
#endif
}

uint64_t Ballot64(bool b)
{
    uint4 ballots = WaveActiveBallot(b);
    return (uint64_t)ballots.y << 32 | (uint64_t)ballots.x;
}

#endif // _WAVE_OP

// Helper function for iterating over a sparse list of bits.  Gets the offset of the next
// set bit, clears it, and returns the offset.
uint PullNextBit( inout uint bits )
{
    uint bitIndex = firstbitlow(bits);
    bits ^= 1 << bitIndex;
    return bitIndex;
}
float2 EncodeUnitVector_CryEngine(float3 u)
{
	//https://aras-p.info/texts/CompactNormalStorage.html
	return normalize(u.xy)*sqrt(u.z*0.5f + 0.5f);
}
[RootSignature(ModelViewer_RootSig)]
float3 main(VSOutput vsOutput) : SV_Target0
{
    uint2 pixelPos = vsOutput.position.xy;
    float3 diffuseAlbedo = texDiffuse.Sample(sampler0, vsOutput.uv);
    float3 colorSum = 0;
    {
        float ao = texSSAO[pixelPos];
        colorSum += ApplyAmbientLight( diffuseAlbedo, ao, AmbientColor );
    }

    float gloss = 128.0;
    float3 normal;
    {
        normal = texNormal.Sample(sampler0, vsOutput.uv) * 2.0 - 1.0;
        AntiAliasSpecular(normal, gloss);
        float3x3 tbn = float3x3(normalize(vsOutput.tangent), normalize(vsOutput.bitangent), normalize(vsOutput.normal));
        normal = normalize(mul(normal, tbn));
    }
    float3 specularAlbedo = float3( 0.56, 0.56, 0.56 );
    float specularMask = texSpecular.Sample(sampler0, vsOutput.uv).g;
    float3 viewDir = normalize(vsOutput.viewDir);
    colorSum += ApplyDirectionalLight( diffuseAlbedo, specularAlbedo, specularMask, gloss, normal, viewDir, SunDirection, SunColor, vsOutput.shadowCoord );

    uint2 tilePos = GetTilePos(pixelPos, InvTileDim.xy);
    uint tileIndex = GetTileIndex(tilePos, TileCount.x);
    uint tileOffset = GetTileOffset(tileIndex);

    // Light Grid Preloading setup
    uint lightBitMaskGroups[4] = { 0, 0, 0, 0 };
#if defined(LIGHT_GRID_PRELOADING)
    uint4 lightBitMask = lightGridBitMask.Load4(tileIndex * 16);
    
    lightBitMaskGroups[0] = lightBitMask.x;
    lightBitMaskGroups[1] = lightBitMask.y;
    lightBitMaskGroups[2] = lightBitMask.z;
    lightBitMaskGroups[3] = lightBitMask.w;
#endif

#define POINT_LIGHT_ARGS \
    diffuseAlbedo, \
    specularAlbedo, \
    specularMask, \
    gloss, \
    normal, \
    viewDir, \
    vsOutput.worldPos, \
    lightData.pos, \
    lightData.radiusSq, \
    lightData.color

#define CONE_LIGHT_ARGS \
    POINT_LIGHT_ARGS, \
    lightData.coneDir, \
    lightData.coneAngles

#define SHADOWED_LIGHT_ARGS \
    CONE_LIGHT_ARGS, \
    lightData.shadowTextureMatrix, \
    lightIndex

#if defined(BIT_MASK)
    uint64_t threadMask = Ballot64(tileIndex != ~0); // attempt to get starting exec mask

    for (uint groupIndex = 0; groupIndex < 4; groupIndex++)
    {
        // combine across threads
        uint groupBits = WaveActiveBitOr(GetGroupBits(groupIndex, tileIndex, lightBitMaskGroups));

        while (groupBits != 0)
        {
            uint bitIndex = PullNextBit(groupBits);
            uint lightIndex = 32 * groupIndex + bitIndex;

            LightData lightData = lightBuffer[lightIndex];

            if (lightIndex < FirstLightIndex.x) // sphere
            {
                colorSum += ApplyPointLight(POINT_LIGHT_ARGS);
            }
            else if (lightIndex < FirstLightIndex.y) // cone
            {
                colorSum += ApplyConeLight(CONE_LIGHT_ARGS);
            }
            else // cone w/ shadow map
            {
                colorSum += ApplyConeShadowedLight(SHADOWED_LIGHT_ARGS);
            }
        }
    }

#elif defined(BIT_MASK_SORTED)

    // Get light type groups - these can be predefined as compile time constants to enable unrolling and better scheduling of vector reads
    uint pointLightGroupTail        = POINT_LIGHT_GROUPS_TAIL;
    uint spotLightGroupTail            = SPOT_LIGHT_GROUPS_TAIL;
    uint spotShadowLightGroupTail    = SHADOWED_SPOT_LIGHT_GROUPS_TAIL;

    uint groupBitsMasks[4] = { 0, 0, 0, 0 };
    for (int i = 0; i < 4; i++)
    {
        // combine across threads
        groupBitsMasks[i] = WaveActiveBitOr(GetGroupBits(i, tileIndex, lightBitMaskGroups));
    }

    for (uint groupIndex = 0; groupIndex < pointLightGroupTail; groupIndex++)
    {
        uint groupBits = groupBitsMasks[groupIndex];

        while (groupBits != 0)
        {
            uint bitIndex = PullNextBit(groupBits);
            uint lightIndex = 32 * groupIndex + bitIndex;

            // sphere
            LightData lightData = lightBuffer[lightIndex];
            colorSum += ApplyPointLight(POINT_LIGHT_ARGS);
        }
    }

    for (uint groupIndex = pointLightGroupTail; groupIndex < spotLightGroupTail; groupIndex++)
    {
        uint groupBits = groupBitsMasks[groupIndex];

        while (groupBits != 0)
        {
            uint bitIndex = PullNextBit(groupBits);
            uint lightIndex = 32 * groupIndex + bitIndex;

            // cone
            LightData lightData = lightBuffer[lightIndex];
            colorSum += ApplyConeLight(CONE_LIGHT_ARGS);
        }
    }

    for (uint groupIndex = spotLightGroupTail; groupIndex < spotShadowLightGroupTail; groupIndex++)
    {
        uint groupBits = groupBitsMasks[groupIndex];

        while (groupBits != 0)
        {
            uint bitIndex = PullNextBit(groupBits);
            uint lightIndex = 32 * groupIndex + bitIndex;

            // cone w/ shadow map
            LightData lightData = lightBuffer[lightIndex];
            colorSum += ApplyConeShadowedLight(SHADOWED_LIGHT_ARGS);
        }
    }

#elif defined(SCALAR_LOOP)
    uint64_t threadMask = Ballot64(tileOffset != ~0); // attempt to get starting exec mask
    uint64_t laneBit = 1ull << WaveGetLaneIndex();

    while ((threadMask & laneBit) != 0) // is this thread waiting to be processed?
    { // exec is now the set of remaining threads
        // grab the tile offset for the first active thread
        uint uniformTileOffset = WaveReadLaneFirst(tileOffset);
        // mask of which threads have the same tile offset as the first active thread
        uint64_t uniformMask = Ballot64(tileOffset == uniformTileOffset);

        if (any((uniformMask & laneBit) != 0)) // is this thread one of the current set of uniform threads?
        {
            uint tileLightCount = lightGrid.Load(uniformTileOffset + 0);
            uint tileLightCountSphere = (tileLightCount >> 0) & 0xff;
            uint tileLightCountCone = (tileLightCount >> 8) & 0xff;
            uint tileLightCountConeShadowed = (tileLightCount >> 16) & 0xff;

            uint tileLightLoadOffset = uniformTileOffset + 4;

            // sphere
            for (uint n = 0; n < tileLightCountSphere; n++, tileLightLoadOffset += 4)
            {
                uint lightIndex = lightGrid.Load(tileLightLoadOffset);
                LightData lightData = lightBuffer[lightIndex];
                colorSum += ApplyPointLight(POINT_LIGHT_ARGS);
            }

            // cone
            for (uint n = 0; n < tileLightCountCone; n++, tileLightLoadOffset += 4)
            {
                uint lightIndex = lightGrid.Load(tileLightLoadOffset);
                LightData lightData = lightBuffer[lightIndex];
                colorSum += ApplyConeLight(CONE_LIGHT_ARGS);
            }

            // cone w/ shadow map
            for (uint n = 0; n < tileLightCountConeShadowed; n++, tileLightLoadOffset += 4)
            {
                uint lightIndex = lightGrid.Load(tileLightLoadOffset);
                LightData lightData = lightBuffer[lightIndex];
                colorSum += ApplyConeShadowedLight(SHADOWED_LIGHT_ARGS);
            }
        }

        // strip the current set of uniform threads from the exec mask for the next loop iteration
        threadMask &= ~uniformMask;
    }

#elif defined(SCALAR_BRANCH)

    if (Ballot64(tileOffset == WaveReadLaneFirst(tileOffset)) == ~0ull)
    {
        // uniform branch
        tileOffset = WaveReadLaneFirst(tileOffset);

        uint tileLightCount = lightGrid.Load(tileOffset + 0);
        uint tileLightCountSphere = (tileLightCount >> 0) & 0xff;
        uint tileLightCountCone = (tileLightCount >> 8) & 0xff;
        uint tileLightCountConeShadowed = (tileLightCount >> 16) & 0xff;

        uint tileLightLoadOffset = tileOffset + 4;

        // sphere
        for (uint n = 0; n < tileLightCountSphere; n++, tileLightLoadOffset += 4)
        {
            uint lightIndex = lightGrid.Load(tileLightLoadOffset);
            LightData lightData = lightBuffer[lightIndex];
            colorSum += ApplyPointLight(POINT_LIGHT_ARGS);
        }

        // cone
        for (uint n = 0; n < tileLightCountCone; n++, tileLightLoadOffset += 4)
        {
            uint lightIndex = lightGrid.Load(tileLightLoadOffset);
            LightData lightData = lightBuffer[lightIndex];
            colorSum += ApplyConeLight(CONE_LIGHT_ARGS);
        }

        // cone w/ shadow map
        for (uint n = 0; n < tileLightCountConeShadowed; n++, tileLightLoadOffset += 4)
        {
            uint lightIndex = lightGrid.Load(tileLightLoadOffset);
            LightData lightData = lightBuffer[lightIndex];
            colorSum += ApplyConeShadowedLight(SHADOWED_LIGHT_ARGS);
        }
    }
    else
    {
        // divergent branch
        uint tileLightCount = lightGrid.Load(tileOffset + 0);
        uint tileLightCountSphere = (tileLightCount >> 0) & 0xff;
        uint tileLightCountCone = (tileLightCount >> 8) & 0xff;
        uint tileLightCountConeShadowed = (tileLightCount >> 16) & 0xff;

        uint tileLightLoadOffset = tileOffset + 4;

        // sphere
        for (uint n = 0; n < tileLightCountSphere; n++, tileLightLoadOffset += 4)
        {
            uint lightIndex = lightGrid.Load(tileLightLoadOffset);
            LightData lightData = lightBuffer[lightIndex];
            colorSum += ApplyPointLight(POINT_LIGHT_ARGS);
        }

        // cone
        for (uint n = 0; n < tileLightCountCone; n++, tileLightLoadOffset += 4)
        {
            uint lightIndex = lightGrid.Load(tileLightLoadOffset);
            LightData lightData = lightBuffer[lightIndex];
            colorSum += ApplyConeLight(CONE_LIGHT_ARGS);
        }

        // cone w/ shadow map
        for (uint n = 0; n < tileLightCountConeShadowed; n++, tileLightLoadOffset += 4)
        {
            uint lightIndex = lightGrid.Load(tileLightLoadOffset);
            LightData lightData = lightBuffer[lightIndex];
            colorSum += ApplyConeShadowedLight(SHADOWED_LIGHT_ARGS);
        }
    }

#else // SM 5.0 (no wave intrinsics)

    uint tileLightCount = lightGrid.Load(tileOffset + 0);
    uint tileLightCountSphere = (tileLightCount >> 0) & 0xff;
    uint tileLightCountCone = (tileLightCount >> 8) & 0xff;
    uint tileLightCountConeShadowed = (tileLightCount >> 16) & 0xff;

    uint tileLightLoadOffset = tileOffset + 4;

    // sphere
    for (uint n = 0; n < tileLightCountSphere; n++, tileLightLoadOffset += 4)
    {
        uint lightIndex = lightGrid.Load(tileLightLoadOffset);
        LightData lightData = lightBuffer[lightIndex];
        colorSum += ApplyPointLight(POINT_LIGHT_ARGS);
    }

    // cone
    for (uint n = 0; n < tileLightCountCone; n++, tileLightLoadOffset += 4)
    {
        uint lightIndex = lightGrid.Load(tileLightLoadOffset);
        LightData lightData = lightBuffer[lightIndex];
        colorSum += ApplyConeLight(CONE_LIGHT_ARGS);
    }

    // cone w/ shadow map
    for (uint n = 0; n < tileLightCountConeShadowed; n++, tileLightLoadOffset += 4)
    {
        uint lightIndex = lightGrid.Load(tileLightLoadOffset);
        LightData lightData = lightBuffer[lightIndex];
        colorSum += ApplyConeShadowedLight(SHADOWED_LIGHT_ARGS);
    }
#endif

    return colorSum;
}
