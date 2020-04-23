
struct VSInput
{
    float3 position : POSITION;
    float2 texcoord0 : TEXCOORD;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
};

struct VSOutput
{
    float4 position : SV_Position;
    float2 texCoord : TexCoord0;
    float3 normal : Normal;
    float3 posW : TexCoord1;
};

VSOutput main(VSInput vsInput)
{
    VSOutput vsOutput;

    vsOutput.position = float4(vsInput.position, 1.0);
    vsOutput.texCoord = vsInput.texcoord0;
    vsOutput.normal = vsInput.normal;
    vsOutput.posW = vsInput.position.xyz;
    return vsOutput;
}
