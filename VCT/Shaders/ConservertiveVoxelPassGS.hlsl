struct GSOutput
{
	float4 position : SV_Position;
     uint  vpindex  : SV_ViewportArrayIndex;
    float2 texCoord : TexCoord0;
    float3 normal : Normal;
};

struct GSInput
{
    float4 position : SV_Position;
    float2 texCoord : TexCoord0;
    float3 normal : Normal;
};

cbuffer regionInfo: register(b0)
{
    float4x4 u_viewProj[3];
    float2 u_viewportSizes[3];
};

int getDominantAxisIdx(float3 v0, float3 v1, float3 v2)
{
    float3 aN = abs(cross(v1 - v0, v2 - v0));
    if (aN.x > aN.y && aN.x > aN.z)
        return 0;
    if (aN.y > aN.z)
        return 1;
    return 2;
}



[maxvertexcount(3)]
void main(
	triangle GSInput input[3],
	inout TriangleStream< GSOutput > output
)
{
    int idx = getDominantAxisIdx(input[0].position.xyz, input[1].position.xyz, input[2].position.xyz);
	for (uint i = 0; i < 3; i++)
	{
		GSOutput element;
        element.position = mul(u_viewProj[i], input[i].position);
        element.vpindex = idx;
        element.texCoord = input[i].texCoord;
        element.normal = input[i].normal;
		output.Append(element);
	}
}