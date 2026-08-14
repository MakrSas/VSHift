#include "core/loader/self_loader.h"

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

SelfLoadResult MapSelfLoadSegments(
    const SelfParseResult& parsed,
    std::span<const std::uint8_t> self_bytes,
    memory::GuestMemory& memory) {
    SelfLoadResult result;
    if (!parsed.ok() || !parsed.has_elf()) {
        result.error = "cannot map a SELF parse result without ELF headers";
        return result;
    }

    const auto mappings = MatchSelfLoadEntries(parsed);
    std::size_t load_count = 0;
    for (const auto& program : parsed.elf.program_headers) {
        if (program.type == kElfProgramLoad && program.memory_size != 0) {
            ++load_count;
        }
    }
    if (mappings.size() != load_count) {
        result.error = "SELF payload map does not cover every PT_LOAD";
        return result;
    }

    bool entry_mapped = false;
    for (const auto& mapping : mappings) {
        if (mapping.program_header_index >= parsed.elf.program_headers.size() ||
            mapping.self_entry_index >= parsed.entries.size()) {
            result.error = "SELF payload map index is invalid";
            result.mappings.clear();
            return result;
        }

        const auto& program =
            parsed.elf.program_headers[mapping.program_header_index];
        const auto& entry = parsed.entries[mapping.self_entry_index];
        if (entry.is_encrypted()) {
            result.error = "SELF payload is encrypted; a decrypted module is required";
            result.mappings.clear();
            return result;
        }
        if (entry.is_compressed()) {
            result.error = "SELF payload is compressed; decompression is required";
            result.mappings.clear();
            return result;
        }
        if (entry.is_blocked()) {
            result.error = "SELF payload is block-based; block reconstruction is required";
            result.mappings.clear();
            return result;
        }
        if (entry.compressed_size != entry.uncompressed_size ||
            entry.uncompressed_size != program.file_size ||
            AddWouldOverflow(entry.offset, entry.uncompressed_size) ||
            entry.offset + entry.uncompressed_size > self_bytes.size()) {
            result.error = "plain SELF payload range is unavailable";
            result.mappings.clear();
            return result;
        }

        const memory::Mapping guest_mapping{
            program.virtual_address,
            program.memory_size,
            ToMemoryPermissions(program.flags),
        };
        const auto mapped = memory.Map(guest_mapping);
        if (!mapped.ok()) {
            result.error = mapped.error;
            result.mappings.clear();
            return result;
        }

        const auto initialized = memory.Initialize(
            program.virtual_address,
            self_bytes.subspan(static_cast<std::size_t>(entry.offset),
                               static_cast<std::size_t>(program.file_size)));
        if (!initialized.ok()) {
            result.error = initialized.error;
            result.mappings.clear();
            return result;
        }

        result.mappings.push_back(guest_mapping);
        if (parsed.elf.header.entry >= program.virtual_address &&
            parsed.elf.header.entry - program.virtual_address <
                program.memory_size) {
            entry_mapped = true;
        }
    }

    if (!entry_mapped) {
        result.error = "SELF ELF entry point is outside PT_LOAD mappings";
        result.mappings.clear();
        return result;
    }
    result.entry = parsed.elf.header.entry;
    return result;
}

} // namespace vshift::loader
