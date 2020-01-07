#define QUAD_WIDTH 8
#define QUAD_HEIGHT QUAD_WIDTH


struct IndirectCommand
{
    uint2 cbvAddress;
    uint4 drawArguments;
};
AppendStructuredBuffer<IndirectCommand> outputCommands : register(u0);
StructuredBuffer<IndirectCommand> inputCommands : register(t0);


cbuffer uniformBlock : register(b0)
{
    float2 screen_res : packoffset(c0);
    float time : packoffset(c0.z);
    float padding : packoffset(c0.w);
    uint2 tile_num :packoffset(c1);
    uint2 tile_res :packoffset(c1.z);
}


bool in_heart(float2 uv)
{
    float tt = fmod(time, 1.5) / 1.5;
    float ss = pow(abs(tt), .2)*0.5 + 0.5;
    ss = 1.0 + ss * 0.5*sin(tt*6.2831*3.0 + uv.y*0.5)*exp(-tt * 4.0);
    uv *= float2(0.5, 1.5) + ss * float2(0.5, -0.5);

    uv *= 0.8;
    uv.y = -0.1 - uv.y*1.2 + abs(uv.x)*(1.0 - abs(uv.x));
    float r = length(uv);
    float d = 0.5;
    return d-r> 0;
}

[numthreads(QUAD_WIDTH, QUAD_HEIGHT, 1)]
void main(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
    int groupIdx = Gid.y * tile_num.x + Gid.x;
    float2 uv = (2.0 *DTid.xy - screen_res)/ min(screen_res.x, screen_res.y);
    if(GTid.x == 0 && GTid.y == 0 && in_heart(uv))
        outputCommands.Append(inputCommands[groupIdx]);
}