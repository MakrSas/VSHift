#include "core/boot/ps4_boot.h"

#include "core/loader/elf.h"

#include <cassert>
#include <cstdint>
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

std::vector<std::uint8_t> RawElf(std::uint64_t base) {
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
    Write64(bytes, 0x60, 1);
    Write64(bytes, 0x68, 0x1000);
    Write64(bytes, 0x70, 0x1000);
    bytes[0x1000] = 0xc3;
    return bytes;
}

} // namespace

int main() {
    const auto syscore = RawElf(0x401000);
    const auto shellcore = RawElf(0x501000);
    vshift::boot::Ps4BootSession session;
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
    return 0;
}
