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
#include "../../Core/Shaders/Buffers.hlsli"
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
Texture2D<float> texSSAO            : register(t64);
Texture2D<float> texShadow            : register(t65);

StructuredBuffer<LightData> lightBuffer : register(t66);
Texture2DArray<float> lightShadowArrayTex : register(t67);
ByteAddressBuffer lightGrid : register(t68);
ByteAddressBuffer lightGridBitMask : register(t69);


SamplerState sampler0 : register(s0);
SamplerComparisonState shadowSampler : register(s1);

void AntiAliasSpecular(inout float3 texNormal, inout float gloss)
{
	float normalLenSq = dot(texNormal, texNormal);
	float invNormalLen = rsqrt(normalLenSq);
	texNormal *= invNormalLen;
	gloss = lerp(1, gloss, rcp(invNormalLen));
}

// Apply fresnel to modulate the specular albedo
void FSchlick(inout float3 specular, inout float3 diffuse, float3 lightDir, float3 halfVec)
{
	float fresnel = pow(1.0 - saturate(dot(lightDir, halfVec)), 5.0);
	specular = lerp(specular, 1, fresnel);
	diffuse = lerp(diffuse, 0, fresnel);
}

float3 ApplyAmbientLight(
	float3    diffuse,    // Diffuse albedo
	float    ao,            // Pre-computed ambient-occlusion
	float3    lightColor    // Radiance of ambient light
)
{
	return ao * diffuse * lightColor;
}

float GetShadow(float3 ShadowCoord)
{
#ifdef SINGLE_SAMPLE
	float result = texShadow.SampleCmpLevelZero(shadowSampler, ShadowCoord.xy, ShadowCoord.z);
#else
	const float Dilation = 2.0;
	float d1 = Dilation * ShadowTexelSize.x * 0.125;
	float d2 = Dilation * ShadowTexelSize.x * 0.875;
	float d3 = Dilation * ShadowTexelSize.x * 0.625;
	float d4 = Dilation * ShadowTexelSize.x * 0.375;
	float result = (
		2.0 * texShadow.SampleCmpLevelZero(shadowSampler, ShadowCoord.xy, ShadowCoord.z) +
		texShadow.SampleCmpLevelZero(shadowSampler, ShadowCoord.xy + float2(-d2, d1), ShadowCoord.z) +
		texShadow.SampleCmpLevelZero(shadowSampler, ShadowCoord.xy + float2(-d1, -d2), ShadowCoord.z) +
		texShadow.SampleCmpLevelZero(shadowSampler, ShadowCoord.xy + float2(d2, -d1), ShadowCoord.z) +
		texShadow.SampleCmpLevelZero(shadowSampler, ShadowCoord.xy + float2(d1, d2), ShadowCoord.z) +
		texShadow.SampleCmpLevelZero(shadowSampler, ShadowCoord.xy + float2(-d4, d3), ShadowCoord.z) +
		texShadow.SampleCmpLevelZero(shadowSampler, ShadowCoord.xy + float2(-d3, -d4), ShadowCoord.z) +
		texShadow.SampleCmpLevelZero(shadowSampler, ShadowCoord.xy + float2(d4, -d3), ShadowCoord.z) +
		texShadow.SampleCmpLevelZero(shadowSampler, ShadowCoord.xy + float2(d3, d4), ShadowCoord.z)
		) / 10.0;
#endif
	return result * result;
}

float GetShadowConeLight(uint lightIndex, float3 shadowCoord)
{
	float result = lightShadowArrayTex.SampleCmpLevelZero(
		shadowSampler, float3(shadowCoord.xy, lightIndex), shadowCoord.z);
	return result * result;
}

float3 ApplyLightCommon(
	float3    diffuseColor,    // Diffuse albedo
	float3    specularColor,    // Specular albedo
	float    specularMask,    // Where is it shiny or dingy?
	float    gloss,            // Specular power
	float3    normal,            // World-space normal
	float3    viewDir,        // World-space vector from eye to point
	float3    lightDir,        // World-space vector from point to light
	float3    lightColor        // Radiance of directional light
)
{
	float3 halfVec = normalize(lightDir - viewDir);
	float nDotH = saturate(dot(halfVec, normal));

	FSchlick(diffuseColor, specularColor, lightDir, halfVec);

	float specularFactor = specularMask * pow(nDotH, gloss) * (gloss + 2) / 8;

	float nDotL = saturate(dot(normal, lightDir));

	return nDotL * lightColor * (diffuseColor + specularFactor * specularColor);
}

float3 ApplyDirectionalLight(
	float3    diffuseColor,    // Diffuse albedo
	float3    specularColor,    // Specular albedo
	float    specularMask,    // Where is it shiny or dingy?
	float    gloss,            // Specular power
	float3    normal,            // World-space normal
	float3    viewDir,        // World-space vector from eye to point
	float3    lightDir,        // World-space vector from point to light
	float3    lightColor,        // Radiance of directional light
	float3    shadowCoord        // Shadow coordinate (Shadow map UV & light-relative Z)
)
{
	float shadow = GetShadow(shadowCoord);

	return shadow * ApplyLightCommon(
		diffuseColor,
		specularColor,
		specularMask,
		gloss,
		normal,
		viewDir,
		lightDir,
		lightColor
	);
}

