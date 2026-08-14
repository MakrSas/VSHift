#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace vshift::firmware {

struct Ps3TarEntry final {
    std::string name;
    std::uint64_t data_offset = 0;
    std::uint64_t data_length = 0;
    bool regular_file = false;
};

struct Ps3TarParseResult final {
    std::vector<Ps3TarEntry> entries;
    std::string error;

    bool ok() const noexcept { return error.empty(); }
};

// Parses only the bounded POSIX TAR headers. Entry contents remain owned by
// the caller, which lets the PS3 firmware path scan the outer PUP TAR without
// copying its 180+ MiB payload.
Ps3TarParseResult ParsePs3Tar(std::span<const std::uint8_t> bytes);

} // namespace vshift::firmware
