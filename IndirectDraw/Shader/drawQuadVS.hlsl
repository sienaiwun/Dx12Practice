
cbuffer ElementData: register(b1)
{
    uint2 index;       // : packoffset(c0);
    uint  index1d;     // : packoffset(c0.z);
    uint  _;
    float3 c_color;      // : packoffset(c1);
    float __;
    float4 tap1;       //  : packoffset(c2);
    float4 tap2;        // : packoffset(c3);
    float4 tap_array[12];
};

cbuffer uniformBlock : register(b0)
{
    float2 screen_res : packoffset(c0);
    float time : packoffset(c0.z);
    float padding : packoffset(c0.w);
    uint2 tile_num :packoffset(c1);
    uint2 tile_res :packoffset(c1.z);
}

float2 DrawQuad(float2 screenQuad, float2 tile_index)
{
    float2 smalltile_res = (screenQuad + float2(1,1)) * tile_res / screen_res;
    return float2(-1, -1) + (tile_index/ tile_num)*float2(2,2)  +  smalltile_res;
}

void main(
    in uint VertID : SV_VertexID,
    in uint InstanceID: SV_InstanceID,
    out float4 Pos : SV_Position,
    out float2 Tex : TexCoord0,
    out float3 color: TexCoord1
)
{
    // Texture coordinates range [0, 2], but only [0, 1] appears on screen.
    Tex = float2(VertID % 2, (VertID % 4) >> 1);
    Pos = float4((Tex.x - 0.5f) * 2, -(Tex.y - 0.5f) * 2, 0, 1);
    int x = index1d % tile_num.x;
    int y = index1d / tile_num.x;
    Pos.xy = DrawQuad(Pos.xy, float2(x, y));
    color = c_color;
}

