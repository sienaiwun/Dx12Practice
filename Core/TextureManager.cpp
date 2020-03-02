//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author(s):  James Stanard 
//             Alex Nankervis
//

#include "pch.h"
#include "TextureManager.h"
#include "FileUtility.h"
#include "DDSTextureLoader.h"
#include "GraphicsCore.h"
#include "CommandContext.h"
#include "CompiledShaders/FillPage.h"
#include <map>
#include <thread>

using namespace std;
using namespace Graphics;
static bool once = true;

static UINT BytesPerPixel( DXGI_FORMAT Format )
{
    return (UINT)BitsPerPixel(Format) / 8;
};

void Texture::Create( size_t Pitch, size_t Width, size_t Height, DXGI_FORMAT Format, const void* InitialData )
{
    m_UsageState = D3D12_RESOURCE_STATE_COPY_DEST;

    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = Width;
    texDesc.Height = (UINT)Height;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = Format;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES HeapProps;
    HeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    HeapProps.CreationNodeMask = 1;
    HeapProps.VisibleNodeMask = 1;

    ASSERT_SUCCEEDED(g_Device->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
        m_UsageState, nullptr, MY_IID_PPV_ARGS(m_pResource.ReleaseAndGetAddressOf())));

    m_pResource->SetName(L"Texture");

    D3D12_SUBRESOURCE_DATA texResource;
    texResource.pData = InitialData;
    texResource.RowPitch = Pitch * BytesPerPixel(Format);
    texResource.SlicePitch = texResource.RowPitch * Height;

    CommandContext::InitializeTexture(*this, 1, &texResource);

    if (m_hCpuDescriptorHandle.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
        m_hCpuDescriptorHandle = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    g_Device->CreateShaderResourceView(m_pResource.Get(), nullptr, m_hCpuDescriptorHandle);
}

void Texture::CreateTGAFromMemory( const void* _filePtr, size_t, bool sRGB )
{
    const uint8_t* filePtr = (const uint8_t*)_filePtr;

    // Skip first two bytes
    filePtr += 2;

    /*uint8_t imageTypeCode =*/ *filePtr++;

    // Ignore another 9 bytes
    filePtr += 9;

    uint16_t imageWidth = *(uint16_t*)filePtr;
    filePtr += sizeof(uint16_t);
    uint16_t imageHeight = *(uint16_t*)filePtr;
    filePtr += sizeof(uint16_t);
    uint8_t bitCount = *filePtr++;

    // Ignore another byte
    filePtr++;

    uint32_t* formattedData = new uint32_t[imageWidth * imageHeight];
    uint32_t* iter = formattedData;

    uint8_t numChannels = bitCount / 8;
    uint32_t numBytes = imageWidth * imageHeight * numChannels;

    switch (numChannels)
    {
    default:
        break;
    case 3:
        for (uint32_t byteIdx = 0; byteIdx < numBytes; byteIdx += 3)
        {
            *iter++ = 0xff000000 | filePtr[0] << 16 | filePtr[1] << 8 | filePtr[2];
            filePtr += 3;
        }
        break;
    case 4:
        for (uint32_t byteIdx = 0; byteIdx < numBytes; byteIdx += 4)
        {
            *iter++ = filePtr[3] << 24 | filePtr[0] << 16 | filePtr[1] << 8 | filePtr[2];
            filePtr += 4;
        }
        break;
    }

    Create( imageWidth, imageHeight, sRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM, formattedData );

    delete [] formattedData;
}

