#include "core/loader/elf_loader.h"

#include <array>
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

std::vector<std::uint8_t> MakeImage() {
    std::vector<std::uint8_t> image(0x3000, 0);
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
    WriteU16LE(image, 0x38, 1);
    WriteU32LE(image, 0x40, vshift::loader::kElfProgramLoad);
    WriteU32LE(image, 0x44, 5);
    WriteU64LE(image, 0x48, 0x1000);
    WriteU64LE(image, 0x50, 0x401000);
    WriteU64LE(image, 0x58, 0);
    WriteU64LE(image, 0x60, 4);
    WriteU64LE(image, 0x68, 0x1000);
    WriteU64LE(image, 0x70, 0x1000);
    for (std::size_t index = 0; index < 4; ++index) {
        image[0x1000 + index] = static_cast<std::uint8_t>(0xa0 + index);
    }
    return image;
}

} // namespace

int main() {
    auto image = MakeImage();
    const auto parsed = vshift::loader::ParseElf64Headers(image, image.size());
    assert(parsed.ok());

    vshift::memory::GuestMemory memory;
    const auto loaded = vshift::loader::MapElfLoadSegments(
        parsed, image, memory);
    assert(loaded.ok());
    assert(loaded.entry == 0x401000);
    assert(loaded.mappings.size() == 1);

    std::array<std::uint8_t, 4> bytes = {};
    assert(memory.Read(0x401000, bytes).ok());
    assert(bytes[0] == 0xa0 && bytes[3] == 0xa3);
    std::array<std::uint8_t, 4> zeroes = {};
    assert(memory.Read(0x401004, zeroes).ok());
    const std::array<std::uint8_t, 4> expected_zeroes = {};
    assert(zeroes == expected_zeroes);
    return 0;
}
