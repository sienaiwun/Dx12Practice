cbuffer CB1 : register(b0)
{
    int3 u_min;
    int u_resolution;
    int u_clipmapLevel;
    int3 u_extent;
    int u_borderWidth = 1;
}

RWTexture3D<float4> voxel_texture : register(u0);
[numthreads(8, 8, 8)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    if (DTid.x >= u_extent.x ||
        DTid.y >= u_extent.y ||
        DTid.z >= u_extent.z) return;
    int3 pos = (int3(DTid) + u_min) & (u_resolution - 1);

    int resolution = u_resolution + 2 * u_borderWidth;
    pos += int3(u_borderWidth, u_borderWidth, u_borderWidth);

    // Target correct clipmap level
    pos.y += u_clipmapLevel * resolution;

    const float4 clearColor = (0.0).xxxx;
    voxel_texture[pos] = clearColor;
    voxel_texture[pos + int3(resolution, 0, 0)] = clearColor;
    voxel_texture[pos + int3(2 * resolution, 0, 0)] = clearColor;
    voxel_texture[pos + int3(3 * resolution, 0, 0)] = clearColor;
    voxel_texture[pos + int3(4 * resolution, 0, 0)] = clearColor;
    voxel_texture[pos + int3(5 * resolution, 0, 0)] = clearColor;

}