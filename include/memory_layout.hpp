#ifndef LANES_MEMORY_LAYOUT_HPP
#define LANES_MEMORY_LAYOUT_HPP

#include <cstdint>

// assigns every field a fixed address in linear memory at compile time

class MemoryLayout
{
public:
    // every block starts on this boundary (the size of one v128)
    static constexpr std::uint32_t ALIGNMENT = 16;

    // WASM memory is measured in 64kb pages
    static constexpr std::uint64_t PAGE_SIZE = 65536;

    // a 32-bit WASM memory can hold at most 65536 pages (4gb)
    static constexpr std::uint64_t MAX_PAGES = 65536;

    // reserves `bytes` bytes and writes the block's address to `address`
    // returns false if the block would not fit in 4gb
    bool reserve(std::uint64_t bytes, std::uint32_t &address);

    // bytes from address 0 to the end of the last block
    std::uint64_t size() const;

    // pages the module needs to hold every block
    std::uint32_t pages() const;

    // true if no bytes have been reserved, so the module needs no memory
    bool empty() const;

private:
    std::uint64_t end_ = 0;
};

#endif // LANES_MEMORY_LAYOUT_HPP