bool Texture::CreateDDSFromMemory( const void* filePtr, size_t fileSize, bool sRGB )
{
    if (m_hCpuDescriptorHandle.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
        m_hCpuDescriptorHandle = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    HRESULT hr = CreateDDSTextureFromMemory( Graphics::g_Device,
        (const uint8_t*)filePtr, fileSize, 0, sRGB, &m_pResource, m_hCpuDescriptorHandle );

    return SUCCEEDED(hr);
}

void Texture::CreatePIXImageFromMemory( const void* memBuffer, size_t fileSize )
{
    struct Header
    {
        DXGI_FORMAT Format;
        uint32_t Pitch;
        uint32_t Width;
        uint32_t Height;
    };
    const Header& header = *(Header*)memBuffer;

    ASSERT(fileSize >= header.Pitch * BytesPerPixel(header.Format) * header.Height + sizeof(Header),
        "Raw PIX image dump has an invalid file size");

    Create(header.Pitch, header.Width, header.Height, header.Format, (uint8_t*)memBuffer + sizeof(Header));
}

PageInfo::PageInfo(const PageInfo & page) :
start_corordinate(page.start_corordinate), 
regionSize(page.regionSize), 
mipLevel(page.mipLevel), 
m_mem(page.m_mem?std::make_unique<DynAlloc>(*page.m_mem):nullptr),
is_packed(page.is_packed)
{}

void PageInfo::LoadData(std::vector<UINT8> data)
{
    CommandContext& InitContext = CommandContext::Begin();
    const size_t NumBytes = data.size() * sizeof(UINT8);
    m_mem = std::make_unique<DynAlloc>(InitContext.ReserveUploadMemory(NumBytes));
    m_mem->Buffer->SetName(L"Page upload buffer");
    SIMDMemCopy(m_mem->DataPtr, data.data(), Math::DivideByMultiple(NumBytes, 16));
    InitContext.Finish(true);
}

void TiledTexture::Create(size_t Width, size_t Height, DXGI_FORMAT Format)
{
    Destroy();

    m_resTexWidth = Width;
    m_resTexHeight = Height;
    m_activeMipChanged = true;
    m_resTexPixelInBytes = 4;
    m_activeMip = 0;
    UINT mipLevels = 0;
    for (UINT w = m_resTexWidth, h = m_resTexHeight; w > 0 && h > 0; w >>= 1, h >>= 1)
    {
        mipLevels++;
    }
    m_activeMip = static_cast<U8>(mipLevels - 1);    // Show the least detailed mip first.
    m_mips.resize(mipLevels);
    D3D12_RESOURCE_DESC reservedTextureDesc{};
    reservedTextureDesc.MipLevels = static_cast<UINT16>(m_mips.size());
    reservedTextureDesc.Format = Format;
    reservedTextureDesc.Width = m_resTexWidth;
    reservedTextureDesc.Height = m_resTexHeight;
    reservedTextureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    reservedTextureDesc.DepthOrArraySize = 1;
    reservedTextureDesc.SampleDesc.Count = 1;
    reservedTextureDesc.SampleDesc.Quality = 0;
    reservedTextureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    reservedTextureDesc.Layout = D3D12_TEXTURE_LAYOUT_64KB_UNDEFINED_SWIZZLE;
    m_UsageState = D3D12_RESOURCE_STATE_COPY_DEST;
    ASSERT_SUCCEEDED(g_Device->CreateReservedResource(
        &reservedTextureDesc,
        m_UsageState,
        nullptr,
        MY_IID_PPV_ARGS(m_pResource.ReleaseAndGetAddressOf())));
    m_pResource->SetName(L"Tiled Resource");
    m_GpuVirtualAddress = m_pResource->GetGPUVirtualAddress();
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = reservedTextureDesc.Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = reservedTextureDesc.MipLevels;
    m_hCpuDescriptorHandle = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    g_Device->CreateShaderResourceView(m_pResource.Get(), &srvDesc, m_hCpuDescriptorHandle);

    const UINT64 resourceSize = GetRequiredIntermediateSize(m_pResource.Get(), 0, 1);

    ASSERT_SUCCEEDED(g_Device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(resourceSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        MY_IID_PPV_ARGS(&m_uploadBuffer)));
    UINT numTiles = 0;
    
    UINT subresourceCount = reservedTextureDesc.MipLevels;
    std::vector<D3D12_SUBRESOURCE_TILING> tilings(subresourceCount);
    m_TileShape = {};
    g_Device->GetResourceTiling(m_pResource.Get(), &numTiles, &m_packedMipInfo, &m_TileShape, &subresourceCount, 0, &tilings[0]);
    UINT heapRangeCount = m_packedMipInfo.NumStandardMips + (m_packedMipInfo.NumPackedMips > 0 ? 1 : 0);
    for (UINT n = 0; n < m_mips.size(); n++)
    {
        if (n < m_packedMipInfo.NumStandardMips)
        {
            m_mips[n].heapRangeIndex = n;
            m_mips[n].packedMip = false;
            m_mips[n].mapped = false;
            m_mips[n].startCoordinate = CD3DX12_TILED_RESOURCE_COORDINATE(0, 0, 0, n);
            m_mips[n].regionSize.Width = tilings[n].WidthInTiles;
            m_mips[n].regionSize.Height = tilings[n].HeightInTiles;
            m_mips[n].regionSize.Depth = tilings[n].DepthInTiles;
            m_mips[n].regionSize.NumTiles = tilings[n].WidthInTiles * tilings[n].HeightInTiles * tilings[n].DepthInTiles;
            m_mips[n].regionSize.UseBox = TRUE;
        }
        else
        {
            // All of the packed mips will go into the last heap.
            m_mips[n].heapRangeIndex = heapRangeCount - 1;
            m_mips[n].packedMip = true;
            m_mips[n].mapped = false;

            // Mark all of the packed mips as having the same start coordinate and size.
            m_mips[n].startCoordinate = CD3DX12_TILED_RESOURCE_COORDINATE(0, 0, 0, heapRangeCount - 1);
            m_mips[n].regionSize.NumTiles = m_packedMipInfo.NumTilesForPackedMips;
            m_mips[n].regionSize.UseBox = FALSE;    // regionSize.Width/Height/Depth will be ignored.
        }
    }

    U32x3 imageGranularity;
    imageGranularity[0] = m_TileShape.WidthInTexels;
    imageGranularity[1] = m_TileShape.HeightInTexels;
    imageGranularity[2] = m_TileShape.DepthInTexels;

    m_pages.resize(0);
    size_t heapOffset = 0;
    for (uint32_t mipLevel = 0; mipLevel < m_packedMipInfo.NumStandardMips; mipLevel++)
    {
        U32x3 extent;
        extent[0] = std::max<UINT>(m_resTexWidth>> mipLevel, 1u);
        extent[1] = std::max<UINT>(m_resTexHeight >> mipLevel, 1u);
        extent[2] = std::max<UINT>(1 >> mipLevel, 1u);

        U32x3 sparseBindCounts = DivideByMultiple(extent, imageGranularity);
        U32x3 lastBlockExtend;
        lastBlockExtend[0] = extent[0] % imageGranularity[0] ? extent[0] % imageGranularity[0] : imageGranularity[0];
        lastBlockExtend[1] = extent[1] % imageGranularity[1] ? extent[1] % imageGranularity[1] : imageGranularity[1];
        lastBlockExtend[2] = extent[2] % imageGranularity[2] ? extent[2] % imageGranularity[2] : imageGranularity[2];
        for (U32 y = 0; y < sparseBindCounts[1]; y++)
        {
            for (U32 x = 0; x < sparseBindCounts[0]; x++)
            {
                D3D12_TILED_RESOURCE_COORDINATE start_coordinate;
                start_coordinate.X = x ;
                start_coordinate.Y = y ;
                start_coordinate.Z = 0;
                start_coordinate.Subresource = mipLevel;
                // Size of the page
                D3D12_TILED_RESOURCE_COORDINATE extent;
                extent.X = (x == sparseBindCounts[0] - 1) ? lastBlockExtend[0] : imageGranularity[0];
                extent.Y = (y == sparseBindCounts[1] - 1) ? lastBlockExtend[1] : imageGranularity[1];
                extent.Z =  lastBlockExtend[2];
                extent.Subresource = mipLevel;
                PageInfo page { };
                page.regionSize.Width = 1;
                page.regionSize.Height = 1;
                page.regionSize.Depth = 1;
                page.regionSize.NumTiles = 1;
                page.regionSize.UseBox = TRUE;
                page.mipLevel = mipLevel;
                page.start_corordinate = start_coordinate;
                page.is_packed = false;
                heapOffset += D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
                page.heapOffset = heapOffset;
                m_pages.emplace_back(std::move(page));
            }
        }
    }
    PageInfo page{ };
    page.regionSize.NumTiles = m_packedMipInfo.NumTilesForPackedMips;
    page.regionSize.UseBox = false;
    page.mipLevel = m_packedMipInfo.NumStandardMips;
    page.regionSize.Width = 0;
    page.regionSize.Height = 0;
    page.regionSize.Depth = 0;
    page.is_packed = true;
    page.start_corordinate = CD3DX12_TILED_RESOURCE_COORDINATE(0, 0, 0, m_packedMipInfo.NumStandardMips);;
    heapOffset += D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
    page.heapOffset = heapOffset;
    m_pages.emplace_back(std::move(page));
    m_visibilityBuffer.Create(L"visibilityBuffer", m_pages.size(), sizeof(int), nullptr);
    m_prevVisBuffer.Create(L"preVisBuffer", m_pages.size(), sizeof(int), nullptr);
    m_alivePagesBuffer.Create(L"aliveBuffer", m_pages.size(), sizeof(int), nullptr);
    m_removedPagesBuffer.Create(L"removedPageBuffer", m_pages.size(), sizeof(int), nullptr);
    m_alivePagesReadBackBuffer.Create(L"alivePagesReadBackBuffer", m_pages.size(), sizeof(int));
    m_removedPagesReadBackBuffer.Create(L"removedPagesBuffer", m_pages.size(), sizeof(int));
    m_alivePagesCounterReadBackBuffer.Create(L"alivePagesCounterReadBackBuffer", 1, sizeof(int));
    m_removedPagesCounterReadBackBuffer.Create(L"removedPagesCounterReadBackBuffer", 1, sizeof(int));
    m_alivePagesCounterBuffer.Create(L"alivePageBufferCounter", 1, sizeof(int));
    m_removedPagesCounterBuffer.Create(L"removedPageBufferCounter", 1, sizeof(int));
    m_heap_offsets.resize(heapRangeCount);
    UINT heapSize = 0;
    for (UINT n = 0; n < heapRangeCount; n++)
    {
        m_heap_offsets[n] = heapSize / D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
        heapSize += m_mips[n].regionSize.NumTiles * D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;

    }
    CD3DX12_HEAP_DESC heapDesc(heapSize, D3D12_HEAP_TYPE_DEFAULT, 0, D3D12_HEAP_FLAG_DENY_BUFFERS | D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES);
    ASSERT_SUCCEEDED(g_Device->CreateHeap(&heapDesc, IID_PPV_ARGS(&m_heap)));

    CD3DX12_HEAP_DESC pageheapDesc(heapOffset, D3D12_HEAP_TYPE_DEFAULT, 0, D3D12_HEAP_FLAG_DENY_BUFFERS | D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES);
    ASSERT_SUCCEEDED(g_Device->CreateHeap(&pageheapDesc, IID_PPV_ARGS(&m_page_heaps)))

    m_rootSig.Reset(TiledComputerParams::NumComputeParams, 0);
    m_rootSig[TiledComputerParams::PageCountInfo].InitAsConstants(0, 4, D3D12_SHADER_VISIBILITY_ALL);
    m_rootSig[TiledComputerParams::Buffers].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 6);
    m_rootSig.Finalize(L"FillPage");
    m_computePSO.SetRootSignature(m_rootSig);
    m_computePSO.SetComputeShader(SHADER_ARGS(g_pFillPage));
    m_computePSO.Finalize();
    m_computePSO.GetPipelineStateObject()->SetName(L"Fill Page PSO");
}

