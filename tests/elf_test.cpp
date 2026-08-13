#include "core/loader/elf.h"

#include <cassert>
#include <cstdint>
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

void WriteProgramHeader(std::vector<std::uint8_t>& bytes,
                        std::size_t offset,
                        std::uint32_t type,
                        std::uint64_t file_offset,
                        std::uint64_t file_size,
                        std::uint64_t memory_size) {
    WriteU32LE(bytes, offset + 0x00, type);
    WriteU32LE(bytes, offset + 0x04, 5);
    WriteU64LE(bytes, offset + 0x08, file_offset);
    WriteU64LE(bytes, offset + 0x10, 0x400000 + file_offset);
    WriteU64LE(bytes, offset + 0x18, 0);
    WriteU64LE(bytes, offset + 0x20, file_size);
    WriteU64LE(bytes, offset + 0x28, memory_size);
    WriteU64LE(bytes, offset + 0x30, 0x1000);
}

std::vector<std::uint8_t> MakeImage() {
    std::vector<std::uint8_t> image(0x40 + 2 * 0x38, 0);
    image[0] = 0x7f;
    image[1] = 'E';
    image[2] = 'L';
    image[3] = 'F';
    image[4] = 2;
    image[5] = 1;
    image[6] = 1;
    WriteU16LE(image, 0x10, 2);
    WriteU16LE(image, 0x12, vshift::loader::kElfMachineX86_64);
    WriteU32LE(image, 0x14, 1);
    WriteU64LE(image, 0x18, 0x401000);
    WriteU64LE(image, 0x20, 0x40);
    WriteU16LE(image, 0x34, 0x40);
    WriteU16LE(image, 0x36, 0x38);
    WriteU16LE(image, 0x38, 2);
    WriteProgramHeader(image, 0x40, vshift::loader::kElfProgramLoad,
                       0x1000, 0x200, 0x300);
    WriteProgramHeader(image, 0x78, 2, 0x2000, 0x80, 0x80);
    return image;
}

} // namespace

int main() {
    const auto image = MakeImage();
    const auto parsed = vshift::loader::ParseElf64Headers(image, 0x3000);
    assert(parsed.ok());
    assert(parsed.header.machine == vshift::loader::kElfMachineX86_64);
    assert(parsed.header.entry == 0x401000);
    assert(parsed.program_headers.size() == 2);
    assert(parsed.program_headers[0].file_size == 0x200);
    assert(parsed.program_headers[0].memory_size == 0x300);

    auto malformed = image;
    WriteU64LE(malformed, 0x40 + 0x28, 0x100);
    assert(!vshift::loader::ParseElf64Headers(malformed, 0x3000).ok());

    assert(!vshift::loader::ParseElf64Headers(
                      std::span<const std::uint8_t>(image.data(), 3),
                      0x3000)
                .ok());
    return 0;
}
