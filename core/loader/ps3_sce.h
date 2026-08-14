#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace vshift::loader {

// PS3 SCE/SELF headers use big-endian scalar fields. This is the small
// portable boundary shared by firmware packages and executable SELF files.
constexpr std::size_t kPs3SceHeaderSize = 0x20;
constexpr std::uint32_t kPs3SceMagic = 0x53434500;
constexpr std::uint16_t kPs3SceDebugFlag = 0x8000;

struct Ps3SceHeader final {
    std::uint32_t magic = 0;
    std::uint32_t header_version = 0;
    std::uint16_t flags = 0;
    std::uint16_t type = 0;
    std::uint32_t metadata_offset = 0;
    std::uint64_t header_size = 0;
    std::uint64_t payload_size = 0;

    // RPCS3 decrypts the metadata info when the debug/plaintext bit is not
    // set. This reports the state; it does not attempt key recovery.
    bool metadata_encrypted() const noexcept {
        return (flags & kPs3SceDebugFlag) == 0;
    }
};

struct Ps3SceParseResult final {
    Ps3SceHeader header;
    std::string error;

    bool ok() const noexcept { return error.empty(); }
};

// Parses only the fixed 0x20-byte SCE header. The caller supplies the size of
// the selected PUP entry/SELF so the header range can be checked without
// reading or decrypting the payload.
Ps3SceParseResult ParsePs3SceHeader(
    std::span<const std::uint8_t> header_bytes,
    std::uint64_t actual_file_size);

} // namespace vshift::loader
