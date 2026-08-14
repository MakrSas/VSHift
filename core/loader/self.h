#pragma once

#include "core/loader/elf.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace vshift::loader {

constexpr std::size_t kSelfHeaderSize = 0x20;
constexpr std::size_t kSelfEntrySize = 0x20;
constexpr std::uint32_t kPs4SelfMagic = 0x1D3D154F;
constexpr std::uint32_t kPs5SelfMagic = 0xEEF51454;
constexpr std::uint64_t kSelfSegmentOrdered = 1ull << 0;
constexpr std::uint64_t kSelfSegmentEncrypted = 1ull << 1;
constexpr std::uint64_t kSelfSegmentSigned = 1ull << 2;
constexpr std::uint64_t kSelfSegmentCompressed = 1ull << 3;
constexpr std::uint64_t kSelfSegmentBlocked = 1ull << 11;

enum class SelfPlatform : std::uint8_t {
    Unknown = 0,
    Ps4,
    Ps5,
};

struct SelfHeader final {
    std::uint32_t magic = 0;
    std::uint8_t version = 0;
    std::uint8_t mode = 0;
    std::uint8_t endian = 0;
    std::uint8_t attributes = 0;
    std::uint32_t key_type = 0;
    std::uint16_t header_size = 0;
    std::uint16_t metadata_size = 0;
    std::uint64_t file_size = 0;
    std::uint16_t entry_count = 0;
    std::uint16_t flags = 0;
};

struct SelfEntry final {
    std::uint64_t flags = 0;
    std::uint64_t offset = 0;
    std::uint64_t compressed_size = 0;
    std::uint64_t uncompressed_size = 0;

    bool is_encrypted() const noexcept {
        return (flags & kSelfSegmentEncrypted) != 0;
    }
    bool is_compressed() const noexcept {
        return (flags & kSelfSegmentCompressed) != 0;
    }
    bool is_blocked() const noexcept {
        return (flags & kSelfSegmentBlocked) != 0;
    }
};

struct SelfElfSummary final {
    std::size_t offset = 0;
    Elf64Header header;
    std::vector<Elf64ProgramHeader> program_headers;
};

struct SelfLoadMapping final {
    std::size_t program_header_index = 0;
    std::size_t self_entry_index = 0;
    std::uint64_t physical_offset = 0;
    std::uint64_t virtual_address = 0;
    std::uint64_t file_size = 0;
    std::uint64_t memory_size = 0;
};

struct SelfParseResult final {
    SelfPlatform platform = SelfPlatform::Unknown;
    SelfHeader header;
    std::vector<SelfEntry> entries;
    SelfElfSummary elf;
    std::string error;

    bool ok() const noexcept { return error.empty(); }
    bool has_elf() const noexcept { return elf.header.header_size != 0; }
};

// Parses a bounded prefix of a PS4 SELF. The prefix must contain the fixed
// header, entry table, and embedded ELF program-header table. Payload bytes,
// encrypted metadata, and executable code are not read or interpreted.
SelfParseResult ParsePs4SelfHeaders(
    std::span<const std::uint8_t> header_bytes,
    std::uint64_t actual_file_size);

// Parses a bounded prefix of a PS5 SELF. The prefix must contain the fixed
// header, entry table, and embedded ELF program-header table. Payload bytes,
// encrypted metadata, and executable code are not read or interpreted.
SelfParseResult ParsePs5SelfHeaders(
    std::span<const std::uint8_t> header_bytes,
    std::uint64_t actual_file_size);

// Dispatches to the PS4 or PS5 SELF parser based on the file magic.
SelfParseResult ParseSelfHeaders(std::span<const std::uint8_t> header_bytes,
                                 std::uint64_t actual_file_size);

// Correlates loadable ELF program headers with SELF entries by exact payload
// size. This is a conservative physical-layout hint: it does not decrypt,
// execute, or claim that the encrypted payload is ready for mapping.
std::vector<SelfLoadMapping> MatchSelfLoadEntries(
    const SelfParseResult& self);

} // namespace vshift::loader
