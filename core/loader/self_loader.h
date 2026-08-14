#pragma once

#include "core/loader/self.h"
#include "core/memory/guest_memory.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace vshift::loader {

struct SelfLoadResult final {
    std::uint64_t entry = 0;
    std::vector<memory::Mapping> mappings;
    std::string error;

    bool ok() const noexcept { return error.empty(); }
};

// Maps PS4/PS5 SELF entries that are already plain, uncompressed payloads.
// Encrypted, compressed, and blocked entries are rejected explicitly; this
// loader never attempts to recover keys or transform protected payloads.
SelfLoadResult MapSelfLoadSegments(
    const SelfParseResult& parsed,
    std::span<const std::uint8_t> self_bytes,
    memory::GuestMemory& memory);

} // namespace vshift::loader
