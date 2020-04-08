#include "ModelViewerRS.hlsli"
#include "../../Core/Shaders/Buffers.hlsli"

// keep in sync with C code
#define MAX_LIGHTS 256
#define TILE_SIZE (4 + MAX_LIGHTS * 4)

struct LightData
{
	float3 pos;
	float radiusSq;

	float3 color;
	uint type;

	float3 coneDir;
	float2 coneAngles; // x = 1.0f / (cos(coneInner) - cos(coneOuter)), y = cos(coneOuter)

	float4x4 shadowTextureMatrix;
};

uint2 GetTilePos(float2 pos, float2 invTileDim)
{
	return pos * invTileDim;
}
uint GetTileIndex(uint2 tilePos, uint tileCountX)
{
	return tilePos.y * tileCountX + tilePos.x;
}
uint GetTileOffset(uint tileIndex)
{
	return tileIndex * TILE_SIZE;
}

#include "Lighting.hlsli"
Texture2D<float4> ColorTex : register(t32);
Texture2D<float4> NormalTex : register(t33);
Texture2D<float2> MaterialTex:register(t34);
Texture2D<float> DepthTex:register(t35);



float3 DecodeNormal_CryEngine(float2 G)
{
	//¡°A bit more Deferred¡± - CryEngine 3
	float z = dot(G.xy, G.xy) * 2.0f - 1.0f;
	float2 xy = normalize(G.xy)*sqrt(1 - z * z);
	return float3(xy, z);
}

float3 HomogeneousDivide(float4 p_proj) {
	const float inv_w = 1.0f / p_proj.w;
	return p_proj.xyz * inv_w;
}


float2 SSViewportToUV(float2 p_ss_viewport) {
	// SS Viewport -> UV
	// .x: [0,g_ss_viewport_resolution.x] -> [-1,1]
	// .y: [0,g_ss_viewport_resolution.y] -> [-1,1]
	float2 temp =  p_ss_viewport * g_inv_viewport.xy * 2 - 1;
	temp.y = -temp.y;
	return temp;
}


float3 SSDisplayToCamera(float2 p_ss_display, float depth)
{
	const float4 p_proj = { SSViewportToUV(p_ss_display), depth, 1.0f };
	const float4 p_camera = mul(g_projection_to_camera, p_proj);
	return HomogeneousDivide(p_camera);
}



[RootSignature(ModelViewer_RootSig)]
float3 main(float4 position : SV_Position) : SV_Target0
{
	uint2 pixelPos = position.xy;
	float3 colorSum = 0;
	float3 diffuseAlbedo = ColorTex[pixelPos].xyz;
	float depth = DepthTex[pixelPos];
	{
		 float ao = texSSAO[pixelPos];
		 colorSum += ApplyAmbientLight(diffuseAlbedo, ao, AmbientColor);
	}
	float gloss = 128.0;
	float3 normal = DecodeNormal_CryEngine(NormalTex[pixelPos].xy);
	float3 specularAlbedo = float3(0.56, 0.56, 0.56);
	float specularMask = MaterialTex[pixelPos].x;
	
	float3 viewpos = SSDisplayToCamera(pixelPos, depth);
	float3 worldpos = HomogeneousDivide(mul(g_camera_to_world, float4(viewpos, 1.0f)));
	float3 viewDir = normalize(worldpos - g_viewer_pos);
	float3 shadowCoord = mul(g_model_to_shadow, float4(worldpos, 1.0)).xyz;
	colorSum += ApplyDirectionalLight(diffuseAlbedo, specularAlbedo, specularMask, gloss, normal, viewDir, SunDirection, SunColor, shadowCoord);


	uint2 tilePos = GetTilePos(pixelPos, InvTileDim.xy);
	uint tileIndex = GetTileIndex(tilePos, TileCount.x);
	uint tileOffset = GetTileOffset(tileIndex);


#define POINT_LIGHT_ARGS \
    diffuseAlbedo, \
    specularAlbedo, \
    specularMask, \
    gloss, \
    normal, \
    viewDir, \
    worldpos, \
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
	return  colorSum;
}