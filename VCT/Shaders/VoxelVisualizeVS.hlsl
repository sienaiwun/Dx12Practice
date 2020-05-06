float4 main( float4 pos : POSITION ) : SV_POSITION
{
	return float4(pos.xyz,1.0f);
}