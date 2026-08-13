#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace vshift::firmware {

// The first 0x10 bytes of a raw PS5 PUP fragment are public. The remaining
// 0x10 bytes of the nominal header are encrypted in the downloaded package.
constexpr std::size_t kPupFragmentPublicHeaderSize = 0x10;
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

} // namespace vshift::firmware
