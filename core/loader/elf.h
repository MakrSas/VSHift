#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace vshift::loader {

constexpr std::size_t kElf64HeaderSize = 0x40;
constexpr std::size_t kElf64ProgramHeaderSize = 0x38;
constexpr std::uint32_t kElfProgramLoad = 1;
constexpr std::uint16_t kElfMachineX86_64 = 62;
constexpr std::uint16_t kElfMachineAArch64 = 183;

struct Elf64Header final {
    std::uint16_t type = 0;
    std::uint16_t machine = 0;
    std::uint32_t version = 0;
    std::uint64_t entry = 0;
    std::uint64_t program_header_offset = 0;
    std::uint64_t section_header_offset = 0;
    std::uint32_t flags = 0;
    std::uint16_t header_size = 0;
    std::uint16_t program_header_size = 0;
    std::uint16_t program_header_count = 0;
    std::uint16_t section_header_size = 0;
    std::uint16_t section_header_count = 0;
    std::uint16_t section_name_index = 0;
};

struct Elf64ProgramHeader final {
    std::uint32_t type = 0;
    std::uint32_t flags = 0;
    std::uint64_t offset = 0;
    std::uint64_t virtual_address = 0;
    std::uint64_t physical_address = 0;
    std::uint64_t file_size = 0;
    std::uint64_t memory_size = 0;
    std::uint64_t alignment = 0;
};

struct ElfParseResult final {
    Elf64Header header;
    std::vector<Elf64ProgramHeader> program_headers;
    std::string error;

    bool ok() const noexcept { return error.empty(); }
};

// Parses an ELF64 header and its program-header table. `file_size` is the
// size of the complete source file; `table_bytes` only needs to contain the
// header and program-header table. No section data or loadable bytes are read.
ElfParseResult ParseElf64Headers(std::span<const std::uint8_t> table_bytes,
                                 std::uint64_t file_size);

} // namespace vshift::loader
