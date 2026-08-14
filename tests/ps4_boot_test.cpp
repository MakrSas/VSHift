#include "core/boot/ps4_boot.h"

#include "core/hle/kernel.h"
#include "core/loader/elf.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace {

void Write16(std::vector<std::uint8_t>& bytes,
             std::size_t offset,
             std::uint16_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void Write32(std::vector<std::uint8_t>& bytes,
             std::size_t offset,
             std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) {
        bytes[offset + i] = static_cast<std::uint8_t>(value >> (i * 8));
    }
}

void Write64(std::vector<std::uint8_t>& bytes,
             std::size_t offset,
             std::uint64_t value) {
    for (std::size_t i = 0; i < 8; ++i) {
        bytes[offset + i] = static_cast<std::uint8_t>(value >> (i * 8));
    }
}

std::vector<std::uint8_t> RawElf(
    std::uint64_t base,
    std::span<const std::uint8_t> program = std::span<const std::uint8_t>()) {
    std::vector<std::uint8_t> bytes(0x2000, 0);
    bytes[0] = 0x7f;
    bytes[1] = 'E';
    bytes[2] = 'L';
    bytes[3] = 'F';
    bytes[4] = 2;
    bytes[5] = 1;
    bytes[6] = 1;
    Write16(bytes, 0x10, 2);
    Write16(bytes, 0x12, vshift::loader::kElfMachineX86_64);
    Write32(bytes, 0x14, 1);
    Write64(bytes, 0x18, base);
    Write64(bytes, 0x20, 0x40);
    Write16(bytes, 0x34, 0x40);
    Write16(bytes, 0x36, 0x38);
    Write16(bytes, 0x38, 1);
    Write32(bytes, 0x40, vshift::loader::kElfProgramLoad);
    Write32(bytes, 0x44, 5);
    Write64(bytes, 0x48, 0x1000);
    Write64(bytes, 0x50, base);
    Write64(bytes, 0x60, program.empty() ? 1 : 0x200);
    Write64(bytes, 0x68, 0x1000);
    Write64(bytes, 0x70, program.empty() ? 0x1000 : 0x2000);
    if (program.empty()) {
        bytes[0x1000] = 0xc3;
    } else {
        std::copy(program.begin(), program.end(), bytes.begin() + 0x1000);
    }
    return bytes;
}

} // namespace

int main() {
    const std::array<std::uint8_t, 53> frameProgram = {
        0x48, 0xb8, 0xed, 0xfe, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, // mov rax, 0xfeed
        0x48, 0xbf, 0x00, 0x11, 0x40, 0x00, 0x00, 0x00,
        0x00, 0x00, // mov rdi, 0x401100
        0x48, 0xbe, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, // mov rsi, 2
        0x48, 0xba, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, // mov rdx, 1
        0x48, 0xb9, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, // mov rcx, 8
        0x0f, 0x05, 0xc3, // syscall; ret
    };
    auto syscore = RawElf(0x401000, frameProgram);
    const std::array<std::uint8_t, 8> pixels = {
        255, 0, 0, 255, 0, 0, 255, 255};
    std::copy(pixels.begin(), pixels.end(), syscore.begin() + 0x1100);
    const auto shellcore = RawElf(0x501000);
    bool presented = false;
    vshift::boot::Ps4BootSession session(
        [&](const vshift::video::GuestFrame& frame) {
            presented = frame.description.width == 2 &&
                        frame.description.height == 1;
            return presented;
        });
    session.syscalls().Register(0xfeed, vshift::hle::PresentRgba8Frame);
    const auto report = session.Run([&](std::string_view path) {
        if (path == "system/sys/SceSysCore.elf") {
            return vshift::boot::BootFile{syscore, {}};
        }
        if (path == "system/vsh/SceShellCore.elf") {
            return vshift::boot::BootFile{shellcore, {}};
        }
        return vshift::boot::BootFile{{}, "not found"};
    });
    assert(report.ok());
    assert(report.modules_mapped());
    assert(report.stage == vshift::boot::Ps4BootStage::GuestExecution);
    assert(report.syscore.entry == 0x401000);
    assert(report.shellcore.entry == 0x501000);
    assert(presented);
    assert(session.video_output().last_frame() != nullptr);
    return 0;
}
