#include "core/firmware/ps3_pup.h"

#include <array>
#include <algorithm>
#include <limits>
#include <unordered_set>

namespace vshift::firmware {

namespace {

constexpr std::array<std::uint8_t, 8> kPs3PupMagic = {
    'S', 'C', 'E', 'U', 'F', 0, 0, 0};

std::uint64_t ReadU64BE(const std::uint8_t* bytes) noexcept {
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        value = (value << 8) | bytes[index];
    }
    return value;
}

bool AddWouldOverflow(std::uint64_t left,
                      std::uint64_t right) noexcept {
    return right > std::numeric_limits<std::uint64_t>::max() - left;
}

} // namespace

Ps3PupParseResult ParsePs3PupHeaders(
    std::span<const std::uint8_t> header_bytes,
    std::uint64_t actual_file_size) {
    Ps3PupParseResult result;
    if (header_bytes.size() < kPs3PupHeaderSize) {
        result.error = "PS3 PUP fixed header is truncated";
        return result;
    }
    if (!std::equal(kPs3PupMagic.begin(), kPs3PupMagic.end(),
                    header_bytes.begin())) {
        result.error = "input is not a PS3 PUP";
        return result;
    }

    const auto* bytes = header_bytes.data();
    result.header.package_version = ReadU64BE(bytes + 0x08);
    result.header.image_version = ReadU64BE(bytes + 0x10);
    result.header.file_count = ReadU64BE(bytes + 0x18);
    result.header.header_length = ReadU64BE(bytes + 0x20);
    result.header.data_length = ReadU64BE(bytes + 0x28);

    constexpr std::uint64_t kMaximumFileCount = 4096;
    if (result.header.file_count == 0 ||
        result.header.file_count > kMaximumFileCount) {
        result.error = "PS3 PUP file count is invalid";
        return result;
    }
    if (result.header.header_length < kPs3PupHeaderSize ||
        result.header.header_length > actual_file_size) {
        result.error = "PS3 PUP header range is invalid";
        return result;
    }
    if (result.header.data_length >
        actual_file_size - result.header.header_length) {
        result.error = "PS3 PUP data range is outside the file";
        return result;
    }
    if (result.header.header_length > header_bytes.size()) {
        result.error = "PS3 PUP header table is truncated";
        return result;
    }

    const auto table_count = result.header.file_count;
    const auto table_bytes =
        table_count * (kPs3PupFileEntrySize + kPs3PupHashEntrySize);
    if (AddWouldOverflow(kPs3PupHeaderSize, table_bytes)) {
        result.error = "PS3 PUP tables overflow";
        return result;
    }
    const auto table_end = kPs3PupHeaderSize + table_bytes;
    if (table_end > result.header.header_length ||
        table_end > header_bytes.size()) {
        result.error = "PS3 PUP file and hash tables are truncated";
        return result;
    }

    std::unordered_set<std::uint64_t> entry_ids;
    entry_ids.reserve(static_cast<std::size_t>(table_count));
    result.entries.reserve(static_cast<std::size_t>(table_count));
    for (std::uint64_t index = 0; index < table_count; ++index) {
        const auto entry_offset =
            kPs3PupHeaderSize + index * kPs3PupFileEntrySize;
        const auto* entry_bytes = header_bytes.data() + entry_offset;
        Ps3PupFileEntry entry;
        entry.entry_id = ReadU64BE(entry_bytes + 0x00);
        entry.data_offset = ReadU64BE(entry_bytes + 0x08);
        entry.data_length = ReadU64BE(entry_bytes + 0x10);

        if (!entry_ids.insert(entry.entry_id).second) {
            result.error = "PS3 PUP contains duplicate file entries";
            result.entries.clear();
            return result;
        }
        if (entry.data_offset > actual_file_size ||
            entry.data_length > actual_file_size - entry.data_offset) {
            result.error = "PS3 PUP file entry range is outside the file";
            result.entries.clear();
            return result;
        }
        result.entries.push_back(entry);
    }

    return result;
}

} // namespace vshift::firmware
