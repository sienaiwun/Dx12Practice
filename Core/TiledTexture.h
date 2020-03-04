
#pragma once

#pragma  region HEADER
#include "pch.h"
#include "GpuBuffer.h"
#include "ReadbackBuffer.h"
#include "PageInfo.h"
#include "PipelineState.h"
#include "RootSignature.h"
#include "Utility.h"
#pragma region 



enum TiledComputerParams :unsigned char
{
    PageCountInfo,
    Buffers,
    NumComputeParams,
};

class TiledTexture : public GpuResource
{
public:
    void Create(U32 Width, U32 Height, DXGI_FORMAT Format);
    void Update(GraphicsContext& gfxContext);
    void LevelUp()
    {
        if (m_activeMip < m_MipLevels)
        {
            m_activeMip = static_cast<U32>(std::min(m_MipLevels-1, m_activeMip + 1));
        }
    }

    void LevelDown()
    {
        if (m_activeMip != 0)
        {
            m_activeMip--;
        }
    }

    inline const UINT GetActiveMip() const
    {
        return m_activeMip;
    }

    inline const UINT GetMipsLevel() const
    {
        return static_cast<UINT>(m_MipLevels);
    }

    inline const UINT GetTiledWidth() const
    {
        return static_cast<UINT>(m_TileShape.WidthInTexels);
    }

    inline const UINT GetTiledHeight() const
    {
        return static_cast<UINT>(m_TileShape.HeightInTexels);
    }

    inline const UINT GetVirtualWidth() const
    {
        return static_cast<UINT>(m_resTexWidth);
    }

    inline const UINT GetVirtualHeight() const
    {
        return static_cast<UINT>(m_resTexHeight);
    }

    void UpdateVisibilityBuffer(ComputeContext& context);
    virtual void Destroy() override
    {
        GpuResource::Destroy();
        m_hCpuDescriptorHandle.ptr = 0;
        m_removedPagesReadBackBuffer.Destroy();
        m_visibilityBuffer.Destroy();
        m_prevVisBuffer.Destroy();
        m_alivePagesBuffer.Destroy();
        m_removedPagesBuffer.Destroy();
        m_alivePagesCounterBuffer.Destroy();
        m_removedPagesCounterBuffer.Destroy();
        m_alivePagesReadBackBuffer.Destroy();
        m_alivePagesCounterReadBackBuffer.Destroy();
        m_removedPagesCounterReadBackBuffer.Destroy();
        if(m_page_heaps)
            m_page_heaps->Release();
        for (PageInfo& page : m_pages)
        {
            if (page.m_mem)
                page.m_mem->Destroy();
        }
        m_pages.clear();
    }

    const D3D12_CPU_DESCRIPTOR_HANDLE& GetSRV() const { return m_hCpuDescriptorHandle; }

    const D3D12_CPU_DESCRIPTOR_HANDLE& GetVisibilityUAV() const { return m_visibilityBuffer.GetUAV(); }

    const D3D12_CPU_DESCRIPTOR_HANDLE& GetVisibilitySRV()  const { return m_visibilityBuffer.GetSRV(); }

    const D3D12_CPU_DESCRIPTOR_HANDLE& GetPrevVisibUAV() const { return m_prevVisBuffer.GetUAV(); }

    const D3D12_CPU_DESCRIPTOR_HANDLE& GetPrevVisibSRV()  const { return m_prevVisBuffer.GetSRV(); }

    bool operator!() { return m_hCpuDescriptorHandle.ptr == 0; }

protected:
    void RemovePages();
    void AddPages(GraphicsContext& gContext);
private:

    struct MipInfo
    {
        UINT heapRangeIndex;
        bool packedMip;
        bool mapped;
        D3D12_TILED_RESOURCE_COORDINATE startCoordinate;
        D3D12_TILE_REGION_SIZE regionSize;
    };

    std::vector<UINT8> GenerateTextureData(UINT offsetX, UINT offsetY, UINT width, UINT height, UINT mip_level);
    std::vector<PageInfo> m_pages;
    U32 m_resTexPixelInBytes;
    U32 m_resTexWidth, m_resTexHeight;
    U32 m_activeMip,m_MipLevels;
    Microsoft::WRL::ComPtr<ID3D12Heap> m_page_heaps;
    D3D12_PACKED_MIP_INFO m_packedMipInfo;
    D3D12_TILE_SHAPE m_TileShape;
    D3D12_CPU_DESCRIPTOR_HANDLE m_hCpuDescriptorHandle;
    StructuredBuffer m_visibilityBuffer;
    StructuredBuffer m_prevVisBuffer;
    StructuredBuffer m_alivePagesBuffer;
    StructuredBuffer m_removedPagesBuffer;
    ByteAddressBuffer m_alivePagesCounterBuffer;
    ByteAddressBuffer m_removedPagesCounterBuffer;
    ReadbackBuffer m_alivePagesReadBackBuffer;
    ReadbackBuffer m_removedPagesReadBackBuffer;
    ReadbackBuffer m_alivePagesCounterReadBackBuffer;
    ReadbackBuffer m_removedPagesCounterReadBackBuffer;
    ComputePSO m_computePSO;
    RootSignature m_rootSig;
    std::unique_ptr<PageAllocator> m_cpu_pages_allocator;
};