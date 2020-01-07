



Texture2D<float4> uTex0 : register(t0);
float3 main(float4 position : SV_Position) : SV_Target0
{
    uint2 pixelPos = position.xy;
    float3 diffuseAlbedo = uTex0[pixelPos].xyz;
    return diffuseAlbedo;
}