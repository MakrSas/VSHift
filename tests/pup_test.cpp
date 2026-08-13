#include "core/firmware/pup.h"

#include <cassert>
#include <cstdint>
#include <vector>

namespace {

void WriteU16LE(std::vector<std::uint8_t>& bytes,
                std::size_t offset,
                std::uint16_t value) {
    bytes[offset + 0] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void WriteU32LE(std::vector<std::uint8_t>& bytes,
                std::size_t offset,
                std::uint32_t value) {
    bytes[offset + 0] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
    bytes[offset + 2] = static_cast<std::uint8_t>(value >> 16);
    bytes[offset + 3] = static_cast<std::uint8_t>(value >> 24);
}

} // namespace

int main() {
    std::vector<std::uint8_t> header(
        vshift::firmware::kPupFragmentPublicHeaderSize);
    WriteU32LE(header, 0x00, vshift::firmware::kPs5PupFragmentMagic);
    header[0x04] = 0x10;
    header[0x05] = 0x01;
    header[0x06] = 0x01;
    header[0x07] = 0x32;
    WriteU32LE(header, 0x08, 4);
    WriteU16LE(header, 0x0C, 0x240);
    WriteU16LE(header, 0x0E, 0x5A0);

    const auto parsed = vshift::firmware::ParsePupFragmentHeader(header);
    assert(parsed.ok());
    assert(parsed.header.version == 0x10);
    assert(parsed.header.mode == 0x01);
    assert(parsed.header.endian == 0x01);
    assert(parsed.header.attributes == 0x32);
    assert(parsed.header.key_type == 4);
    assert(parsed.header.header_size == 0x240);
    assert(parsed.header.metadata_size == 0x5A0);

    header.resize(3);
    assert(!vshift::firmware::ParsePupFragmentHeader(header).ok());

    header.assign(vshift::firmware::kPupFragmentPublicHeaderSize, 0);
    WriteU32LE(header, 0x00, 0x12345678);
    assert(!vshift::firmware::ParsePupFragmentHeader(header).ok());

    return 0;
}
