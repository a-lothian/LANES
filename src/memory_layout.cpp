#include "memory_layout.hpp"

bool MemoryLayout::reserve(std::uint64_t bytes, std::uint32_t &address)
{
    // round the start up to the next 16 byte boundary
    std::uint64_t start = (end_ + ALIGNMENT - 1) / ALIGNMENT * ALIGNMENT;

    // 64-bit math so this cannot overflow for any field the parser can express
    if (start + bytes > MAX_PAGES * PAGE_SIZE)
    {
        return false;
    }

    address = static_cast<std::uint32_t>(start);
    end_ = start + bytes;
    return true;
}

std::uint64_t MemoryLayout::size() const
{
    return end_;
}

std::uint32_t MemoryLayout::pages() const
{
    // round up to whole pages
    return static_cast<std::uint32_t>((end_ + PAGE_SIZE - 1) / PAGE_SIZE);
}

bool MemoryLayout::empty() const
{
    return end_ == 0;
}
