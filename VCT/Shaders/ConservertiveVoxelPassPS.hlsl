
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

    int u_clipmapLevel;
    int u_clipmapResolution;
    int u_clipmapResolutionWithBorder;

    float3 u_regionMin;
    float3 u_regionMax;
    float3 u_prevRegionMin;
    float3 u_prevRegionMax;
    float u_downsampleTransitionRegionSize;
    float u_maxExtent;
    float u_voxelSize;
}

float3 transformPosWToClipUVW(float3 posW, float3 extent)
{
    return frac(posW / extent);
}


int3 computeImageCoords(float3 posW)
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
    int3 imageCoords = computeImageCoords(vsOutput.posW);

    for (int i = 0; i < 6; ++i)
    {
        // Currently not supporting alpha blending so just make it fully opaque
        const float4 fillColor = (0.0).xxxx;
        voxel_texture[imageCoords] = fillColor;
        imageCoords.x += u_clipmapResolutionWithBorder;
    }
}