#include "core/loader/ps3_sce.h"

#include <cassert>
#include <cstdint>
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
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        bytes[offset + index] = static_cast<std::uint8_t>(
            value >> ((sizeof(value) - index - 1) * 8));
    }
}

std::vector<std::uint8_t> BuildHeader() {
    std::vector<std::uint8_t> bytes(vshift::loader::kPs3SceHeaderSize, 0);
    WriteU32BE(bytes, 0x00, vshift::loader::kPs3SceMagic);
    WriteU32BE(bytes, 0x04, 2);
    WriteU16BE(bytes, 0x08, 1);
    WriteU16BE(bytes, 0x0a, 1);
    WriteU32BE(bytes, 0x0c, 0x3a0);
    WriteU64BE(bytes, 0x10, 0x880);
    WriteU64BE(bytes, 0x18, 0xb82460);
    return bytes;
}

} // namespace

int main() {
    const auto header = BuildHeader();
    const auto parsed = vshift::loader::ParsePs3SceHeader(header, 0x568050);
    assert(parsed.ok());
    assert(parsed.header.header_version == 2);
    assert(parsed.header.flags == 1);
    assert(parsed.header.type == 1);
    assert(parsed.header.metadata_offset == 0x3a0);
    assert(parsed.header.header_size == 0x880);
    assert(parsed.header.payload_size == 0xb82460);
    assert(parsed.header.metadata_encrypted());

    auto plaintext = header;
    WriteU16BE(plaintext, 0x08, vshift::loader::kPs3SceDebugFlag);
    assert(!vshift::loader::ParsePs3SceHeader(plaintext, 0x568050)
                .header.metadata_encrypted());

    auto invalid = header;
    invalid[0] = 'X';
    assert(!vshift::loader::ParsePs3SceHeader(invalid, 0x568050).ok());

    invalid = header;
    WriteU64BE(invalid, 0x10, 0x568051);
    assert(!vshift::loader::ParsePs3SceHeader(invalid, 0x568050).ok());

    invalid = header;
    WriteU32BE(invalid, 0x0c, 0x881);
    assert(!vshift::loader::ParsePs3SceHeader(invalid, 0x568050).ok());
    return 0;
}
