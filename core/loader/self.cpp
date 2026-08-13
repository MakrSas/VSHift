#include "core/loader/self.h"

#include <algorithm>
#include <limits>
#include <vector>

namespace vshift::loader {

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

SelfParseResult ParsePs5SelfHeaders(
    std::span<const std::uint8_t> header_bytes,
    std::uint64_t actual_file_size) {
    SelfParseResult result;
    if (header_bytes.size() < kSelfHeaderSize) {
        result.error = "PS5 SELF header is truncated";
        return result;
    }

    const auto* bytes = header_bytes.data();
    result.header.magic = ReadU32LE(bytes + 0x00);
    if (result.header.magic != kPs5SelfMagic) {
        result.error = "payload is not a PS5 SELF";
        return result;
    }
    result.header.version = bytes[0x04];
    result.header.mode = bytes[0x05];
    result.header.endian = bytes[0x06];
    result.header.attributes = bytes[0x07];
    result.header.key_type = ReadU32LE(bytes + 0x08);
    result.header.header_size = ReadU16LE(bytes + 0x0C);
    result.header.metadata_size = ReadU16LE(bytes + 0x0E);
    result.header.file_size = ReadU64LE(bytes + 0x10);
    result.header.entry_count = ReadU16LE(bytes + 0x18);
    result.header.flags = ReadU16LE(bytes + 0x1A);

    if (result.header.file_size != actual_file_size ||
        result.header.file_size < kSelfHeaderSize ||
        result.header.header_size < kSelfHeaderSize ||
        result.header.header_size > result.header.file_size) {
        result.error = "PS5 SELF size fields are invalid";
        return result;
    }

    constexpr std::uint16_t kMaximumEntries = 4096;
    if (result.header.entry_count > kMaximumEntries) {
        result.error = "PS5 SELF entry count is unreasonable";
        return result;
    }

    const auto table_size =
        static_cast<std::uint64_t>(result.header.entry_count) *
        kSelfEntrySize;
    if (AddWouldOverflow(kSelfHeaderSize, table_size)) {
        result.error = "PS5 SELF entry table overflows";
        return result;
    }
    const auto table_end = static_cast<std::uint64_t>(kSelfHeaderSize) +
                           table_size;
    if (table_end > result.header.header_size ||
        table_end > header_bytes.size()) {
        result.error = "PS5 SELF entry table is truncated";
        return result;
    }

    if (AddWouldOverflow(result.header.header_size,
                         result.header.metadata_size) ||
        static_cast<std::uint64_t>(result.header.header_size) +
                result.header.metadata_size > result.header.file_size) {
        result.error = "PS5 SELF metadata range is invalid";
        return result;
    }

    result.entries.reserve(result.header.entry_count);
    for (std::uint16_t index = 0;
         index < result.header.entry_count;
         ++index) {
        const auto entry_offset = static_cast<std::size_t>(
            kSelfHeaderSize + static_cast<std::uint64_t>(index) *
                                 kSelfEntrySize);
        const auto* entry_bytes = bytes + entry_offset;
        SelfEntry entry;
        entry.flags = ReadU64LE(entry_bytes + 0x00);
        entry.offset = ReadU64LE(entry_bytes + 0x08);
        entry.compressed_size = ReadU64LE(entry_bytes + 0x10);
        entry.uncompressed_size = ReadU64LE(entry_bytes + 0x18);
        if (entry.compressed_size > entry.uncompressed_size ||
            AddWouldOverflow(entry.offset, entry.compressed_size) ||
            entry.offset + entry.compressed_size > result.header.file_size) {
            result.error = "PS5 SELF entry range is invalid";
            result.entries.clear();
            return result;
        }
        result.entries.push_back(entry);
    }

    // The embedded ELF header is in the bounded SELF header area. Search only
    // that area; a payload magic later in encrypted data is not a header.
    const auto header_end = static_cast<std::size_t>(
        result.header.header_size);
    for (std::size_t offset = kSelfHeaderSize;
         offset + kElf64HeaderSize <= header_end &&
         offset + kElf64HeaderSize <= header_bytes.size();
         ++offset) {
        if (bytes[offset + 0] != 0x7f || bytes[offset + 1] != 'E' ||
            bytes[offset + 2] != 'L' || bytes[offset + 3] != 'F') {
            continue;
        }

        const auto elf = ParseElf64Headers(
            header_bytes.subspan(offset),
            std::numeric_limits<std::uint64_t>::max());
        if (!elf.ok()) {
            result.error = "embedded ELF header failed: " + elf.error;
            return result;
        }
        result.elf.offset = offset;
        result.elf.header = elf.header;
        result.elf.program_headers = elf.program_headers;
        return result;
    }

    result.error = "PS5 SELF has no embedded ELF header in its header area";
    return result;
}

std::vector<SelfLoadMapping> MatchSelfLoadEntries(
    const SelfParseResult& self) {
    std::vector<SelfLoadMapping> mappings;
    if (!self.ok() || !self.has_elf()) {
        return mappings;
    }

    std::vector<bool> used_entries(self.entries.size(), false);
    for (std::size_t program_index = 0;
         program_index < self.elf.program_headers.size();
         ++program_index) {
        const auto& program = self.elf.program_headers[program_index];
        if (program.type != kElfProgramLoad || program.file_size == 0) {
            continue;
        }

        const auto entry = std::find_if(
            self.entries.begin(), self.entries.end(),
            [&](const SelfEntry& candidate) {
                const auto candidate_index = static_cast<std::size_t>(
                    &candidate - self.entries.data());
                if (used_entries[candidate_index]) {
                    return false;
                }
                return candidate.compressed_size == program.file_size ||
                       candidate.uncompressed_size == program.file_size;
            });
        if (entry == self.entries.end()) {
            continue;
        }

        const auto entry_index = static_cast<std::size_t>(
            entry - self.entries.begin());
        used_entries[entry_index] = true;
        mappings.push_back(SelfLoadMapping{
            program_index,
            entry_index,
            entry->offset,
            program.virtual_address,
            program.file_size,
            program.memory_size,
        });
    }
    return mappings;
}

} // namespace vshift::loader
