#include "core/loader/elf_dynamic.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace vshift::loader {

namespace {

constexpr std::int64_t kDtNull = 0;
constexpr std::int64_t kDtNeeded = 1;
constexpr std::int64_t kDtPltRelSize = 2;
constexpr std::int64_t kDtStrTab = 5;
constexpr std::int64_t kDtSymTab = 6;
constexpr std::int64_t kDtRela = 7;
constexpr std::int64_t kDtRelaSize = 8;
constexpr std::int64_t kDtRelaEntry = 9;
constexpr std::int64_t kDtStrSize = 10;
constexpr std::int64_t kDtPltRel = 20;
constexpr std::int64_t kDtJmpRel = 23;
constexpr std::uint64_t kDynamicEntrySize = 16;

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

bool VirtualToFile(const ElfParseResult& parsed,
                   std::uint64_t virtual_address,
                   std::uint64_t size,
                   std::uint64_t& file_offset) {
    for (const auto& program : parsed.program_headers) {
        if (program.type != kElfProgramLoad ||
            virtual_address < program.virtual_address ||
            AddWouldOverflow(virtual_address, size) ||
            AddWouldOverflow(program.virtual_address, program.file_size)) {
            continue;
        }
        const auto relative = virtual_address - program.virtual_address;
        if (relative > program.file_size ||
            size > program.file_size - relative ||
            AddWouldOverflow(program.offset, relative)) {
            continue;
        }
        file_offset = program.offset + relative;
        return true;
    }
    return false;
}

bool ReadString(std::span<const std::uint8_t> bytes,
                std::uint64_t offset,
                std::uint64_t size,
                std::uint64_t string_offset,
                std::string& value) {
    if (string_offset >= size || offset > bytes.size() ||
        size > bytes.size() - offset) {
        return false;
    }
    const auto begin = bytes.begin() + static_cast<std::size_t>(
        offset + string_offset);
    const auto end = bytes.begin() + static_cast<std::size_t>(offset + size);
    const auto terminator = std::find(begin, end, std::uint8_t{0});
    if (terminator == end) {
        return false;
    }
    value.assign(begin, terminator);
    return true;
}

} // namespace

ElfDynamicInfo ParseElf64Dynamic(
    const ElfParseResult& parsed,
    std::span<const std::uint8_t> file_bytes) {
    ElfDynamicInfo result;
    if (!parsed.ok()) {
        result.error = "cannot parse dynamic metadata from an invalid ELF";
        return result;
    }

    const auto dynamic = std::find_if(
        parsed.program_headers.begin(), parsed.program_headers.end(),
        [](const Elf64ProgramHeader& program) {
            return program.type == kElfProgramDynamic;
        });
    if (dynamic == parsed.program_headers.end()) {
        return result;
    }
    result.present = true;
    if (dynamic->file_size == 0 || dynamic->file_size % kDynamicEntrySize != 0 ||
        dynamic->offset > file_bytes.size() ||
        dynamic->file_size > file_bytes.size() - dynamic->offset) {
        result.error = "ELF dynamic segment range is invalid";
        return result;
    }

    const auto entry_count = dynamic->file_size / kDynamicEntrySize;
    constexpr std::uint64_t kMaximumDynamicEntries = 1'000'000;
    if (entry_count > kMaximumDynamicEntries) {
        result.error = "ELF dynamic segment has too many entries";
        return result;
    }

    std::vector<std::uint64_t> needed_offsets;
    for (std::uint64_t index = 0; index < entry_count; ++index) {
        const auto offset = dynamic->offset + index * kDynamicEntrySize;
        const auto* bytes = file_bytes.data() +
                            static_cast<std::size_t>(offset);
        const auto tag = static_cast<std::int64_t>(ReadU64LE(bytes));
        const auto value = ReadU64LE(bytes + 8);
        result.entries.push_back({tag, value});
        if (tag == kDtNull) {
            break;
        }
        if (tag == kDtNeeded) {
            needed_offsets.push_back(value);
        } else if (tag == kDtStrTab) {
            result.string_table = value;
        } else if (tag == kDtStrSize) {
            result.string_table_size = value;
        } else if (tag == kDtSymTab) {
            result.symbol_table = value;
        } else if (tag == kDtRela) {
            result.rela_table = value;
        } else if (tag == kDtRelaSize) {
            result.rela_size = value;
        } else if (tag == kDtRelaEntry) {
            result.rela_entry_size = value;
        } else if (tag == kDtJmpRel) {
            result.jump_rela_table = value;
        } else if (tag == kDtPltRelSize) {
            result.jump_rela_size = value;
        } else if (tag == kDtPltRel) {
            result.plt_relocation_type = value;
        }
    }

    if (result.string_table_size == 0 && !needed_offsets.empty()) {
        result.error = "ELF DT_NEEDED entries have no string-table size";
        return result;
    }
    if (!needed_offsets.empty()) {
        std::uint64_t string_file_offset = 0;
        if (!VirtualToFile(parsed, result.string_table,
                           result.string_table_size, string_file_offset) ||
            string_file_offset > file_bytes.size() ||
            result.string_table_size > file_bytes.size() - string_file_offset) {
            result.error = "ELF dynamic string table is not file-backed";
            return result;
        }
        for (const auto needed : needed_offsets) {
            std::string library;
            if (!ReadString(file_bytes, string_file_offset,
                            result.string_table_size, needed, library)) {
                result.error = "ELF DT_NEEDED string is outside the string table";
                result.needed_libraries.clear();
                return result;
            }
            result.needed_libraries.push_back(std::move(library));
        }
    }

    if (result.rela_size != 0 && result.rela_entry_size == 0) {
        result.error = "ELF DT_RELA has no entry size";
        return result;
    }
    if (result.rela_entry_size != 0 && result.rela_entry_size < 24) {
        result.error = "ELF DT_RELA entry size is invalid";
    }
    return result;
}

} // namespace vshift::loader
