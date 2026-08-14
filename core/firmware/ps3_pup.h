#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace vshift::firmware {

// The PS3 PUP header and tables are big-endian. This parser intentionally
// stops at the metadata boundary; it does not verify HMACs or decrypt the
// package payloads.
constexpr std::size_t kPs3PupHeaderSize = 0x30;
constexpr std::size_t kPs3PupFileEntrySize = 0x20;
constexpr std::size_t kPs3PupHashEntrySize = 0x20;

struct Ps3PupHeader final {
    std::uint64_t package_version = 0;
    std::uint64_t image_version = 0;
    std::uint64_t file_count = 0;
    std::uint64_t header_length = 0;
    std::uint64_t data_length = 0;
};

struct Ps3PupFileEntry final {
    std::uint64_t entry_id = 0;
    std::uint64_t data_offset = 0;
    std::uint64_t data_length = 0;
};

struct Ps3PupParseResult final {
    Ps3PupHeader header;
    std::vector<Ps3PupFileEntry> entries;
    std::string error;

    bool ok() const noexcept { return error.empty(); }
};

// Parses the PS3 PUP fixed header and file table from a bounded prefix. The
// caller supplies the actual file size separately so every payload range can
// be checked without loading the firmware into memory.
Ps3PupParseResult ParsePs3PupHeaders(
    std::span<const std::uint8_t> header_bytes,
    std::uint64_t actual_file_size);

} // namespace vshift::firmware
