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

#pragma once

#include "pch.h"
#include "GpuBuffer.h"
#include "ReadbackBuffer.h"
#include "LinearAllocator.h"
#include "PipelineState.h"
#include "RootSignature.h"
#include "Utility.h"

class Texture : public GpuResource
{
    friend class CommandContext;

public:

    Texture() { m_hCpuDescriptorHandle.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN; }
    Texture(D3D12_CPU_DESCRIPTOR_HANDLE Handle) : m_hCpuDescriptorHandle(Handle) {}

    // Create a 1-level 2D texture
    void Create(size_t Pitch, size_t Width, size_t Height, DXGI_FORMAT Format, const void* InitData );
    void Create(size_t Width, size_t Height, DXGI_FORMAT Format, const void* InitData )
    {
        Create(Width, Width, Height, Format, InitData);
    }

    void CreateTGAFromMemory( const void* memBuffer, size_t fileSize, bool sRGB );
    bool CreateDDSFromMemory( const void* memBuffer, size_t fileSize, bool sRGB );
    void CreatePIXImageFromMemory( const void* memBuffer, size_t fileSize );

    virtual void Destroy() override
    {
        GpuResource::Destroy();
        // This leaks descriptor handles.  We should really give it back to be reused.
        m_hCpuDescriptorHandle.ptr = 0;
    }

    const D3D12_CPU_DESCRIPTOR_HANDLE& GetSRV() const { return m_hCpuDescriptorHandle; }

    bool operator!() { return m_hCpuDescriptorHandle.ptr == 0; }

protected:

    D3D12_CPU_DESCRIPTOR_HANDLE m_hCpuDescriptorHandle;
};

class ManagedTexture : public Texture
{
public:
    ManagedTexture( const std::wstring& FileName ) : m_MapKey(FileName), m_IsValid(true) {}

    void operator= ( const Texture& Texture );

    void WaitForLoad(void) const;
    void Unload(void);

    void SetToInvalidTexture(void);
    bool IsValid(void) const { return m_IsValid; }

private:
    std::wstring m_MapKey;        // For deleting from the map later
    bool m_IsValid;
};



enum TiledComputerParams :unsigned char
{
    PageCountInfo,
    Buffers,
    NumComputeParams,
};

struct PageInfo
{
    D3D12_TILED_RESOURCE_COORDINATE start_corordinate;
    D3D12_TILE_REGION_SIZE regionSize;
    U8 mipLevel;
    size_t heapOffset = 0;
    std::unique_ptr<DynAlloc> m_mem = nullptr;
    bool is_packed = false;
    PageInfo() :m_mem(nullptr) {    }
    PageInfo(const PageInfo &);
    ~PageInfo() = default;
    void LoadData(std::vector<UINT8> data);
};

class TiledTexture : public GpuResource
{
public :
    void Create(size_t Width, size_t Height, DXGI_FORMAT Format);
    void Update(GraphicsContext& gfxContext);
    void LevelUp()
    {
        if (m_activeMip < 10)
        {
            m_activeMip =  static_cast<U8>( std::min(9, m_activeMip + 1));
            m_activeMipChanged = true;
        }
    }

    void LevelDown()
    {
        if (m_activeMip != 0)
        {
            m_activeMip--;
            m_activeMipChanged = true;
        }
    }

    inline const UINT GetActiveMip() const
    {
        return m_activeMip;
    }

    inline const UINT GetMipsLevel() const
    {
        return static_cast<UINT>(m_mips.size());
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

    void UpdateTileMapping(GraphicsContext& gfxContext);
    void UpdateVisibilityBuffer(ComputeContext& context);
    virtual void Destroy() override
    {
        GpuResource::Destroy();
        // This leaks descriptor handles.  We should really give it back to be reused.
        m_hCpuDescriptorHandle.ptr = 0;
    }

    const D3D12_CPU_DESCRIPTOR_HANDLE& GetSRV() const { return m_hCpuDescriptorHandle; }

    const D3D12_CPU_DESCRIPTOR_HANDLE& GetVisibilityUAV() const { return m_visibilityBuffer.GetUAV(); }

    const D3D12_CPU_DESCRIPTOR_HANDLE& GetVisibilitySRV()  const { return m_visibilityBuffer.GetSRV(); }

    const D3D12_CPU_DESCRIPTOR_HANDLE& GetPrevVisibUAV() const { return m_prevVisBuffer.GetUAV(); }

    const D3D12_CPU_DESCRIPTOR_HANDLE& GetPrevVisibSRV()  const { return m_prevVisBuffer.GetSRV(); }

    bool operator!() { return m_hCpuDescriptorHandle.ptr == 0; }

protected:
    void RemovePages(GraphicsContext& gContext);
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
    std::vector<UINT8> GenerateTextureData(UINT firstMip, UINT mipCount);
    std::vector<MipInfo> m_mips;
    std::vector<PageInfo> m_pages;
    std::vector<U32> m_heap_offsets;
    U32 m_resTexPixelInBytes;
    size_t m_resTexWidth, m_resTexHeight;
    U8 m_activeMip;
    bool m_activeMipChanged;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_uploadBuffer;
    D3D12_PACKED_MIP_INFO m_packedMipInfo;
    Microsoft::WRL::ComPtr<ID3D12Heap> m_heap;
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
    Microsoft::WRL::ComPtr<ID3D12Heap> m_page_heaps;
    ComputePSO m_computePSO;
    RootSignature m_rootSig;
};

namespace TextureManager
{
    void Initialize( const std::wstring& TextureLibRoot );
    void Shutdown(void);

    const ManagedTexture* LoadFromFile( const std::wstring& fileName, bool sRGB = false );
    const ManagedTexture* LoadDDSFromFile( const std::wstring& fileName, bool sRGB = false );
    const ManagedTexture* LoadTGAFromFile( const std::wstring& fileName, bool sRGB = false );
    const ManagedTexture* LoadPIXImageFromFile( const std::wstring& fileName );

    inline const ManagedTexture* LoadFromFile( const std::string& fileName, bool sRGB = false )
    {
        return LoadFromFile(MakeWStr(fileName), sRGB);
    }

    inline const ManagedTexture* LoadDDSFromFile( const std::string& fileName, bool sRGB = false )
    {
        return LoadDDSFromFile(MakeWStr(fileName), sRGB);
    }

    inline const ManagedTexture* LoadTGAFromFile( const std::string& fileName, bool sRGB = false )
    {
        return LoadTGAFromFile(MakeWStr(fileName), sRGB);
    }

    inline const ManagedTexture* LoadPIXImageFromFile( const std::string& fileName )
    {
        return LoadPIXImageFromFile(MakeWStr(fileName));
    }

    const Texture& GetBlackTex2D(void);
    const Texture& GetWhiteTex2D(void);
}
