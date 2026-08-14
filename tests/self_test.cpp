#include "core/loader/self.h"
#include "core/loader/self_loader.h"
#include "core/memory/guest_memory.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

namespace {

void WriteU16LE(std::vector<std::uint8_t>& bytes,
                std::size_t offset,
                std::uint16_t value) {
    bytes[offset + 0] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void WriteU32LE(std::vector<std::uint8_t>& bytes,
                std::size_t offset,
                std::uint32_t value) {
    bytes[offset + 0] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
    bytes[offset + 2] = static_cast<std::uint8_t>(value >> 16);
    bytes[offset + 3] = static_cast<std::uint8_t>(value >> 24);
}

void WriteU64LE(std::vector<std::uint8_t>& bytes,
                std::size_t offset,
                std::uint64_t value) {
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        bytes[offset + index] =
            static_cast<std::uint8_t>(value >> (index * 8));
    }
}

} // namespace

int main() {
    constexpr std::uint64_t kFileSize = 0x10000;
    std::vector<std::uint8_t> header(0x2B0, 0);

    WriteU32LE(header, 0x00, vshift::loader::kPs5SelfMagic);
    header[0x04] = 0x10;
    header[0x05] = 0x01;
    header[0x06] = 0x01;
    header[0x07] = 0x32;
    WriteU32LE(header, 0x08, 0x10000701);
    WriteU16LE(header, 0x0C, 0x2B0);
    WriteU16LE(header, 0x0E, 0x3F0);
    WriteU64LE(header, 0x10, kFileSize);
    WriteU16LE(header, 0x18, 3);
    WriteU16LE(header, 0x1A, 0x52);

    WriteU64LE(header, 0x20, 0x110006);
    WriteU64LE(header, 0x28, 0x6A0);
    WriteU64LE(header, 0x30, 0x500);
    WriteU64LE(header, 0x38, 0x500);
    WriteU64LE(header, 0x40, 0x280006);
    WriteU64LE(header, 0x48, 0xBA0);
    WriteU64LE(header, 0x50, 0x2000);
    WriteU64LE(header, 0x58, 0x2000);
    WriteU64LE(header, 0x60, 0x510006);
    WriteU64LE(header, 0x68, 0x3BA0);
    WriteU64LE(header, 0x70, 0x3000);
    WriteU64LE(header, 0x78, 0x3000);

    constexpr std::size_t kElfOffset = 0xE0;
    header[kElfOffset + 0x00] = 0x7f;
    header[kElfOffset + 0x01] = 'E';
    header[kElfOffset + 0x02] = 'L';
    header[kElfOffset + 0x03] = 'F';
    header[kElfOffset + 0x04] = 2;
    header[kElfOffset + 0x05] = 1;
    header[kElfOffset + 0x06] = 1;
    WriteU16LE(header, kElfOffset + 0x10, 2);
    WriteU16LE(header, kElfOffset + 0x12, vshift::loader::kElfMachineX86_64);
    WriteU32LE(header, kElfOffset + 0x14, 1);
    WriteU64LE(header, kElfOffset + 0x18, 0x400080);
    WriteU64LE(header, kElfOffset + 0x20, 0x40);
    WriteU16LE(header, kElfOffset + 0x34, 0x40);
    WriteU16LE(header, kElfOffset + 0x36, 0x38);
    WriteU16LE(header, kElfOffset + 0x38, 2);

    const auto ph0 = kElfOffset + 0x40;
    WriteU32LE(header, ph0 + 0x00, vshift::loader::kElfProgramLoad);
    WriteU32LE(header, ph0 + 0x04, 5);
    WriteU64LE(header, ph0 + 0x08, 0x4000);
    WriteU64LE(header, ph0 + 0x10, 0x400000);
    WriteU64LE(header, ph0 + 0x20, 0x2000);
    WriteU64LE(header, ph0 + 0x28, 0x2000);
    WriteU64LE(header, ph0 + 0x30, 0x4000);

    const auto ph1 = ph0 + 0x38;
    WriteU32LE(header, ph1 + 0x00, vshift::loader::kElfProgramLoad);
    WriteU32LE(header, ph1 + 0x04, 6);
    WriteU64LE(header, ph1 + 0x08, 0x5000);
    WriteU64LE(header, ph1 + 0x10, 0x404000);
    WriteU64LE(header, ph1 + 0x20, 0x3000);
    WriteU64LE(header, ph1 + 0x28, 0x3000);
    WriteU64LE(header, ph1 + 0x30, 0x4000);

    const auto parsed = vshift::loader::ParsePs5SelfHeaders(
        header, kFileSize);
    assert(parsed.ok());
    assert(parsed.header.entry_count == 3);
    assert(parsed.entries.size() == 3);
    assert(parsed.elf.offset == kElfOffset);
    assert(parsed.elf.header.machine == vshift::loader::kElfMachineX86_64);
    assert(parsed.elf.header.entry == 0x400080);
    assert(parsed.elf.program_headers.size() == 2);

    const auto mappings = vshift::loader::MatchSelfLoadEntries(parsed);
    assert(mappings.size() == 2);
    assert(mappings[0].program_header_index == 0);
    assert(mappings[0].self_entry_index == 1);
    assert(mappings[0].physical_offset == 0xBA0);
    assert(mappings[1].program_header_index == 1);
    assert(mappings[1].self_entry_index == 2);

    WriteU64LE(header, 0x50, kFileSize);
    assert(!vshift::loader::ParsePs5SelfHeaders(header, kFileSize).ok());

    WriteU64LE(header, 0x50, 0x2000);
    WriteU32LE(header, 0x00, vshift::loader::kPs4SelfMagic);
    const auto ps4 = vshift::loader::ParsePs4SelfHeaders(header, kFileSize);
    assert(ps4.ok());
    assert(ps4.platform == vshift::loader::SelfPlatform::Ps4);
    assert(ps4.header.magic == vshift::loader::kPs4SelfMagic);
    assert(ps4.elf.header.machine == vshift::loader::kElfMachineX86_64);
    assert(ps4.entries[0].is_encrypted());
    assert(!ps4.entries[0].is_compressed());
    assert(!ps4.entries[0].is_blocked());

    const auto dispatched =
        vshift::loader::ParseSelfHeaders(header, kFileSize);
    assert(dispatched.ok());
    assert(dispatched.platform == vshift::loader::SelfPlatform::Ps4);

    std::vector<std::uint8_t> plain_self(kFileSize, 0);
    std::copy(header.begin(), header.end(), plain_self.begin());
    WriteU32LE(plain_self, 0x00, vshift::loader::kPs4SelfMagic);
    for (std::size_t index = 0; index < 3; ++index) {
        WriteU64LE(plain_self, 0x20 + index * 0x20, 0x100001);
    }
    for (std::size_t index = 0; index < 0x2000; ++index) {
        plain_self[0xBA0 + index] = static_cast<std::uint8_t>(index);
    }
    for (std::size_t index = 0; index < 0x3000; ++index) {
        plain_self[0x3BA0 + index] = static_cast<std::uint8_t>(index ^ 0x5a);
    }
    const auto plain_headers = vshift::loader::ParsePs4SelfHeaders(
        std::span<const std::uint8_t>(plain_self.data(), header.size()),
        kFileSize);
    assert(plain_headers.ok());
    vshift::memory::GuestMemory guest_memory;
    const auto plain_loaded = vshift::loader::MapSelfLoadSegments(
        plain_headers, plain_self, guest_memory);
    assert(plain_loaded.ok());
    assert(plain_loaded.entry == 0x400080);
    assert(plain_loaded.mappings.size() == 2);
    std::array<std::uint8_t, 4> mapped_bytes = {};
    assert(guest_memory.Read(0x400000, mapped_bytes).ok());
    assert(mapped_bytes[0] == 0x00);
    assert(mapped_bytes[1] == 0x01);

    const auto protected_loaded = vshift::loader::MapSelfLoadSegments(
        ps4, plain_self, guest_memory);
    assert(!protected_loaded.ok());
    assert(protected_loaded.error.find("encrypted") != std::string::npos);

    WriteU32LE(header, 0x00, 0x12345678);
    assert(!vshift::loader::ParseSelfHeaders(header, kFileSize).ok());
    return 0;
}
