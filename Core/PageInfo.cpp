#include "pch.h"
#include "PageInfo.h"
#include "CommandContext.h"


PageInfo::PageInfo(const PageInfo & page) :
    start_corordinate(page.start_corordinate),
    regionSize(page.regionSize),
    mipLevel(page.mipLevel),
    m_mem(page.m_mem ? std::make_unique<PageAlloc>(*page.m_mem) : nullptr),
    is_packed(page.is_packed)
{}

void PageInfo::LoadData(std::vector<UINT8> data, PageAllocator& allocator)
{
    CommandContext& InitContext = CommandContext::Begin();
    const size_t NumBytes = data.size() * sizeof(UINT8);
    m_mem = std::make_unique<PageAlloc>(allocator.Allocate(NumBytes));
    SIMDMemCopy(m_mem->DataPtr, data.data(), Math::DivideByMultiple(NumBytes, 16));
    InitContext.Finish(true);
}