float3 ApplyPointLight(
	float3    diffuseColor,    // Diffuse albedo
	float3    specularColor,    // Specular albedo
	float    specularMask,    // Where is it shiny or dingy?
	float    gloss,            // Specular power
	float3    normal,            // World-space normal
	float3    viewDir,        // World-space vector from eye to point
	float3    worldPos,        // World-space fragment position
	float3    lightPos,        // World-space light position
	float    lightRadiusSq,
	float3    lightColor        // Radiance of directional light
)
{
	float3 lightDir = lightPos - worldPos;
	float lightDistSq = dot(lightDir, lightDir);
	float invLightDist = rsqrt(lightDistSq);
	lightDir *= invLightDist;

	// modify 1/d^2 * R^2 to fall off at a fixed radius
	// (R/d)^2 - d/R = [(1/d^2) - (1/R^2)*(d/R)] * R^2
	float distanceFalloff = lightRadiusSq * (invLightDist * invLightDist);
	distanceFalloff = max(0, distanceFalloff - rsqrt(distanceFalloff));

	return distanceFalloff * ApplyLightCommon(
		diffuseColor,
		specularColor,
		specularMask,
		gloss,
		normal,
		viewDir,
		lightDir,
		lightColor
	);
}

float3 ApplyConeLight(
	float3    diffuseColor,    // Diffuse albedo
	float3    specularColor,    // Specular albedo
	float    specularMask,    // Where is it shiny or dingy?
	float    gloss,            // Specular power
	float3    normal,            // World-space normal
	float3    viewDir,        // World-space vector from eye to point
	float3    worldPos,        // World-space fragment position
	float3    lightPos,        // World-space light position
	float    lightRadiusSq,
	float3    lightColor,        // Radiance of directional light
	float3    coneDir,
	float2    coneAngles
)
{
	float3 lightDir = lightPos - worldPos;
	float lightDistSq = dot(lightDir, lightDir);
	float invLightDist = rsqrt(lightDistSq);
	lightDir *= invLightDist;

	// modify 1/d^2 * R^2 to fall off at a fixed radius
	// (R/d)^2 - d/R = [(1/d^2) - (1/R^2)*(d/R)] * R^2
	float distanceFalloff = lightRadiusSq * (invLightDist * invLightDist);
	distanceFalloff = max(0, distanceFalloff - rsqrt(distanceFalloff));

	float coneFalloff = dot(-lightDir, coneDir);
	coneFalloff = saturate((coneFalloff - coneAngles.y) * coneAngles.x);

	return (coneFalloff * distanceFalloff) * ApplyLightCommon(
		diffuseColor,
		specularColor,
		specularMask,
		gloss,
		normal,
		viewDir,
		lightDir,
		lightColor
	);
}

float3 ApplyConeShadowedLight(
	float3    diffuseColor,    // Diffuse albedo
	float3    specularColor,    // Specular albedo
	float    specularMask,    // Where is it shiny or dingy?
	float    gloss,            // Specular power
	float3    normal,            // World-space normal
	float3    viewDir,        // World-space vector from eye to point
	float3    worldPos,        // World-space fragment position
	float3    lightPos,        // World-space light position
	float    lightRadiusSq,
	float3    lightColor,        // Radiance of directional light
	float3    coneDir,
	float2    coneAngles,
	float4x4 shadowTextureMatrix,
	uint    lightIndex
)
{
	float4 shadowCoord = mul(shadowTextureMatrix, float4(worldPos, 1.0));
	shadowCoord.xyz *= rcp(shadowCoord.w);
	float shadow = GetShadowConeLight(lightIndex, shadowCoord.xyz);

	return shadow * ApplyConeLight(
		diffuseColor,
		specularColor,
		specularMask,
		gloss,
		normal,
		viewDir,
		worldPos,
		lightPos,
		lightRadiusSq,
		lightColor,
		coneDir,
		coneAngles
	);
}

// options for F+ variants and optimizations
#ifdef _WAVE_OP // SM 6.0 (new shader compiler)

// choose one of these:
//# define BIT_MASK
//# define BIT_MASK_SORTED
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
uint PullNextBit(inout uint bits)
{
	uint bitIndex = firstbitlow(bits);
	bits ^= 1 << bitIndex;
	return bitIndex;
}

[RootSignature(ModelViewer_RootSig)]
float3 main(VSOutput vsOutput) : SV_Target0
{
	uint2 pixelPos = vsOutput.position.xy;
	float3 diffuseAlbedo = texDiffuse.Sample(sampler0, vsOutput.uv);
	float3 colorSum = 0;
	{
		float ao = texSSAO[pixelPos];
		colorSum += ApplyAmbientLight(diffuseAlbedo, ao, AmbientColor);
	}

	float gloss = 128.0;
	float3 normal;
	{
		normal = texNormal.Sample(sampler0, vsOutput.uv) * 2.0 - 1.0;
		AntiAliasSpecular(normal, gloss);
		float3x3 tbn = float3x3(normalize(vsOutput.tangent), normalize(vsOutput.bitangent), normalize(vsOutput.normal));
		normal = normalize(mul(normal, tbn));
	}

	float3 specularAlbedo = float3(0.56, 0.56, 0.56);
	float specularMask = texSpecular.Sample(sampler0, vsOutput.uv).g;
	float3 viewDir = normalize(vsOutput.viewDir);
	colorSum += ApplyDirectionalLight(diffuseAlbedo, specularAlbedo, specularMask, gloss, normal, viewDir, SunDirection, SunColor, vsOutput.shadowCoord);



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


	for (uint lightIndex = 0; lightIndex < MAX_LIGHTS; lightIndex += 1)
	{
		LightData lightData = lightBuffer[lightIndex];
		if (0==lightData.type)
		{
			colorSum += ApplyPointLight(POINT_LIGHT_ARGS);
		}
		else if (1 == lightData.type)
		{
			colorSum += ApplyConeLight(CONE_LIGHT_ARGS);
		}
		else if (2 == lightData.type)
		{
			colorSum += ApplyConeShadowedLight(SHADOWED_LIGHT_ARGS);
		}
	}

	return colorSum;
}
