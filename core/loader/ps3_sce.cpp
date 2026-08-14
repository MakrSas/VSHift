#include "core/loader/ps3_sce.h"

namespace vshift::loader {

namespace {

std::uint16_t ReadU16BE(const std::uint8_t* bytes) noexcept {
    return static_cast<std::uint16_t>(bytes[0] << 8) |
           static_cast<std::uint16_t>(bytes[1]);
}

std::uint32_t ReadU32BE(const std::uint8_t* bytes) noexcept {
    return (static_cast<std::uint32_t>(bytes[0]) << 24) |
           (static_cast<std::uint32_t>(bytes[1]) << 16) |
           (static_cast<std::uint32_t>(bytes[2]) << 8) |
           static_cast<std::uint32_t>(bytes[3]);
}

std::uint64_t ReadU64BE(const std::uint8_t* bytes) noexcept {
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        value = (value << 8) | bytes[index];
    }
    return value;
}

} // namespace

Ps3SceParseResult ParsePs3SceHeader(
    std::span<const std::uint8_t> header_bytes,
    std::uint64_t actual_file_size) {
    Ps3SceParseResult result;
    if (header_bytes.size() < kPs3SceHeaderSize ||
        actual_file_size < kPs3SceHeaderSize) {
        result.error = "PS3 SCE header is truncated";
        return result;
    }

    const auto* bytes = header_bytes.data();
    result.header.magic = ReadU32BE(bytes + 0x00);
    if (result.header.magic != kPs3SceMagic) {
        result.error = "payload is not a PS3 SCE file";
        return result;
    }
    result.header.header_version = ReadU32BE(bytes + 0x04);
    result.header.flags = ReadU16BE(bytes + 0x08);
    result.header.type = ReadU16BE(bytes + 0x0a);
    result.header.metadata_offset = ReadU32BE(bytes + 0x0c);
    result.header.header_size = ReadU64BE(bytes + 0x10);
    result.header.payload_size = ReadU64BE(bytes + 0x18);

    if (result.header.header_version == 0 ||
        result.header.header_size < kPs3SceHeaderSize ||
        result.header.header_size > actual_file_size) {
        result.error = "PS3 SCE header size fields are invalid";
        return result;
    }
    if (result.header.metadata_offset > result.header.header_size) {
        result.error = "PS3 SCE metadata offset is outside the header";
        return result;
    }
    return result;
}

} // namespace vshift::loader
