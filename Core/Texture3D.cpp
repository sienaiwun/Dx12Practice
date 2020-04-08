
#include "pch.h"
#include "Texture3D.h"
#include "GraphicsCore.h"
#include "CommandContext.h"
#include "EsramAllocator.h"

using namespace Graphics;


void Texture3D::CreateDerivedViews(ID3D12Device* Device, DXGI_FORMAT Format, uint32_t ArraySize, uint32_t NumMips)
{
    ASSERT(ArraySize > 1);
    D3D12_RENDER_TARGET_VIEW_DESC RTVDesc = {};
    D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
    D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};

    RTVDesc.Format = Format;
    UAVDesc.Format = GetUAVFormat(Format);
    SRVDesc.Format = Format;
    SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
    RTVDesc.Texture2DArray.MipSlice = 0;
    RTVDesc.Texture2DArray.FirstArraySlice = 0;
    RTVDesc.Texture2DArray.ArraySize = (UINT)ArraySize;

    UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
    UAVDesc.Texture2DArray.MipSlice = 0;
    UAVDesc.Texture2DArray.FirstArraySlice = 0;
    UAVDesc.Texture2DArray.ArraySize = (UINT)ArraySize;

    SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
    SRVDesc.Texture2DArray.MipLevels = NumMips;
    SRVDesc.Texture2DArray.MostDetailedMip = 0;
    SRVDesc.Texture2DArray.FirstArraySlice = 0;
    SRVDesc.Texture2DArray.ArraySize = (UINT)ArraySize;

    if (m_SRVHandle.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
    {
        m_RTVHandle = Graphics::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        m_SRVHandle = Graphics::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }

    ID3D12Resource* Resource = m_pResource.Get();

    // Create the render target view
    Device->CreateRenderTargetView(Resource, &RTVDesc, m_RTVHandle);

    // Create the shader resource view
    Device->CreateShaderResourceView(Resource, &SRVDesc, m_SRVHandle);
}


void Texture3D::Create(const std::wstring& Name, uint32_t Width, uint32_t Height, uint32_t Depth,
    DXGI_FORMAT Format, D3D12_GPU_VIRTUAL_ADDRESS VidMem)
{
    D3D12_RESOURCE_FLAGS Flags = CombineResourceFlags();
    D3D12_RESOURCE_DESC ResourceDesc = DescribeTex3D(Width, Height, Depth, 1, Format, Flags);

    ResourceDesc.SampleDesc.Count = m_FragmentCount;
    ResourceDesc.SampleDesc.Quality = 0;

    D3D12_CLEAR_VALUE ClearValue = {};
    ClearValue.Format = Format;
    ClearValue.Color[0] = m_ClearColor.R();
    ClearValue.Color[1] = m_ClearColor.G();
    ClearValue.Color[2] = m_ClearColor.B();
    ClearValue.Color[3] = m_ClearColor.A();

    CreateTextureResource(Graphics::g_Device, Name, ResourceDesc, ClearValue, VidMem);
    CreateDerivedViews(Graphics::g_Device, Format, Depth, 1);
}


D3D12_RESOURCE_DESC Texture3D::DescribeTex3D(uint32_t Width, uint32_t Height, uint32_t DepthOrArraySize,
    uint32_t NumMips, DXGI_FORMAT Format, UINT Flags)
{
    m_Width = Width;
    m_Height = Height;
    m_ArraySize = DepthOrArraySize;
    m_Format = Format;

    D3D12_RESOURCE_DESC Desc = {};
    Desc.Alignment = 0;
    Desc.DepthOrArraySize = (UINT16)DepthOrArraySize;
    Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
    Desc.Flags = (D3D12_RESOURCE_FLAGS)Flags;
    Desc.Format = GetBaseFormat(Format);
    Desc.Height = (UINT)Height;
    Desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    Desc.MipLevels = (UINT16)NumMips;
    Desc.SampleDesc.Count = 1;
    Desc.SampleDesc.Quality = 0;
    Desc.Width = (UINT64)Width;
    return Desc;
}