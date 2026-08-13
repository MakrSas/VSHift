#include "core/firmware/pup.h"

#include <limits>

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

std::uint64_t ReadU64LE(const std::uint8_t* bytes) noexcept {
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        value |= static_cast<std::uint64_t>(bytes[index]) << (index * 8);
    }
    return value;
}

bool AddWouldOverflow(std::uint64_t left,
                      std::uint64_t right) noexcept {
    return right > std::numeric_limits<std::uint64_t>::max() - left;
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

DecryptedPupParseResult ParseDecryptedPupHeaders(
    std::span<const std::uint8_t> header_bytes,
    std::uint64_t actual_file_size) {
    DecryptedPupParseResult result;
    if (header_bytes.size() < kPupFixedHeaderSize) {
        result.error = "decrypted PUP fixed header is truncated";
        return result;
    }

    const auto public_header = ParsePupFragmentHeader(
        header_bytes.first(kPupFragmentPublicHeaderSize));
    if (!public_header.ok()) {
        result.error = public_header.error;
        return result;
    }
    result.header.public_header = public_header.header;

    const auto* bytes = header_bytes.data();
    result.header.file_size = ReadU64LE(bytes + 0x10);
    result.header.segment_count = ReadU16LE(bytes + 0x18);
    result.header.flags = ReadU16LE(bytes + 0x1A);
    result.header.reserved = ReadU32LE(bytes + 0x1C);

    if (result.header.file_size != actual_file_size) {
        result.error = "PUP file-size field does not match the selected file";
        return result;
    }
    if (result.header.file_size < kPupFixedHeaderSize ||
        result.header.public_header.header_size < kPupFixedHeaderSize) {
        result.error = "decrypted PUP header size is invalid";
        return result;
    }

    constexpr std::uint16_t kMaximumSegments = 4096;
    if (result.header.segment_count > kMaximumSegments) {
        result.error = "decrypted PUP segment count is unreasonable";
        return result;
    }

    const auto segment_table_size =
        static_cast<std::uint64_t>(result.header.segment_count) *
        kPupSegmentEntrySize;
    if (AddWouldOverflow(kPupFixedHeaderSize, segment_table_size)) {
        result.error = "decrypted PUP segment table overflows";
        return result;
    }
    const auto segment_table_end =
        kPupFixedHeaderSize + segment_table_size;
    if (segment_table_end > result.header.public_header.header_size ||
        segment_table_end > header_bytes.size()) {
        result.error = "decrypted PUP segment table is truncated";
        return result;
    }

    if (AddWouldOverflow(result.header.public_header.header_size,
                         result.header.public_header.metadata_size)) {
        result.error = "decrypted PUP metadata range overflows";
        return result;
    }
    const auto metadata_end =
        static_cast<std::uint64_t>(result.header.public_header.header_size) +
        result.header.public_header.metadata_size;
    if (metadata_end > result.header.file_size ||
        result.header.public_header.header_size > header_bytes.size()) {
        result.error = "decrypted PUP metadata range is unavailable";
        return result;
    }

    // The PUP information block follows the segment table. Its first field is
    // the firmware version; the remaining fields are intentionally opaque.
    const auto info_offset = static_cast<std::size_t>(segment_table_end);
    if (info_offset + sizeof(std::uint32_t) <= header_bytes.size()) {
        result.header.firmware_version = ReadU32LE(bytes + info_offset);
    }

    result.segments.reserve(result.header.segment_count);
    for (std::uint16_t index = 0;
         index < result.header.segment_count;
         ++index) {
        const auto entry_offset = static_cast<std::size_t>(
            kPupFixedHeaderSize +
            static_cast<std::uint64_t>(index) * kPupSegmentEntrySize);
        const auto* entry = bytes + entry_offset;
        PupSegmentHeader segment;
        segment.flags = ReadU64LE(entry + 0x00);
        segment.offset = ReadU64LE(entry + 0x08);
        segment.compressed_size = ReadU64LE(entry + 0x10);
        segment.uncompressed_size = ReadU64LE(entry + 0x18);

        if (AddWouldOverflow(segment.offset, segment.compressed_size) ||
            segment.offset + segment.compressed_size > result.header.file_size) {
            result.error = "decrypted PUP segment range is outside the file";
            result.segments.clear();
            return result;
        }
        result.segments.push_back(segment);
    }

    return result;
}

} // namespace vshift::firmware
