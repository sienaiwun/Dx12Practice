
struct VSOutput
{
    float4 position : SV_Position;
    float2 texCoord : TexCoord0;
    float3 normal : Normal;
};
float3 main(VSOutput vsOutput) : SV_Target0
{
    return float3(1.0f,1.0f,1.0f);
}