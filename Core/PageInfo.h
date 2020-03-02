#pragma once

#pragma  region HEADER
#include "pch.h"
#include "GpuBuffer.h"
#include "LinearAllocator.h"
#pragma region 


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
