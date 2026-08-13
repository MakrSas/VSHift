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

void WriteU64LE(std::vector<std::uint8_t>& bytes,
                std::size_t offset,
                std::uint64_t value) {
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        bytes[offset + index] =
            static_cast<std::uint8_t>(value >> (index * 8));
    }
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

    constexpr std::uint64_t kFileSize = 0x4000;
    constexpr std::uint16_t kSegmentCount = 2;
    std::vector<std::uint8_t> decrypted(0x5B0, 0);
    WriteU32LE(decrypted, 0x00, vshift::firmware::kPs5PupFragmentMagic);
    decrypted[0x04] = 0x10;
    decrypted[0x05] = 0x01;
    decrypted[0x06] = 0x01;
    decrypted[0x07] = 0x32;
    WriteU32LE(decrypted, 0x08, 4);
    WriteU16LE(decrypted, 0x0C, 0x5B0);
    WriteU16LE(decrypted, 0x0E, 0xDE0);
    WriteU64LE(decrypted, 0x10, kFileSize);
    WriteU16LE(decrypted, 0x18, kSegmentCount);
    WriteU16LE(decrypted, 0x1A, 0x32);
    WriteU32LE(decrypted, 0x440, 0x12020000);
    WriteU64LE(decrypted, 0x20, 0x0000000000F02000);
    WriteU64LE(decrypted, 0x28, 0x1390);
    WriteU64LE(decrypted, 0x30, 0x1000);
    WriteU64LE(decrypted, 0x38, 0x1000);
    WriteU64LE(decrypted, 0x40, 0x0000000000230007);
    WriteU64LE(decrypted, 0x48, 0x2390);
    WriteU64LE(decrypted, 0x50, 0xA0);
    WriteU64LE(decrypted, 0x58, 0xA0);

    const auto parsed_decrypted =
        vshift::firmware::ParseDecryptedPupHeaders(decrypted, kFileSize);
    assert(parsed_decrypted.ok());
    assert(parsed_decrypted.header.file_size == kFileSize);
    assert(parsed_decrypted.header.segment_count == kSegmentCount);
    assert(parsed_decrypted.header.firmware_version == 0x12020000);
    assert(parsed_decrypted.segments.size() == kSegmentCount);
    assert(parsed_decrypted.segments[0].offset == 0x1390);
    assert(parsed_decrypted.segments[0].compressed_size == 0x1000);
    assert(parsed_decrypted.segments[1].offset == 0x2390);

    WriteU64LE(decrypted, 0x28, kFileSize);
    assert(!vshift::firmware::ParseDecryptedPupHeaders(
                     decrypted, kFileSize)
                 .ok());

    return 0;
}
