#pragma once

#include "core/loader/elf.h"
#include "core/memory/guest_memory.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace vshift::loader {

struct ElfLoadResult final {
    std::uint64_t entry = 0;
    std::vector<memory::Mapping> mappings;
    std::string error;

    bool ok() const noexcept { return error.empty(); }
};

// Maps validated PT_LOAD segments into guest memory and initializes their
// file-backed prefixes. The backing source is caller-owned and may be a
// bounded synthetic ELF fixture or a read-only component range.
ElfLoadResult MapElfLoadSegments(const ElfParseResult& parsed,
                                 std::span<const std::uint8_t> file_bytes,
                                 memory::GuestMemory& memory);

} // namespace vshift::loader
