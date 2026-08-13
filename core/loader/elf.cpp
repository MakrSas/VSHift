#include "core/loader/elf.h"

#include <limits>

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

bool IsPowerOfTwo(std::uint64_t value) noexcept {
    return value != 0 && (value & (value - 1)) == 0;
}

} // namespace

ElfParseResult ParseElf64Headers(std::span<const std::uint8_t> table_bytes,
                                 std::uint64_t file_size) {
    ElfParseResult result;
    if (file_size < kElf64HeaderSize ||
        table_bytes.size() < kElf64HeaderSize) {
        result.error = "ELF64 header is truncated";
        return result;
    }

    const auto* bytes = table_bytes.data();
    if (bytes[0] != 0x7f || bytes[1] != 'E' || bytes[2] != 'L' ||
        bytes[3] != 'F') {
        result.error = "file does not start with ELF magic";
        return result;
    }
    if (bytes[4] != 2 || bytes[5] != 1 || bytes[6] != 1) {
        result.error = "ELF file is not a little-endian ELF64 image";
        return result;
    }

    result.header.type = ReadU16LE(bytes + 0x10);
    result.header.machine = ReadU16LE(bytes + 0x12);
    result.header.version = ReadU32LE(bytes + 0x14);
    result.header.entry = ReadU64LE(bytes + 0x18);
    result.header.program_header_offset = ReadU64LE(bytes + 0x20);
    result.header.section_header_offset = ReadU64LE(bytes + 0x28);
    result.header.flags = ReadU32LE(bytes + 0x30);
    result.header.header_size = ReadU16LE(bytes + 0x34);
    result.header.program_header_size = ReadU16LE(bytes + 0x36);
    result.header.program_header_count = ReadU16LE(bytes + 0x38);
    result.header.section_header_size = ReadU16LE(bytes + 0x3A);
    result.header.section_header_count = ReadU16LE(bytes + 0x3C);
    result.header.section_name_index = ReadU16LE(bytes + 0x3E);

    if (result.header.version != 1 ||
        result.header.header_size < kElf64HeaderSize) {
        result.error = "ELF64 header fields are invalid";
        return result;
    }
    if (result.header.program_header_size < kElf64ProgramHeaderSize) {
        result.error = "ELF64 program-header size is invalid";
        return result;
    }

    constexpr std::uint16_t kMaximumProgramHeaders = 4096;
    if (result.header.program_header_count > kMaximumProgramHeaders) {
        result.error = "ELF64 program-header count is unreasonable";
        return result;
    }

    const auto program_table_size =
        static_cast<std::uint64_t>(result.header.program_header_size) *
        result.header.program_header_count;
    if (AddWouldOverflow(result.header.program_header_offset,
                         program_table_size)) {
        result.error = "ELF64 program-header table overflows";
        return result;
    }
    const auto program_table_end = result.header.program_header_offset +
                                   program_table_size;
    if (program_table_end > file_size ||
        program_table_end > table_bytes.size()) {
        result.error = "ELF64 program-header table is truncated";
        return result;
    }

    result.program_headers.reserve(result.header.program_header_count);
    for (std::uint16_t index = 0;
         index < result.header.program_header_count; ++index) {
        const auto entry_offset = result.header.program_header_offset +
                                  static_cast<std::uint64_t>(index) *
                                      result.header.program_header_size;
        const auto* entry = table_bytes.data() +
                            static_cast<std::size_t>(entry_offset);
        Elf64ProgramHeader program;
        program.type = ReadU32LE(entry + 0x00);
        program.flags = ReadU32LE(entry + 0x04);
        program.offset = ReadU64LE(entry + 0x08);
        program.virtual_address = ReadU64LE(entry + 0x10);
        program.physical_address = ReadU64LE(entry + 0x18);
        program.file_size = ReadU64LE(entry + 0x20);
        program.memory_size = ReadU64LE(entry + 0x28);
        program.alignment = ReadU64LE(entry + 0x30);

        if (program.type == kElfProgramLoad) {
            if (program.file_size > program.memory_size ||
                AddWouldOverflow(program.offset, program.file_size) ||
                program.offset + program.file_size > file_size ||
                AddWouldOverflow(program.virtual_address,
                                 program.memory_size)) {
                result.error = "ELF64 PT_LOAD range is invalid";
                result.program_headers.clear();
                return result;
            }
            if (program.alignment != 0 &&
                !IsPowerOfTwo(program.alignment)) {
                result.error = "ELF64 PT_LOAD alignment is invalid";
                result.program_headers.clear();
                return result;
            }
        }
        result.program_headers.push_back(program);
    }

    return result;
}

} // namespace vshift::loader
