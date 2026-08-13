#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace vshift::firmware {

constexpr std::size_t kSlb2HeaderSize = 0x30;
constexpr std::size_t kSlb2EntrySize = 0x30;
constexpr std::uint64_t kSlb2SectorSize = 0x200;

struct Slb2Entry final {
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
    std::string name;
};

struct Slb2Package final {
    std::uint32_t version = 0;
    std::uint32_t flags = 0;
    std::uint32_t entry_count = 0;
    std::uint32_t size_in_sectors = 0;
    std::vector<Slb2Entry> entries;
};

struct Slb2ParseResult final {
    Slb2Package package;
    std::string error;

    bool ok() const noexcept { return error.empty(); }
};

// Parses the SLB2 header and file table. `container_size` is the size of the
// complete user-provided PUP; `table_bytes` only needs to contain the header
// and entry table, so inspection never loads the whole firmware into memory.
Slb2ParseResult ParseSlb2Table(std::span<const std::uint8_t> table_bytes,
                               std::uint64_t container_size);

} // namespace vshift::firmware
