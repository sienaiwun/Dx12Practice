#define VOXEL_TEXTURE_WITH_BORDER
struct GSOutput
{
	float4 pos : SV_POSITION;
    float2 uv : TexCoord0;
    float4 color:TexCoord1;
};



cbuffer regionInfo: register(b0)
{
    int3 u_imageMin  : packoffset(c0);

    int3 u_regionMin: packoffset(c1);
    // Expecting u_prevRegionMin and u_prevVolumeMax in the voxel coordinates of the current region
    int3 u_prevRegionMin:  packoffset(c2);
    int3 u_prevRegionMax:  packoffset(c3);

    int u_hasPrevClipmapLevel : packoffset(c4.x);
    int u_clipmapResolution : packoffset(c4.y);
    int u_clipmapLevel : packoffset(c4.z);
    float u_voxelSize : packoffset(c4.w);
    int u_hasMultipleFaces : packoffset(c5.x);
    int u_numColorComponents : packoffset(c5.y);
    float4x4 u_viewProj: packoffset(c6);
};

Texture3D<float4> u_3dTexture : register(t0);


float3 toWorld(int3 p)
{
    return p * u_voxelSize;
}

static const int BORDER_WIDTH = 1;
static const float EPSILON = 0.00001;

void createQuad(float4 v0, float4 v1, float4 v2, float4 v3, float4 uni_color, inout TriangleStream< GSOutput > output)
{
    GSOutput element;
    element.pos = v0;
    element.color = uni_color;
    element.uv = float2(0.0f, 0.0f);
    output.Append(element);
    element.pos = v1;
    element.color = uni_color;
    element.uv = float2(0.0f, 1.0f);
    output.Append(element);
    element.pos = v3;
    element.color = uni_color;
    element.uv = float2(1.0f, 0.0f);
    output.Append(element);
    element.pos = v2;
    element.uv = float2(1.0f, 1.0f);
    element.color = uni_color;
    output.Append(element);
    output.RestartStrip();
}



[maxvertexcount(24)]
void main(
    point float4 input_pos[1] : SV_POSITION,
	inout TriangleStream< GSOutput > output
)
{
    float3 point_pos = input_pos[0].xyz;
    int3 pos = int3(point_pos.xyz);
    int3 posV = pos + u_regionMin;

    int3 samplePos = (u_imageMin + pos) & (u_clipmapResolution - 1);

#ifdef VOXEL_TEXTURE_WITH_BORDER
    int resolution = u_clipmapResolution + BORDER_WIDTH * 2;
    samplePos += int3(BORDER_WIDTH, BORDER_WIDTH, BORDER_WIDTH);
#else
    int resolution = u_clipmapResolution;
#endif

    samplePos.y += u_clipmapLevel * resolution;

    float4 color = u_3dTexture.Load(int4(samplePos,0));

    float4 colors[6] = { color, color, color, color, color, color };

    float4 v0 = mul(u_viewProj , float4(toWorld(posV), 1.0));
    float4 v1 = mul(u_viewProj , float4(toWorld(posV + int3(1, 0, 0)), 1.0));
    float4 v2 = mul(u_viewProj , float4(toWorld(posV + int3(0, 1, 0)), 1.0));
    float4 v3 = mul(u_viewProj , float4(toWorld(posV + int3(1, 1, 0)), 1.0));
    float4 v4 = mul(u_viewProj , float4(toWorld(posV + int3(0, 0, 1)), 1.0));
    float4 v5 = mul(u_viewProj , float4(toWorld(posV + int3(1, 0, 1)), 1.0));
    float4 v6 = mul(u_viewProj , float4(toWorld(posV + int3(0, 1, 1)), 1.0));
    float4 v7 = mul(u_viewProj , float4(toWorld(posV + int3(1, 1, 1)), 1.0));


    if (colors[0].a >= EPSILON)
    {
        createQuad(v0, v2, v6, v4, colors[0], output);
    }

    if (colors[1].a >= EPSILON)
    {
        createQuad(v1, v5, v7, v3, colors[1], output);
    }

    if (colors[2].a >= EPSILON)
    {
        createQuad(v0, v4, v5, v1, colors[2], output);
    }

    // Y Axis top face of the cube
    if (colors[3].a >= EPSILON)
    {
        createQuad(v2, v3, v7, v6, colors[3], output);
    }

    // Z Axis front face of the cube
    if (colors[4].a >= EPSILON)
    {
        createQuad(v0, v1, v3, v2, colors[4], output);
    }

    // Z Axis back face of the cube
    if (colors[5].a >= EPSILON)
    {
        createQuad(v4, v6, v7, v5, colors[5], output);
    }


}