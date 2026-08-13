#include "core/loader/elf_loader.h"

#include <limits>

namespace vshift::loader {

namespace {

bool AddWouldOverflow(std::uint64_t left,
                      std::uint64_t right) noexcept {
    return right > std::numeric_limits<std::uint64_t>::max() - left;
}

std::uint32_t ToMemoryPermissions(std::uint32_t flags) noexcept {
    std::uint32_t permissions = 0;
    if ((flags & 4u) != 0) {
        permissions |= memory::kPermissionRead;
    }
    if ((flags & 2u) != 0) {
        permissions |= memory::kPermissionWrite;
    }
    if ((flags & 1u) != 0) {
        permissions |= memory::kPermissionExecute;
    }
    return permissions;
}

} // namespace

ElfLoadResult MapElfLoadSegments(
    const ElfParseResult& parsed,
    std::span<const std::uint8_t> file_bytes,
    memory::GuestMemory& memory) {
    ElfLoadResult result;
    if (!parsed.ok()) {
        result.error = "cannot map an ELF parse result with errors";
        return result;
    }

    bool entry_mapped = false;
    for (const auto& program : parsed.program_headers) {
        if (program.type != kElfProgramLoad || program.memory_size == 0) {
            continue;
        }

        if (AddWouldOverflow(program.offset, program.file_size) ||
            program.offset + program.file_size > file_bytes.size()) {
            result.error = "ELF PT_LOAD bytes are not available";
            return result;
        }

        const memory::Mapping mapping{
            program.virtual_address,
            program.memory_size,
            ToMemoryPermissions(program.flags),
        };
        const auto mapped = memory.Map(mapping);
        if (!mapped.ok()) {
            result.error = mapped.error;
            return result;
        }

        const auto initialized = memory.Initialize(
            program.virtual_address,
            file_bytes.subspan(static_cast<std::size_t>(program.offset),
                               static_cast<std::size_t>(program.file_size)));
        if (!initialized.ok()) {
            result.error = initialized.error;
            return result;
        }

        result.mappings.push_back(mapping);
        if (parsed.header.entry >= program.virtual_address &&
            parsed.header.entry - program.virtual_address <
                program.memory_size) {
            entry_mapped = true;
        }
    }

    if (result.mappings.empty()) {
        result.error = "ELF image has no non-empty PT_LOAD segments";
        return result;
    }
    if (parsed.header.type == 2 && !entry_mapped) {
        result.error = "ELF entry point is outside PT_LOAD segments";
        result.mappings.clear();
        return result;
    }

    result.entry = parsed.header.entry;
    return result;
}

} // namespace vshift::loader
