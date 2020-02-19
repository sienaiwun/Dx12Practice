

float3 main(float4 position : SV_Position,
            float2 tc: TexCoord0,
            float3 color: TexCoord1) : SV_Target0
{
    return color;
}