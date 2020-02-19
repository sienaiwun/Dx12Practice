#include "../../Core/Shaders/Buffers.hlsli"
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
