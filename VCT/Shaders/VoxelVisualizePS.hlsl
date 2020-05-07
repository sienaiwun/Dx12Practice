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
    float3 normal:TexCoord2;
};


static const float EPSILON = 0.00001;


struct OMInputDeferred {
    float4 base_color		: SV_Target0;
    float4 n				: SV_Target1;
    float2 material			: SV_Target2;
};

float2 EncodeUnitVector_CryEngine(float3 u)
{
    //https://aras-p.info/texts/CompactNormalStorage.html
    return normalize(u.xy) * sqrt(u.z * 0.5f + 0.5f);
}

OMInputDeferred main(GSOutput In) 
{
    OMInputDeferred output;
    float4 out_color = In.color;
    if (u_borderWidth > EPSILON)
        out_color = lerp(u_borderColor, In.color, min(min(In.uv.x, min(In.uv.y, min((1.0 - In.uv.x), (1.0 - In.uv.y)))) / u_borderWidth, 1.0));
    out_color.a = u_alpha;
    output.base_color = out_color;
    output.n.xy = EncodeUnitVector_CryEngine(In.normal);
    output.material.x = 0.0f;
    output.material.y = 0.0f;
	return output;
}