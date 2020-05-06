cbuffer CB1 : register(b1)
{
    float4 u_borderColor : packoffset(c0);
    float u_borderWidth : packoffset(c1.x);
    float u_alpha : packoffset(c1.y);
}

struct GSOutput
{
    float4 position : SV_Position;
    float2 uv : TexCoord0;
    float4 color : TexCoord1;
};




static const float EPSILON = 0.00001;

float4 main(GSOutput In) : SV_TARGET
{
    float4 out_color = In.color;
    if (u_borderWidth > EPSILON)
        out_color = lerp(u_borderColor, In.color, min(min(In.uv.x, min(In.uv.y, min((1.0 - In.uv.x), (1.0 - In.uv.y)))) / u_borderWidth, 1.0));
    out_color.a = u_alpha;
	return out_color;
}