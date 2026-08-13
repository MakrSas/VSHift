#include "core/firmware/slb2.h"

#include <algorithm>
#include <limits>

namespace vshift::firmware {

namespace {

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

Slb2ParseResult ParseSlb2Table(std::span<const std::uint8_t> table_bytes,
                               std::uint64_t container_size) {
    Slb2ParseResult result;
    if (table_bytes.size() < kSlb2HeaderSize) {
        result.error = "SLB2 header is truncated";
        return result;
    }

    if (table_bytes[0] != 'S' || table_bytes[1] != 'L' ||
        table_bytes[2] != 'B' || table_bytes[3] != '2') {
        result.error = "firmware container does not start with SLB2";
        return result;
    }

    const auto version = ReadU32LE(table_bytes.data() + 0x04);
    const auto flags = ReadU32LE(table_bytes.data() + 0x08);
    const auto entry_count = ReadU32LE(table_bytes.data() + 0x0C);
    const auto size_in_sectors = ReadU32LE(table_bytes.data() + 0x10);

    constexpr std::uint32_t kMaximumEntries = 1'000'000;
    if (entry_count > kMaximumEntries) {
        result.error = "SLB2 entry count is unreasonably large";
        return result;
    }

    const auto table_size = kSlb2HeaderSize +
                            static_cast<std::size_t>(entry_count) *
                                kSlb2EntrySize;
    if (table_size > table_bytes.size()) {
        result.error = "SLB2 file table is truncated";
        return result;
    }

    result.package.version = version;
    result.package.flags = flags;
    result.package.entry_count = entry_count;
    result.package.size_in_sectors = size_in_sectors;
    result.package.entries.reserve(entry_count);

    for (std::uint32_t index = 0; index < entry_count; ++index) {
        const auto* entry = table_bytes.data() + kSlb2HeaderSize +
                            static_cast<std::size_t>(index) * kSlb2EntrySize;
        const auto start_sector = ReadU32LE(entry + 0x00);
        const auto file_size = ReadU32LE(entry + 0x04);
        const auto offset = static_cast<std::uint64_t>(start_sector) *
                            kSlb2SectorSize;

        if (AddWouldOverflow(offset, file_size) ||
            offset + file_size > container_size) {
            result.error = "SLB2 entry points outside the container";
            result.package.entries.clear();
            return result;
        }

        const auto* name_begin = reinterpret_cast<const char*>(entry + 0x10);
        const auto* name_end = std::find(name_begin, name_begin + 0x20, '\0');
        if (name_end == name_begin + 0x20) {
            result.error = "SLB2 entry name is not NUL terminated";
            result.package.entries.clear();
            return result;
        }

        result.package.entries.push_back({
            offset,
            file_size,
            std::string(name_begin, name_end),
        });
    }

    return result;
}

} // namespace vshift::firmware
