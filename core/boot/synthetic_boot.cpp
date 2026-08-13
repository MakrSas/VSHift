#include "core/boot/synthetic_boot.h"

#include "core/cpu/arm64_jit.h"
#include "core/cpu/ir.h"
#include "core/cpu/x86_decoder.h"
#include "core/loader/elf.h"
#include "core/loader/elf_loader.h"
#include "core/memory/guest_memory.h"

#include <algorithm>
#include <array>

namespace vshift::boot {

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

constexpr std::array<std::uint8_t, 9> kSyntheticProgram = {
    0xB8, 0x28, 0x00, 0x00, 0x00,
    0x83, 0xC0, 0x02,
    0xC3,
};

} // namespace

std::vector<std::uint8_t> BuildSyntheticElfFixture() {
    std::vector<std::uint8_t> image(0x2000, 0);
    image[0] = 0x7f;
    image[1] = 'E';
    image[2] = 'L';
    image[3] = 'F';
    image[4] = 2;
    image[5] = 1;
    image[6] = 1;
    WriteU16LE(image, 0x10, 2);
    WriteU16LE(image, 0x12, loader::kElfMachineX86_64);
    WriteU32LE(image, 0x14, 1);
    WriteU64LE(image, 0x18, 0x401000);
    WriteU64LE(image, 0x20, 0x40);
    WriteU16LE(image, 0x34, 0x40);
    WriteU16LE(image, 0x36, 0x38);
    WriteU16LE(image, 0x38, 1);
    WriteU32LE(image, 0x40, loader::kElfProgramLoad);
    WriteU32LE(image, 0x44, 5);
    WriteU64LE(image, 0x48, 0x1000);
    WriteU64LE(image, 0x50, 0x401000);
    WriteU64LE(image, 0x58, 0);
    WriteU64LE(image, 0x60, kSyntheticProgram.size());
    WriteU64LE(image, 0x68, 0x1000);
    WriteU64LE(image, 0x70, 0x1000);
    std::copy(kSyntheticProgram.begin(), kSyntheticProgram.end(),
              image.begin() + 0x1000);
    return image;
}

SyntheticBootReport RunSyntheticElfBoot(
    std::span<const std::uint8_t> elf_image,
    ExecutionMode mode) {
    SyntheticBootReport report;
    report.mode = mode;

    const auto parsed = loader::ParseElf64Headers(elf_image, elf_image.size());
    if (!parsed.ok()) {
        report.error = parsed.error;
        return report;
    }

    memory::GuestMemory guest_memory;
    const auto loaded = loader::MapElfLoadSegments(
        parsed, elf_image, guest_memory);
    if (!loaded.ok()) {
        report.error = loaded.error;
        return report;
    }
    report.entry = loaded.entry;
    report.mapped_segments = loaded.mappings.size();

    std::array<std::uint8_t, kSyntheticProgram.size()> guest_program = {};
    const auto read_result = guest_memory.Read(
        report.entry, guest_program);
    if (!read_result.ok()) {
        report.error = read_result.error;
        return report;
    }

    const auto decoded = cpu::Decode(guest_program);
    if (!decoded.ok()) {
        report.error = decoded.error;
        return report;
    }
    const auto lowered = cpu::LowerToIr(decoded.instructions);
    if (!lowered.ok()) {
        report.error = lowered.error;
        return report;
    }

    if (mode == ExecutionMode::JitLess) {
        const auto interpreted = cpu::Interpret(lowered.instructions);
        if (!interpreted.ok()) {
            report.error = interpreted.error;
            return report;
        }
        report.result = interpreted.state.eax;
        return report;
    }

    const auto compiled = cpu::Arm64Jit::CompileIr(lowered.instructions);
    if (!compiled.ok()) {
        report.error = compiled.error;
        return report;
    }
    if (!compiled.jit->Execute(report.result)) {
        report.error = "JIT compiled but did not execute on this architecture";
    }
    return report;
}

} // namespace vshift::boot
