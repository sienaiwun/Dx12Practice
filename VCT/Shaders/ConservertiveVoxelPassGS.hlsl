struct GSOutput
{
	float4 position : SV_Position;
    float2 texCoord : TexCoord0;
    float3 normal : Normal;
};

struct GSInput
{
    float4 position : SV_Position;
    float2 texCoord : TexCoord0;
    float3 normal : Normal;
};

[maxvertexcount(3)]
void main(
	triangle GSInput input[3],
	inout TriangleStream< GSOutput > output
)
{
	for (uint i = 0; i < 3; i++)
	{
		GSOutput element;
		element = input[i];
		output.Append(element);
	}
}