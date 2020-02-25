
#define THREAD_SIZE_X 1024
#define THREAD_SIZE_Y 1
#define THREAD_SIZE_Z 1

cbuffer PageCountInfo : register(b0)
{
    uint MaxPageCount;
    uint PageOffset;
    uint Pad1;
    uint Pad2;
};


RWStructuredBuffer<uint> PrevVisibBuffer : register(u0);

RWStructuredBuffer<uint> VisibilityBuffer : register(u1);

RWStructuredBuffer<uint> AlivePageTableBuffer : register(u2);

RWStructuredBuffer<uint> AlivePageCountBuffer : register(u3);

RWStructuredBuffer<uint> RemovePageTableBuffer : register(u4);

RWStructuredBuffer<uint> RemovePageCountBuffer : register(u5);


#define FillPage_RootSig \
    "RootFlags(0), " \
    "RootConstants(b0, num32BitConstants=4), " \
    "DescriptorTable(UAV(u0, numDescriptors = 6))," 
[RootSignature(FillPage_RootSig)]
[numthreads(THREAD_SIZE_X, 1, 1)]
void main(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
    // Fill mips of Page table
    uint Index = DTid.x;

    if (Index >= MaxPageCount)
        return;

    Index += PageOffset;

    if (VisibilityBuffer[Index] == 1)
    {
        uint currentTexureID;
        InterlockedAdd(AlivePageCountBuffer[0], 1, currentTexureID);

        // save the index of alive page
        AlivePageTableBuffer[currentTexureID] = Index;
    }
    else if (PrevVisibBuffer[Index] == 1)
    {
        uint currentTexureID;
        InterlockedAdd(RemovePageCountBuffer[0], 1, currentTexureID);

        // remove the index of alive page
        RemovePageTableBuffer[currentTexureID] = Index;
    }

    PrevVisibBuffer[Index] = VisibilityBuffer[Index];

    // clear page
    VisibilityBuffer[Index] = 0;
}