void TiledTexture::UpdateTileMapping(GraphicsContext& gfxContext)
{
    U32 firstSubresource = m_mips[m_activeMip].heapRangeIndex;
    U32 subResourceCount = m_mips[m_activeMip].packedMip ? m_packedMipInfo.NumPackedMips : 1;
    if (!m_mips[firstSubresource].mapped)
    {
        std::vector<UINT8> texture = GenerateTextureData(firstSubresource, subResourceCount);
        // update mapping
        UINT updatedRegions = 0;
        std::vector<D3D12_TILED_RESOURCE_COORDINATE> startCoordinates;
        std::vector<D3D12_TILE_REGION_SIZE> regionSizes;
        std::vector<D3D12_TILE_RANGE_FLAGS> rangeFlags;
        std::vector<UINT> heapRangeStartOffsets;
        std::vector<UINT> rangeTileCounts;
        for (size_t n = 0; n < m_heap_offsets.size(); n++)
        {
            if (!m_mips[n].mapped && n != firstSubresource)
            {
                // Skip unchanged tile regions.
                continue;
            }
            startCoordinates.push_back(m_mips[n].startCoordinate);
            regionSizes.push_back(m_mips[n].regionSize);
            if (n == firstSubresource)
            {
                // Map the currently active mip.
                rangeFlags.push_back(D3D12_TILE_RANGE_FLAG_NONE);
                m_mips[n].mapped = true;
            }
            else
            {
                // Unmap the previously active mip.
                assert(m_mips[n].mapped);

                rangeFlags.push_back(D3D12_TILE_RANGE_FLAG_NULL);
                m_mips[n].mapped = false;
            }
            heapRangeStartOffsets.push_back(m_heap_offsets[n]);        // In this sample, each heap contains only one tile region.
            rangeTileCounts.push_back(m_mips[n].regionSize.NumTiles);

            updatedRegions++;
        }
        Graphics::g_CommandManager.GetCommandQueue()->UpdateTileMappings(
            m_pResource.Get(),
            updatedRegions,
            &startCoordinates[0],
            &regionSizes[0],
            m_heap.Get(),
            updatedRegions,
            &rangeFlags[0],
            &heapRangeStartOffsets[0],
            &rangeTileCounts[0],
            D3D12_TILE_MAPPING_FLAG_NONE
        );
        {
            UINT mipOffset = 0;
            std::vector<D3D12_SUBRESOURCE_DATA> data(subResourceCount);
            for (UINT n = 0; n < subResourceCount; n++)
            {
                UINT currentMip = firstSubresource + n;

                data[n].pData = &texture[mipOffset];
                data[n].RowPitch = (m_resTexWidth >> currentMip) * m_resTexPixelInBytes;
                data[n].SlicePitch = data[n].RowPitch * (m_resTexHeight >> currentMip);

                mipOffset += static_cast<UINT>(data[n].SlicePitch);
            }

            UpdateSubresources(gfxContext.GetCommandList(), m_pResource.Get(), m_uploadBuffer.Get(), 0, firstSubresource, subResourceCount, &data[0]);
        }
    }
    m_activeMipChanged = false;
}
void TiledTexture::UpdateVisibilityBuffer(ComputeContext& computeContext)
{
    computeContext.SetRootSignature(m_rootSig);
    computeContext.SetPipelineState(m_computePSO);
    computeContext.FillBuffer(m_alivePagesCounterBuffer, 0, 0, sizeof(uint32_t));
    computeContext.FillBuffer(m_removedPagesCounterBuffer, 0, 0, sizeof(uint32_t));
    computeContext.TransitionResource(m_visibilityBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    computeContext.TransitionResource(m_prevVisBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    computeContext.TransitionResource(m_alivePagesBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    computeContext.TransitionResource(m_removedPagesBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    computeContext.TransitionResource(m_alivePagesCounterBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    computeContext.TransitionResource(m_removedPagesCounterBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    D3D12_CPU_DESCRIPTOR_HANDLE handles[6] = {m_prevVisBuffer.GetUAV(),m_visibilityBuffer.GetUAV(),m_alivePagesBuffer.GetUAV(),m_alivePagesCounterBuffer.GetUAV(),m_removedPagesBuffer.GetUAV(),m_removedPagesCounterBuffer.GetUAV()};
    computeContext.SetDynamicDescriptors(TiledComputerParams::Buffers, 0, 6 ,handles);
    computeContext.SetConstants(TiledComputerParams::PageCountInfo, (UINT)m_pages.size(), 0, 0, 0);
    computeContext.Dispatch3D(m_pages.size(), 1, 1, 1024, 1, 1);

    computeContext.CopyBuffer(m_alivePagesCounterReadBackBuffer,m_alivePagesCounterBuffer );
    computeContext.CopyBuffer(m_removedPagesCounterReadBackBuffer, m_removedPagesCounterBuffer);
    computeContext.CopyBuffer(m_alivePagesReadBackBuffer, m_alivePagesBuffer);
    computeContext.CopyBuffer(m_removedPagesReadBackBuffer, m_removedPagesBuffer);
}


void TiledTexture::AddPages(GraphicsContext& gContext)
{
    int active_page_count = *static_cast<int*>(m_alivePagesCounterReadBackBuffer.Map());
    m_alivePagesCounterReadBackBuffer.Unmap();
    if (active_page_count == 0)
        return;
    std::vector<U32> active_pages;
    active_pages.resize(active_page_count);
    memcpy(active_pages.data(), m_alivePagesReadBackBuffer.Map(), active_page_count * sizeof(int));
    m_alivePagesReadBackBuffer.Unmap();
    std::vector<D3D12_TILED_RESOURCE_COORDINATE> startCoordinates;
    std::vector<D3D12_TILE_REGION_SIZE> regionSizes;
    std::vector<UINT> heapRangeStartOffsets;
    std::vector<D3D12_TILE_RANGE_FLAGS> rangeFlags;
    std::vector<UINT> rangeTileCounts;
   
   // for each (U32 i in active_pages)

    const UINT tile_width = m_TileShape.WidthInTexels;
    const UINT tile_height = m_TileShape.HeightInTexels;
    for each (int i in active_pages)
    {
        PageInfo& page = m_pages[i];
        if(page.m_mem)
            continue;
        const UINT width = m_resTexWidth >> page.mipLevel;
        const UINT height = m_resTexHeight >> page.mipLevel;
        std::vector<UINT8> data = GenerateTextureData(page.start_corordinate.X * tile_width, page.start_corordinate.Y * tile_height, tile_width, tile_height, page.mipLevel);
        page.LoadData(std::move(data));
        startCoordinates.push_back(page.start_corordinate);
        regionSizes.push_back(page.regionSize);
        heapRangeStartOffsets.push_back(i);
        rangeTileCounts.push_back(1);
       
        rangeFlags.push_back(D3D12_TILE_RANGE_FLAG_NONE);
        if (page.is_packed)
        {
            D3D12_RESOURCE_DESC         Desc = m_pResource->GetDesc();
            D3D12_TEXTURE_COPY_LOCATION Dst = {};
            Dst.pResource = m_pResource.Get();
            Dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            D3D12_TEXTURE_COPY_LOCATION Src = {};
            Src.pResource = page.m_mem->Buffer.GetResource();
            std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layout(m_packedMipInfo.NumPackedMips);
            g_Device->GetCopyableFootprints(&Desc, page.start_corordinate.Subresource, m_packedMipInfo.NumPackedMips, 0, &layout[0], nullptr, nullptr, nullptr);
            
            for(size_t sub_index = 0; sub_index <m_packedMipInfo.NumPackedMips;sub_index++)
            {
                Src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                Src.PlacedFootprint = layout[sub_index];
                Dst.SubresourceIndex = sub_index + page.start_corordinate.Subresource;
                gContext.GetCommandList()->CopyTextureRegion(&Dst,0, 0, 0, &Src, NULL);
            }
        }
        else
        {
            D3D12_RESOURCE_DESC         Desc = m_pResource->GetDesc();
            D3D12_TEXTURE_COPY_LOCATION Dst = {};
            Dst.pResource = m_pResource.Get();
            Dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            Dst.SubresourceIndex = page.start_corordinate.Subresource;
            D3D12_TEXTURE_COPY_LOCATION Src = {};
            Src.pResource = page.m_mem->Buffer.GetResource();
            Src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            Src.PlacedFootprint =  D3D12_PLACED_SUBRESOURCE_FOOTPRINT{ 0,
                                    { Desc.Format,
                                        tile_width, tile_height, 1, tile_width * sizeof(uint32_t) } };//BitsPerPixels
            gContext.GetCommandList()->CopyTextureRegion(&Dst, page.start_corordinate.X * tile_width, page.start_corordinate.Y * tile_height, 0, &Src, NULL);
       }
    }
    if(regionSizes.size())
    {
        Graphics::g_CommandManager.GetCommandQueue()->UpdateTileMappings(
            m_pResource.Get(),
            startCoordinates.size(),
            startCoordinates.data(),
            regionSizes.data(),
            m_heap.Get(),
            regionSizes.size(),
            rangeFlags.data(),
            heapRangeStartOffsets.data(),
            rangeTileCounts.data(),
            D3D12_TILE_MAPPING_FLAG_NONE);
    }

}

void TiledTexture::RemovePages(GraphicsContext& gContext)
{
    int removed_page_count = *static_cast<int*>(m_removedPagesCounterReadBackBuffer.Map());
    m_removedPagesCounterReadBackBuffer.Unmap();
    if (removed_page_count == 0)
        return;
    std::vector<uint32_t> removed_pages;
    removed_pages.resize(removed_page_count);
    memcpy(removed_pages.data(), m_removedPagesReadBackBuffer.Map(), removed_page_count * sizeof(int));
    m_removedPagesReadBackBuffer.Unmap();
    for each (int i in removed_pages)
    {
        PageInfo& page = m_pages[i];
        //page.m_mem->~DynAlloc();
        page.m_mem = nullptr;
        std::vector<D3D12_TILED_RESOURCE_COORDINATE> startCoordinates;
        std::vector<D3D12_TILE_REGION_SIZE> regionSizes;
        std::vector<UINT> heapRangeStartOffsets;
        std::vector<D3D12_TILE_RANGE_FLAGS> rangeFlags;
        std::vector<UINT> rangeTileCounts;
        startCoordinates.push_back(page.start_corordinate);
        regionSizes.push_back(page.regionSize);
        heapRangeStartOffsets.push_back(i);
        rangeTileCounts.push_back(1);
        rangeFlags.push_back(D3D12_TILE_RANGE_FLAG_NULL);
        Graphics::g_CommandManager.GetCommandQueue()->UpdateTileMappings(
            m_pResource.Get(),
            startCoordinates.size(),
            startCoordinates.data(),
            regionSizes.data(),
            m_heap.Get(),
            regionSizes.size(),
            rangeFlags.data(),
            heapRangeStartOffsets.data(),
            rangeTileCounts.data(),
            D3D12_TILE_MAPPING_FLAG_NONE);

    }
}

void TiledTexture::Update(GraphicsContext& gfxContext)
{
    ScopedTimer _prof4(L"Pages Update", gfxContext);
    UpdateVisibilityBuffer(gfxContext.GetComputeContext());
    {
        gfxContext.TransitionResource(*this, D3D12_RESOURCE_STATE_COPY_DEST, true);
     //   UpdateTileMapping(gfxContext);
        gfxContext.TransitionResource(*this, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }
    {
        gfxContext.TransitionResource(*this, D3D12_RESOURCE_STATE_COPY_DEST, true);
        AddPages(gfxContext);
        RemovePages(gfxContext);
        gfxContext.TransitionResource(*this, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }
}

std::vector<UINT8> TiledTexture::GenerateTextureData(UINT offsetX, UINT offsetY, UINT W, UINT H, UINT currentMip)
{
    if (currentMip == 3)
    {
        auto result =  GenerateTextureData(3, 7);
        W = H = 128;
        UINT dataSize = W * H* m_resTexPixelInBytes;
        std::vector<UINT8> data(dataSize);
        UINT8* pData = &data[0];
        int i = 0;
      //  for (; i < std::min<UINT>(dataSize,result.size()); i++)
        //    pData[i] = result[i];
        for (; i < dataSize; i++)
            pData[i] = 0xff;
        return data;
    }
    UINT dataSize = W*H* m_resTexPixelInBytes;
    std::vector<UINT8> data(dataSize);
    UINT8* pData = &data[0];
    const UINT pitch = m_resTexWidth >> currentMip;
    const UINT heightpitch = m_resTexHeight >> currentMip;
    const UINT cellPitch = std::max<UINT>(pitch >> 3, 1);    // The width of a cell in the checkboard texture.
    const UINT cellHeight = std::max<UINT>(heightpitch >> 3, 1);
    INT index = 0;
    for (UINT y = offsetY; y < std::min(heightpitch, offsetY + H); y++)
    {
        for (UINT x = offsetX; x < std::min(pitch, offsetX + W); x++)
        {
            UINT i = x / cellPitch;
            UINT j = y / cellHeight;
            if (index > dataSize)
                break;;
            if (i % 2 == j % 2)
            {
                pData[index++] = 0xff;    // R
                pData[index++] = 0x00;    // G
                pData[index++] = 0x00;    // B
                pData[index++] = 0xff;    // A
            }
            else
            {
                pData[index++] = 0xff;    // R
                pData[index++] = 0xff;    // G
                pData[index++] = 0xff;    // B
                pData[index++] = 0xff;    // A
            }
        }
    }
    return data;
}

std::vector<UINT8> TiledTexture::GenerateTextureData(UINT firstMip, UINT mipCount)
{
    // Determine the size of the data required by the mips(s).
    UINT dataSize = (m_resTexWidth >> firstMip) * (m_resTexHeight >> firstMip) * m_resTexPixelInBytes;
    if (mipCount > 1)
    {
        // If generating more than 1 mip, double the size of the texture allocation
        // (you will never need more than this).
        dataSize *= 2;
    }
    std::vector<UINT8> data(dataSize);
    UINT8* pData = &data[0];

    UINT index = 0;
    for (UINT n = 0; n < mipCount; n++)
    {
        const UINT currentMip = firstMip + n;
        const UINT width = m_resTexWidth >> currentMip;
        const UINT height = m_resTexHeight >> currentMip;
        const UINT rowPitch = width * m_resTexPixelInBytes;
        const UINT cellPitch = std::max(rowPitch >> 3, m_resTexPixelInBytes);    // The width of a cell in the checkboard texture.
        const UINT cellHeight = std::max<U32>(height >> 3, 1);                        // The height of a cell in the checkerboard texture.
        const UINT textureSize = rowPitch * height;

        for (UINT m = 0; m < textureSize; m += m_resTexPixelInBytes)
        {
            UINT x = m % rowPitch;
            UINT y = m / rowPitch;
            UINT i = x / cellPitch;
            UINT j = y / cellHeight;

            if (i % 2 == j % 2)
            {
                pData[index++] = 0xff;    // R
                pData[index++] = 0x00;    // G
                pData[index++] = 0x00;    // B
                pData[index++] = 0xff;    // A
            }
            else
            {
                pData[index++] = 0xff;    // R
                pData[index++] = 0xff;    // G
                pData[index++] = 0xff;    // B
                pData[index++] = 0xff;    // A
            }
        }
    }
    return data;
}

namespace TextureManager
{
    wstring s_RootPath = L"";
    map< wstring, unique_ptr<ManagedTexture> > s_TextureCache;

    void Initialize( const std::wstring& TextureLibRoot )
    {
        s_RootPath = TextureLibRoot;
    }

    void Shutdown( void )
    {
        s_TextureCache.clear();
    }

    pair<ManagedTexture*, bool> FindOrLoadTexture( const wstring& fileName )
    {
        static mutex s_Mutex;
        lock_guard<mutex> Guard(s_Mutex);

        auto iter = s_TextureCache.find(fileName);

        // If it's found, it has already been loaded or the load process has begun
        if (iter != s_TextureCache.end())
            return make_pair(iter->second.get(), false);

        ManagedTexture* NewTexture = new ManagedTexture(fileName);
        s_TextureCache[fileName].reset( NewTexture );

        // This was the first time it was requested, so indicate that the caller must read the file
        return make_pair(NewTexture, true);
    }

    const Texture& GetBlackTex2D(void)
    {
        auto ManagedTex = FindOrLoadTexture(L"DefaultBlackTexture");

        ManagedTexture* ManTex = ManagedTex.first;
        const bool RequestsLoad = ManagedTex.second;

        if (!RequestsLoad)
        {
            ManTex->WaitForLoad();
            return *ManTex;
        }

        uint32_t BlackPixel = 0;
        ManTex->Create(1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, &BlackPixel);
        return *ManTex;
    }

    const Texture& GetWhiteTex2D(void)
    {
        auto ManagedTex = FindOrLoadTexture(L"DefaultWhiteTexture");

        ManagedTexture* ManTex = ManagedTex.first;
        const bool RequestsLoad = ManagedTex.second;

        if (!RequestsLoad)
        {
            ManTex->WaitForLoad();
            return *ManTex;
        }

        uint32_t WhitePixel = 0xFFFFFFFFul;
        ManTex->Create(1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, &WhitePixel);
        return *ManTex;
    }

    const Texture& GetMagentaTex2D(void)
    {
        auto ManagedTex = FindOrLoadTexture(L"DefaultMagentaTexture");

        ManagedTexture* ManTex = ManagedTex.first;
        const bool RequestsLoad = ManagedTex.second;

        if (!RequestsLoad)
        {
            ManTex->WaitForLoad();
            return *ManTex;
        }

        uint32_t MagentaPixel = 0x00FF00FF;
        ManTex->Create(1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, &MagentaPixel);
        return *ManTex;
    }

} // namespace TextureManager

void ManagedTexture::WaitForLoad( void ) const
{
    volatile D3D12_CPU_DESCRIPTOR_HANDLE& VolHandle = (volatile D3D12_CPU_DESCRIPTOR_HANDLE&)m_hCpuDescriptorHandle;
    volatile bool& VolValid = (volatile bool&)m_IsValid;
    while (VolHandle.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN && VolValid)
        this_thread::yield();
}

void ManagedTexture::SetToInvalidTexture( void )
{
    m_hCpuDescriptorHandle = TextureManager::GetMagentaTex2D().GetSRV();
    m_IsValid = false;
}

const ManagedTexture* TextureManager::LoadFromFile( const std::wstring& fileName, bool sRGB )
{
    std::wstring CatPath = fileName;

    const ManagedTexture* Tex = LoadDDSFromFile( CatPath + L".dds", sRGB );
    if (!Tex->IsValid())
        Tex = LoadTGAFromFile( CatPath + L".tga", sRGB );

    return Tex;
}

const ManagedTexture* TextureManager::LoadDDSFromFile( const std::wstring& fileName, bool sRGB )
{
    auto ManagedTex = FindOrLoadTexture(fileName);

    ManagedTexture* ManTex = ManagedTex.first;
    const bool RequestsLoad = ManagedTex.second;

    if (!RequestsLoad)
    {
        ManTex->WaitForLoad();
        return ManTex;
    }

    Utility::ByteArray ba = Utility::ReadFileSync( s_RootPath + fileName );
    if (ba->size() == 0 || !ManTex->CreateDDSFromMemory( ba->data(), ba->size(), sRGB ))
        ManTex->SetToInvalidTexture();
    else
        ManTex->GetResource()->SetName(fileName.c_str());

    return ManTex;
}

const ManagedTexture* TextureManager::LoadTGAFromFile( const std::wstring& fileName, bool sRGB )
{
    auto ManagedTex = FindOrLoadTexture(fileName);

    ManagedTexture* ManTex = ManagedTex.first;
    const bool RequestsLoad = ManagedTex.second;

    if (!RequestsLoad)
    {
        ManTex->WaitForLoad();
        return ManTex;
    }

    Utility::ByteArray ba = Utility::ReadFileSync( s_RootPath + fileName );
    if (ba->size() > 0)
    {
        ManTex->CreateTGAFromMemory( ba->data(), ba->size(), sRGB );
        ManTex->GetResource()->SetName(fileName.c_str());
    }
    else
        ManTex->SetToInvalidTexture();

    return ManTex;
}


const ManagedTexture* TextureManager::LoadPIXImageFromFile( const std::wstring& fileName )
{
    auto ManagedTex = FindOrLoadTexture(fileName);

    ManagedTexture* ManTex = ManagedTex.first;
    const bool RequestsLoad = ManagedTex.second;

    if (!RequestsLoad)
    {
        ManTex->WaitForLoad();
        return ManTex;
    }

    Utility::ByteArray ba = Utility::ReadFileSync( s_RootPath + fileName );
    if (ba->size() > 0)
    {
        ManTex->CreatePIXImageFromMemory(ba->data(), ba->size());
        ManTex->GetResource()->SetName(fileName.c_str());
    }
    else
        ManTex->SetToInvalidTexture();

    return ManTex;
}
