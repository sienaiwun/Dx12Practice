#pragma once

#pragma  region HEADER
#include "pch.h"
#include "GpuBuffer.h"
#include "LinearAllocator.h"
#pragma region 


struct PageInfo
{
public:
    PageInfo() :m_mem(nullptr) {}
    
    PageInfo(const PageInfo &);
    
    ~PageInfo() = default;

    void LoadData(std::vector<UINT8> data, PageAllocator& allocator);

    D3D12_TILED_RESOURCE_COORDINATE start_corordinate;
    
    D3D12_TILE_REGION_SIZE regionSize;
    
    U32 mipLevel;
    
    std::unique_ptr<PageAlloc> m_mem = nullptr;
    
    bool is_packed = false;

};
