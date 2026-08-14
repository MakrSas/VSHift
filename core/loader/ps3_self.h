#pragma once

#include "core/loader/ps3_sce.h"
#include "core/memory/guest_memory.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace vshift::loader {

// These are the portable PS3 SELF records needed by the first VSH boot path.
// They are intentionally independent from the PS4/PS5 SELF reader.
struct Ps3SelfExtensionHeader final {
    std::uint64_t version = 0;
    std::uint64_t program_identification_offset = 0;
    std::uint64_t elf_header_offset = 0;
    std::uint64_t program_header_offset = 0;
    std::uint64_t section_header_offset = 0;
    std::uint64_t segment_extension_offset = 0;
    std::uint64_t version_header_offset = 0;
    std::uint64_t supplemental_header_offset = 0;
    std::uint64_t supplemental_header_size = 0;
    std::uint64_t padding = 0;
};

struct Ps3SelfProgramIdentification final {
    std::uint64_t authority_id = 0;
    std::uint32_t vendor_id = 0;
    std::uint32_t program_type = 0;
    std::uint64_t sce_version = 0;
    std::uint64_t padding = 0;
};

struct Ps3SelfSegmentExtension final {
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
    std::uint32_t compression = 0;
    std::uint32_t unknown = 0;
    std::uint64_t encryption = 0;
};

struct Ps3SelfProgramHeader final {
    std::uint32_t type = 0;
    std::uint32_t flags = 0;
    std::uint64_t offset = 0;
    std::uint64_t virtual_address = 0;
    std::uint64_t physical_address = 0;
    std::uint64_t file_size = 0;
    std::uint64_t memory_size = 0;
    std::uint64_t alignment = 0;
};

struct Ps3SelfSection final {
    std::uint64_t data_offset = 0;
    std::uint64_t data_size = 0;
    std::uint32_t type = 0;
    std::uint32_t program_index = 0;
    std::uint32_t hashed = 0;
    std::uint32_t sha1_index = 0;
    std::uint32_t encrypted = 0;
    std::uint32_t key_index = 0;
    std::uint32_t iv_index = 0;
    std::uint32_t compressed = 0;
    std::vector<std::uint8_t> bytes;
};

struct Ps3SelfImage final {
    Ps3SceHeader sce_header;
    Ps3SelfExtensionHeader extension;
    Ps3SelfProgramIdentification program_identification;
    std::vector<Ps3SelfSegmentExtension> segment_extensions;
    std::uint8_t elf_class = 0;
    std::uint8_t elf_data = 0;
    std::uint16_t elf_type = 0;
    std::uint16_t elf_machine = 0;
    std::uint64_t entry_point = 0;
    std::vector<Ps3SelfProgramHeader> program_headers;
    std::vector<Ps3SelfSection> sections;
};

struct Ps3SelfKeys final {
    std::array<std::uint8_t, 32> metadata_key{};
    std::array<std::uint8_t, 16> metadata_iv{};
};

// The PS3 VSH SELF key for revision 0x1c. This is the public application
// key used by the PS3 firmware VSH, not a device- or user-specific secret.
const Ps3SelfKeys& DefaultPs3VshSelfKeys() noexcept;

struct Ps3SelfParseResult final {
    Ps3SelfImage image;
    std::string error;

    bool ok() const noexcept { return error.empty(); }
};

Ps3SelfParseResult ParsePs3Self(
    std::span<const std::uint8_t> self_bytes,
    const Ps3SelfKeys& keys = DefaultPs3VshSelfKeys());

struct Ps3SelfLoadResult final {
    std::uint64_t entry_point = 0;
    std::size_t loaded_segments = 0;
    std::string error;

    bool ok() const noexcept { return error.empty(); }
};

// Maps decrypted PT_LOAD sections into the existing sparse guest memory. The
// loader does not start a CPU or invent a framebuffer; those are separate
// runtime stages that consume this image and entry point.
Ps3SelfLoadResult LoadPs3SelfIntoMemory(
    const Ps3SelfImage& image,
    memory::GuestMemory& memory);

} // namespace vshift::loader
