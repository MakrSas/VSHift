#pragma once

#include "core/loader/elf.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace vshift::loader {

constexpr std::uint32_t kElfProgramDynamic = 2;

struct ElfDynamicEntry final {
    std::int64_t tag = 0;
    std::uint64_t value = 0;
};

struct ElfDynamicInfo final {
    bool present = false;
    std::uint64_t string_table = 0;
    std::uint64_t string_table_size = 0;
    std::uint64_t symbol_table = 0;
    std::uint64_t rela_table = 0;
    std::uint64_t rela_size = 0;
    std::uint64_t rela_entry_size = 0;
    std::uint64_t jump_rela_table = 0;
    std::uint64_t jump_rela_size = 0;
    std::uint64_t plt_relocation_type = 0;
    std::vector<ElfDynamicEntry> entries;
    std::vector<std::string> needed_libraries;
    std::string error;

    bool ok() const noexcept { return error.empty(); }
};

// Parses only the file-backed dynamic segment. It validates every virtual
// address against a PT_LOAD file range and returns bounded DT_NEEDED names;
// relocation application and symbol binding remain a separate linker stage.
ElfDynamicInfo ParseElf64Dynamic(
    const ElfParseResult& parsed,
    std::span<const std::uint8_t> file_bytes);

} // namespace vshift::loader
