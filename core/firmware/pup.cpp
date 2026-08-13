#include "core/firmware/pup.h"

namespace vshift::firmware {

namespace {

std::uint16_t ReadU16LE(const std::uint8_t* bytes) noexcept {
    return static_cast<std::uint16_t>(bytes[0]) |
           (static_cast<std::uint16_t>(bytes[1]) << 8);
}

std::uint32_t ReadU32LE(const std::uint8_t* bytes) noexcept {
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8) |
           (static_cast<std::uint32_t>(bytes[2]) << 16) |
           (static_cast<std::uint32_t>(bytes[3]) << 24);
}

} // namespace

PupFragmentParseResult ParsePupFragmentHeader(
    std::span<const std::uint8_t> header_bytes) {
    PupFragmentParseResult result;
    if (header_bytes.size() < kPupFragmentPublicHeaderSize) {
        result.error = "PUP fragment public header is truncated";
        return result;
    }

    const auto* bytes = header_bytes.data();
    result.header.magic = ReadU32LE(bytes + 0x00);
    if (result.header.magic != kPs5PupFragmentMagic) {
        result.error = "PUP fragment is not a PS5 SELF/PUP payload";
        return result;
    }

    result.header.version = bytes[0x04];
    result.header.mode = bytes[0x05];
    result.header.endian = bytes[0x06];
    result.header.attributes = bytes[0x07];
    result.header.key_type = ReadU32LE(bytes + 0x08);
    result.header.header_size = ReadU16LE(bytes + 0x0C);
    result.header.metadata_size = ReadU16LE(bytes + 0x0E);

    if (result.header.header_size < 0x20) {
        result.error = "PUP fragment header size is invalid";
    }
    return result;
}

} // namespace vshift::firmware
