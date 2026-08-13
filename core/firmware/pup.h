#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace vshift::firmware {

// The first 0x10 bytes of a raw PS5 PUP fragment are public. A decrypted PUP
// exposes the remaining fixed header, segment table, and metadata offsets.
constexpr std::size_t kPupFragmentPublicHeaderSize = 0x10;
constexpr std::size_t kPupFixedHeaderSize = 0x20;
constexpr std::size_t kPupSegmentEntrySize = 0x20;
constexpr std::uint32_t kPs5PupFragmentMagic = 0xEEF51454;

struct PupFragmentHeader final {
    std::uint32_t magic = 0;
    std::uint8_t version = 0;
    std::uint8_t mode = 0;
    std::uint8_t endian = 0;
    std::uint8_t attributes = 0;
    std::uint32_t key_type = 0;
    std::uint16_t header_size = 0;
    std::uint16_t metadata_size = 0;
};

struct PupFragmentParseResult final {
    PupFragmentHeader header;
    std::string error;

    bool ok() const noexcept { return error.empty(); }
};

// Parses only the unencrypted public prefix of a PS5 PUP fragment. It never
// attempts to decrypt the encrypted header tail or any segment metadata.
PupFragmentParseResult ParsePupFragmentHeader(
    std::span<const std::uint8_t> header_bytes);

struct PupSegmentHeader final {
    std::uint64_t flags = 0;
    std::uint64_t offset = 0;
    std::uint64_t compressed_size = 0;
    std::uint64_t uncompressed_size = 0;
};

struct DecryptedPupHeader final {
    PupFragmentHeader public_header;
    std::uint64_t file_size = 0;
    std::uint16_t segment_count = 0;
    std::uint16_t flags = 0;
    std::uint32_t reserved = 0;
    std::uint32_t firmware_version = 0;
};

struct DecryptedPupParseResult final {
    DecryptedPupHeader header;
    std::vector<PupSegmentHeader> segments;
    std::string error;

    bool ok() const noexcept { return error.empty(); }
};

// Parses the fixed header and segment table of a decrypted PS5 PUP. The
// caller supplies only a bounded prefix (normally header_size bytes); no
// metadata keys or segment payload bytes are interpreted.
DecryptedPupParseResult ParseDecryptedPupHeaders(
    std::span<const std::uint8_t> header_bytes,
    std::uint64_t actual_file_size);

} // namespace vshift::firmware
