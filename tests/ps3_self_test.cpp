#include "core/crypto/aes.h"
#include "core/loader/ps3_self.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

void WriteU16BE(std::vector<std::uint8_t>& bytes,
                std::size_t offset,
                std::uint16_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value >> 8);
    bytes[offset + 1] = static_cast<std::uint8_t>(value);
}

void WriteU32BE(std::vector<std::uint8_t>& bytes,
                std::size_t offset,
                std::uint32_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value >> 24);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 16);
    bytes[offset + 2] = static_cast<std::uint8_t>(value >> 8);
    bytes[offset + 3] = static_cast<std::uint8_t>(value);
}

void WriteU64BE(std::vector<std::uint8_t>& bytes,
                std::size_t offset,
                std::uint64_t value) {
    for (std::size_t index = 0; index < 8; ++index) {
        bytes[offset + index] = static_cast<std::uint8_t>(
            value >> ((7 - index) * 8));
    }
}

std::vector<std::uint8_t> BuildSelf() {
    constexpr std::size_t kInfoOffset = 0x410;
    constexpr std::size_t kMetadataOffset = 0x450;
    constexpr std::size_t kDataOffset = 0x900;
    std::vector<std::uint8_t> bytes(kDataOffset + 4, 0);

    WriteU32BE(bytes, 0x00, vshift::loader::kPs3SceMagic);
    WriteU32BE(bytes, 0x04, 2);
    WriteU16BE(bytes, 0x08, vshift::loader::kPs3SceDebugFlag);
    WriteU16BE(bytes, 0x0a, 1);
    WriteU32BE(bytes, 0x0c, 0x3f0);
    WriteU64BE(bytes, 0x10, 0x900);
    WriteU64BE(bytes, 0x18, 0x1000);

    WriteU64BE(bytes, 0x20, 1);
    WriteU64BE(bytes, 0x28, 0x70);
    WriteU64BE(bytes, 0x30, 0x90);
    WriteU64BE(bytes, 0x38, 0xd0);
    WriteU64BE(bytes, 0x40, 0);
    WriteU64BE(bytes, 0x48, 0x290);

    WriteU64BE(bytes, 0x70, 0x107000005ff00001ull);
    WriteU32BE(bytes, 0x78, 1);
    WriteU32BE(bytes, 0x7c, 4);
    WriteU64BE(bytes, 0x80, 0x0004004600000000ull);

    WriteU32BE(bytes, 0x90, 0x7f454c46);
    bytes[0x94] = 2;
    bytes[0x95] = 2;
    bytes[0x96] = 1;
    bytes[0x97] = 102;
    WriteU16BE(bytes, 0xa0, 2);
    WriteU16BE(bytes, 0xa2, 0x15);
    WriteU32BE(bytes, 0xa4, 1);
    WriteU64BE(bytes, 0xa8, 0x1000);
    WriteU64BE(bytes, 0xb0, 0x40);
    WriteU16BE(bytes, 0xc4, 0x40);
    WriteU16BE(bytes, 0xc6, 0x38);
    WriteU16BE(bytes, 0xc8, 1);

    WriteU32BE(bytes, 0xd0, 1);
    WriteU32BE(bytes, 0xd4, 5);
    WriteU64BE(bytes, 0xd8, 0);
    WriteU64BE(bytes, 0xe0, 0x1000);
    WriteU64BE(bytes, 0xe8, 0x1000);
    WriteU64BE(bytes, 0xf0, 4);
    WriteU64BE(bytes, 0xf8, 8);
    WriteU64BE(bytes, 0x100, 0x1000);

    WriteU64BE(bytes, 0x290, kDataOffset);
    WriteU64BE(bytes, 0x298, 4);
    WriteU32BE(bytes, 0x2a0, 1);
    WriteU64BE(bytes, 0x2a8, 1);

    const std::array<std::uint8_t, 16> info_key{
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f};
    const std::array<std::uint8_t, 16> info_iv{
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
        0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f};
    std::copy(info_key.begin(), info_key.end(), bytes.begin() + kInfoOffset);
    std::copy(info_iv.begin(), info_iv.end(), bytes.begin() + kInfoOffset + 0x20);

    std::vector<std::uint8_t> metadata(0x4b0, 0);
    WriteU32BE(metadata, 0x0c, 1);
    WriteU32BE(metadata, 0x10, 0);
    WriteU64BE(metadata, 0x20, kDataOffset);
    WriteU64BE(metadata, 0x28, 4);
    WriteU32BE(metadata, 0x30, 2);
    WriteU32BE(metadata, 0x34, 0);
    WriteU32BE(metadata, 0x4c, 1);
    std::vector<std::uint8_t> encrypted_metadata;
    const auto crypto = vshift::crypto::AesCtrCrypt(
        info_key, info_iv, metadata, encrypted_metadata);
    if (!crypto.ok()) {
        std::cerr << crypto.error << '\n';
        return {};
    }
    std::copy(encrypted_metadata.begin(), encrypted_metadata.end(),
              bytes.begin() + kMetadataOffset);
    bytes[kDataOffset + 0] = 0x60;
    bytes[kDataOffset + 1] = 0x00;
    bytes[kDataOffset + 2] = 0x00;
    bytes[kDataOffset + 3] = 0x01;
    return bytes;
}

} // namespace

int main() {
    const auto bytes = BuildSelf();
    if (bytes.empty()) {
        return 1;
    }
    const auto parsed = vshift::loader::ParsePs3Self(bytes);
    if (!parsed.ok()) {
        std::cerr << parsed.error << '\n';
        return 1;
    }
    assert(parsed.ok());
    assert(parsed.image.elf_class == 2);
    assert(parsed.image.elf_machine == 0x15);
    assert(parsed.image.entry_point == 0x1000);
    assert(parsed.image.sections.size() == 1);
    assert(parsed.image.sections[0].bytes.size() == 4);

    vshift::memory::GuestMemory memory;
    const auto loaded = vshift::loader::LoadPs3SelfIntoMemory(parsed.image, memory);
    assert(loaded.ok());
    assert(loaded.loaded_segments == 1);
    std::array<std::uint8_t, 4> code{};
    assert(memory.Read(0x1000, code).ok());
    assert(code[0] == 0x60 && code[3] == 0x01);
    return 0;
}
