
struct VSOutput
{
    float4 position : SV_Position;
    float2 texCoord : TexCoord0;
    float3 normal : Normal;
    float3 posW : TexCoord1;
    uint  vpindex  : SV_ViewportArrayIndex;
};
#define BORDER_WIDTH  1

RWTexture3D<float4> voxel_texture : register(u0);

cbuffer CB1 : register(b1)
{

    int u_clipmapLevel : packoffset(c0.x);
    int u_clipmapResolution : packoffset(c0.y);
    int u_clipmapResolutionWithBorder : packoffset(c0.z);

    float3 u_regionMin: packoffset(c1);
    float3 u_regionMax: packoffset(c2);
    float3 u_prevRegionMin: packoffset(c3);
    float3 u_prevRegionMax: packoffset(c4);
    float u_downsampleTransitionRegionSize : packoffset(c5.x);
    float u_maxExtent : packoffset(c5.y);
    float u_voxelSize : packoffset(c5.z);
}

float3 transformPosWToClipUVW(float3 posW, float3 extent)
{
    return frac(posW / extent);
}


float3 computeImageCoords(float3 posW)
{
    float c = u_voxelSize * .25f;
    posW = clamp(posW, u_regionMin + c, u_regionMax - c);

    float3 clipCoords = transformPosWToClipUVW(posW, u_maxExtent);

    // The & (u_clipmapResolution - 1) (aka % u_clipmapResolution) is important here because
    // clipCoords can be in [0,1] and thus cause problems at the border (value of 1) of the physical
    // clipmap since the computed value would be 1 * u_clipmapResolution and thus out of bounds.
    // The reason is that in transformPosWToClipUVW the frac() operation is used and due to floating point
    // precision limitations the operation can return 1 instead of the mathematically correct fraction.
    int3 imageCoords = int3(clipCoords * u_clipmapResolution) & (u_clipmapResolution - 1);

    imageCoords += int3(BORDER_WIDTH, BORDER_WIDTH, BORDER_WIDTH);

    // Target the correct clipmap level
    imageCoords.y += u_clipmapResolutionWithBorder * u_clipmapLevel;

    return imageCoords;
}
void main(VSOutput vsOutput) 
{

    if (any(vsOutput.posW < u_regionMin) || any(vsOutput.posW > u_regionMax))
        return;
    float3 imageCoords = computeImageCoords(vsOutput.posW);

    for (int i = 0; i < 6; ++i)
    {
        // Currently not supporting alpha blending so just make it fully opaque
        const float4 fillColor = (1.0).xxxx;
        voxel_texture[imageCoords] = fillColor;
        imageCoords.x += u_clipmapResolutionWithBorder;
    }
}