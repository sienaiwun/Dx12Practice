#include "pch.h"
#include "GraphicsCore.h"
#include "CommandContext.h"
#include "TiledTexture.h"
#include "CompiledShaders/FillPage.h"
#include <map>
#include <thread>

using namespace Graphics;

static std::vector<UINT8> GenerateTextureDataHelper(const U32 totalWidth, const U32 totalHeight, const  U32 pixelInPytes, const U32 offsetX, const  U32 offsetY, const U32 W, const U32 H, const  U32 mip_level, const U32 mipCount)
{
    U32 dataSize = W * H* pixelInPytes;
    std::vector<UINT8> data(dataSize);
    UINT8* pData = &data[0];
    U32 index = 0;
    U32 currentMip = mip_level;
    for (U32 count = 0; count < mipCount; count++)
    {
        const U32 pitch = Math::AlignUp(totalWidth >> currentMip, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT / 4);
        const U32 heightpitch = totalHeight >> currentMip;
        const U32 cellHeight = std::max<U32>(heightpitch >> 3, 1);
        const U32 cellPitch = cellHeight;    // The width of a cell in the checkboard texture.

        for (U32 y = offsetY; y < offsetY + std::min(H, heightpitch); y++)
        {
            for (U32 x = offsetX; x < offsetX + std::min(W, pitch); x++)
            {
                U32 i = x / cellPitch;
                U32 j = y / cellHeight;
                if (index > dataSize)
                    return data;
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
        currentMip++;
    }
    return data;
}

void TiledTexture::Create(U32 Width, U32 Height, DXGI_FORMAT Format)
{
    Destroy();

    m_resTexWidth = Width;
    m_resTexHeight = Height;
    m_resTexPixelInBytes = 4;
    m_activeMip = 0;
    U32 mipLevels = 0;
    for (U32 w = m_resTexWidth, h = m_resTexHeight; w > 0 && h > 0; w >>= 1, h >>= 1)
    {
        mipLevels++;
    }
    m_activeMip = static_cast<U8>(mipLevels - 1);    // Show the least detailed mip first.
    m_MipLevels = mipLevels;
    D3D12_RESOURCE_DESC reservedTextureDesc{};
    reservedTextureDesc.MipLevels = static_cast<UINT16>(m_MipLevels);
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


    U32 numTiles = 0;
    U32 subresourceCount = reservedTextureDesc.MipLevels;
    std::vector<D3D12_SUBRESOURCE_TILING> tilings(subresourceCount);
    m_TileShape = {};
    g_Device->GetResourceTiling(m_pResource.Get(), &numTiles, &m_packedMipInfo, &m_TileShape, &subresourceCount, 0, &tilings[0]);
    
    U32x3 imageGranularity;
    imageGranularity[0] = m_TileShape.WidthInTexels;
    imageGranularity[1] = m_TileShape.HeightInTexels;
    imageGranularity[2] = m_TileShape.DepthInTexels;

    m_pages.resize(0);
    size_t heapOffset = 0;
    for (U32 mipLevel = 0; mipLevel < m_packedMipInfo.NumStandardMips; mipLevel++)
    {
        U32x3 extent;
        extent[0] = std::max<U32>(m_resTexWidth >> mipLevel, 1u);
        extent[1] = std::max<U32>(m_resTexHeight >> mipLevel, 1u);
        extent[2] = std::max<U32>(1 >> mipLevel, 1u);

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
                start_coordinate.X = x;
                start_coordinate.Y = y;
                start_coordinate.Z = 0;
                start_coordinate.Subresource = mipLevel;
                // Size of the page
                PageInfo page{ };
                page.regionSize.Width = 1;
                page.regionSize.Height = 1;
                page.regionSize.Depth = 1;
                page.regionSize.NumTiles = 1;
                page.regionSize.UseBox = TRUE;
                page.mipLevel = mipLevel;
                page.start_corordinate = start_coordinate;
                page.is_packed = false;
                heapOffset += D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
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
    m_pages.emplace_back(std::move(page));
    m_visibilityBuffer.Create(L"visibilityBuffer", (U32)m_pages.size(), sizeof(int), nullptr);
    m_prevVisBuffer.Create(L"preVisBuffer", (U32)m_pages.size(), sizeof(int), nullptr);
    m_alivePagesBuffer.Create(L"aliveBuffer", (U32)m_pages.size(), sizeof(int), nullptr);
    m_removedPagesBuffer.Create(L"removedPageBuffer", (U32)m_pages.size(), sizeof(int), nullptr);
    m_alivePagesReadBackBuffer.Create(L"alivePagesReadBackBuffer", (U32)m_pages.size(), sizeof(int));
    m_removedPagesReadBackBuffer.Create(L"removedPagesReadbackBuffer", (U32)m_pages.size(), sizeof(int));
    m_alivePagesCounterReadBackBuffer.Create(L"alivePagesCounterReadBackBuffer", 1, sizeof(int));
    m_removedPagesCounterReadBackBuffer.Create(L"removedPagesCounterReadBackBuffer", 1, sizeof(int));
    m_alivePagesCounterBuffer.Create(L"alivePageBufferCounter", 1, sizeof(int));
    m_removedPagesCounterBuffer.Create(L"removedPageBufferCounter", 1, sizeof(int));
    m_cpu_pages_allocator = std::make_unique<PageAllocator>(m_TileShape.WidthInTexels*m_TileShape.HeightInTexels*m_resTexPixelInBytes);

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

void TiledTexture::UpdateVisibilityBuffer(ComputeContext& computeContext)
{
    computeContext.SetRootSignature(m_rootSig);
    computeContext.SetPipelineState(m_computePSO);
    computeContext.FillBuffer(m_alivePagesCounterBuffer, 0, 0, sizeof(U32));
    computeContext.FillBuffer(m_removedPagesCounterBuffer, 0, 0, sizeof(U32));
    computeContext.TransitionResource(m_visibilityBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    computeContext.TransitionResource(m_prevVisBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    computeContext.TransitionResource(m_alivePagesBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    computeContext.TransitionResource(m_removedPagesBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    computeContext.TransitionResource(m_alivePagesCounterBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    computeContext.TransitionResource(m_removedPagesCounterBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    D3D12_CPU_DESCRIPTOR_HANDLE handles[6] = { m_prevVisBuffer.GetUAV(),m_visibilityBuffer.GetUAV(),m_alivePagesBuffer.GetUAV(),m_alivePagesCounterBuffer.GetUAV(),m_removedPagesBuffer.GetUAV(),m_removedPagesCounterBuffer.GetUAV() };
    computeContext.SetDynamicDescriptors(TiledComputerParams::Buffers, 0, 6, handles);
    computeContext.SetConstants(TiledComputerParams::PageCountInfo, (U32)m_pages.size(), 0, 0, 0);
    computeContext.Dispatch3D(m_pages.size(), 1, 1, 1024, 1, 1);
    computeContext.CopyBuffer(m_alivePagesCounterReadBackBuffer, m_alivePagesCounterBuffer);
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
    std::vector<U32> heapRangeStartOffsets;
    std::vector<D3D12_TILE_RANGE_FLAGS> rangeFlags;
    std::vector<U32> rangeTileCounts;

    // for each (U32 i in active_pages)

    const U32 tile_width = m_TileShape.WidthInTexels;
    const U32 tile_height = m_TileShape.HeightInTexels;
    for each (int i in active_pages)
    {
        PageInfo& page = m_pages[i];
        if (page.m_mem)
            continue;
        const U32 width = m_resTexWidth >> page.mipLevel;
        const U32 height = m_resTexHeight >> page.mipLevel;
        std::vector<UINT8> data = GenerateTextureData(page.start_corordinate.X * tile_width, page.start_corordinate.Y * tile_height, tile_width, tile_height, page.mipLevel);
        page.LoadData(std::move(data),*m_cpu_pages_allocator);
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

            for (U32 sub_index = 0; sub_index < m_packedMipInfo.NumPackedMips; sub_index++)
            {
                Src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                Src.PlacedFootprint = layout[sub_index];
                Dst.SubresourceIndex = sub_index + page.start_corordinate.Subresource;
                gContext.GetCommandList()->CopyTextureRegion(&Dst, 0, 0, 0, &Src, NULL);
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
            Src.PlacedFootprint = D3D12_PLACED_SUBRESOURCE_FOOTPRINT{ 0,
                                    { Desc.Format,
                                        tile_width, tile_height, 1, tile_width * sizeof(U32) } };//BitsPerPixels
            gContext.GetCommandList()->CopyTextureRegion(&Dst, page.start_corordinate.X * tile_width, page.start_corordinate.Y * tile_height, 0, &Src, NULL);
        }
    }
    if (regionSizes.size())
    {
        Graphics::g_CommandManager.GetCommandQueue()->UpdateTileMappings(
            m_pResource.Get(),
            (U32)startCoordinates.size(),
            startCoordinates.data(),
            regionSizes.data(),
            m_page_heaps.Get(),
            (U32)regionSizes.size(),
            rangeFlags.data(),
            heapRangeStartOffsets.data(),
            rangeTileCounts.data(),
            D3D12_TILE_MAPPING_FLAG_NONE);
    }
}

void TiledTexture::RemovePages()
{
    int removed_page_count = *static_cast<int*>(m_removedPagesCounterReadBackBuffer.Map());
    m_removedPagesCounterReadBackBuffer.Unmap();
    if (removed_page_count == 0)
        return;
    std::vector<U32> removed_pages;
    removed_pages.resize(removed_page_count);
    memcpy(removed_pages.data(), m_removedPagesReadBackBuffer.Map(), removed_page_count * sizeof(int));
    m_removedPagesReadBackBuffer.Unmap();
    for each (int i in removed_pages)
    {
        PageInfo& page = m_pages[i];
        if (page.m_mem)
            continue;
        page.m_mem = nullptr;
        std::vector<D3D12_TILED_RESOURCE_COORDINATE> startCoordinates;
        std::vector<D3D12_TILE_REGION_SIZE> regionSizes;
        std::vector<U32> heapRangeStartOffsets;
        std::vector<D3D12_TILE_RANGE_FLAGS> rangeFlags;
        std::vector<U32> rangeTileCounts;
        startCoordinates.push_back(page.start_corordinate);
        regionSizes.push_back(page.regionSize);
        heapRangeStartOffsets.push_back(i);
        rangeTileCounts.push_back(1);
        rangeFlags.push_back(D3D12_TILE_RANGE_FLAG_NULL);
        Graphics::g_CommandManager.GetCommandQueue()->UpdateTileMappings(
            m_pResource.Get(),
            (U32)startCoordinates.size(),
            startCoordinates.data(),
            regionSizes.data(),
            m_page_heaps.Get(),
            (U32)regionSizes.size(),
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
        AddPages(gfxContext);
        RemovePages();
        gfxContext.TransitionResource(*this, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }
}

std::vector<UINT8> TiledTexture::GenerateTextureData(U32 offsetX, U32 offsetY, U32 W, U32 H, U32 currentMip)
{
    if (currentMip <= 2)
        return GenerateTextureDataHelper(m_resTexWidth,m_resTexHeight,m_resTexPixelInBytes,offsetX, offsetY, W, H, currentMip,1);
    else
    {
       return  GenerateTextureDataHelper(m_resTexWidth, m_resTexHeight, m_resTexPixelInBytes, offsetX, offsetY, W, H, currentMip, m_packedMipInfo.NumPackedMips);
    }
   
}

