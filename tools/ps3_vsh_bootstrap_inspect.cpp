#include "core/firmware/ps3_package.h"
#include "core/firmware/ps3_pup.h"
#include "core/firmware/ps3_tar.h"
#include "core/cpu/ppu_runtime.h"
#include "core/loader/ps3_sce.h"
#include "core/loader/ps3_self.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

bool ReadAt(std::ifstream& file,
            std::uint64_t offset,
            std::uint64_t size,
            std::vector<std::uint8_t>& output) {
    output.resize(static_cast<std::size_t>(size));
    file.seekg(static_cast<std::streamoff>(offset));
    file.read(reinterpret_cast<char*>(output.data()),
              static_cast<std::streamsize>(size));
    return file.good() || file.gcount() == static_cast<std::streamsize>(size);
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2 && argc != 4) {
        std::cerr << "usage: vshift_ps3_vsh_bootstrap_inspect PS3UPDAT.PUP [--dump-vsh path]\n";
        return 2;
    }
    const bool dump_vsh = argc == 4 && std::string(argv[2]) == "--dump-vsh";
    if (argc == 4 && !dump_vsh) {
        std::cerr << "unknown option\n";
        return 2;
    }

    std::ifstream file(argv[1], std::ios::binary);
    if (!file) {
        std::cerr << "could not open PUP\n";
        return 1;
    }
    file.seekg(0, std::ios::end);
    const auto file_size = static_cast<std::uint64_t>(file.tellg());
    file.seekg(0);

    std::vector<std::uint8_t> fixed(vshift::firmware::kPs3PupHeaderSize);
    file.read(reinterpret_cast<char*>(fixed.data()),
              static_cast<std::streamsize>(fixed.size()));
    if (file.gcount() != static_cast<std::streamsize>(fixed.size())) {
        std::cerr << "could not read PUP header\n";
        return 1;
    }
    const auto header_length = [&]() {
        std::uint64_t value = 0;
        for (std::size_t index = 0; index < 8; ++index) {
            value = (value << 8) | fixed[0x20 + index];
        }
        return value;
    }();
    std::vector<std::uint8_t> header(static_cast<std::size_t>(header_length));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(header.data()),
              static_cast<std::streamsize>(header.size()));
    const auto pup = vshift::firmware::ParsePs3PupHeaders(
        header, file_size);
    if (!pup.ok()) {
        std::cerr << "PUP parse failed: " << pup.error << '\n';
        return 1;
    }

    const auto entry = std::find_if(
        pup.entries.begin(), pup.entries.end(),
        [](const auto& candidate) { return candidate.entry_id == 0x300; });
    if (entry == pup.entries.end()) {
        std::cerr << "PUP has no update TAR entry\n";
        return 1;
    }

    std::vector<std::uint8_t> update_tar;
    if (!ReadAt(file, entry->data_offset, entry->data_length, update_tar)) {
        std::cerr << "could not read update TAR\n";
        return 1;
    }
    const auto tar = vshift::firmware::ParsePs3Tar(update_tar);
    if (!tar.ok()) {
        std::cerr << "update TAR parse failed: " << tar.error << '\n';
        return 1;
    }

    const auto package_entry = std::find_if(
        tar.entries.begin(), tar.entries.end(), [](const auto& candidate) {
            return candidate.regular_file &&
                   candidate.name.find("dev_flash_012") == 0;
        });
    if (package_entry == tar.entries.end()) {
        std::cerr << "update TAR has no dev_flash_012 package\n";
        return 1;
    }

    const auto package_begin = static_cast<std::size_t>(
        package_entry->data_offset);
    const auto package_end = package_begin + static_cast<std::size_t>(
        package_entry->data_length);
    if (package_end > update_tar.size()) {
        std::cerr << "dev_flash package range is invalid\n";
        return 1;
    }
    const auto package = vshift::firmware::DecryptPs3ScePackage(
        std::span<const std::uint8_t>(update_tar.data() + package_begin,
                                      package_end - package_begin));
    if (!package.ok()) {
        std::cerr << "dev_flash package decrypt failed: " << package.error
                  << '\n';
        return 1;
    }

    const vshift::firmware::Ps3TarParseResult* vsh_tar = nullptr;
    const std::vector<std::uint8_t>* vsh_section = nullptr;
    vshift::firmware::Ps3TarParseResult parsed_tar;
    for (const auto& section : package.sections) {
        parsed_tar = vshift::firmware::ParsePs3Tar(section.bytes);
        if (parsed_tar.ok() && !parsed_tar.entries.empty()) {
            vsh_tar = &parsed_tar;
            vsh_section = &section.bytes;
            break;
        }
    }
    if (vsh_tar == nullptr) {
        std::cerr << "dev_flash package has no decrypted TAR section\n";
        return 1;
    }

    const auto vsh = std::find_if(
        vsh_tar->entries.begin(), vsh_tar->entries.end(), [](const auto& candidate) {
            return candidate.regular_file &&
                   candidate.name == "dev_flash/vsh/module/vsh.self";
        });
    if (vsh == vsh_tar->entries.end()) {
        std::cerr << "dev_flash TAR has no vsh.self\n";
        return 1;
    }
    if (vsh_section == nullptr ||
        static_cast<std::uint64_t>(vsh->data_offset) + vsh->data_length >
            vsh_section->size()) {
        std::cerr << "VSH TAR entry range is invalid\n";
        return 1;
    }
    const auto vsh_bytes = std::span<const std::uint8_t>(
        vsh_section->data() + static_cast<std::size_t>(vsh->data_offset),
        static_cast<std::size_t>(vsh->data_length));
    if (dump_vsh) {
        std::ofstream output(argv[3], std::ios::binary);
        if (!output) {
            std::cerr << "could not open VSH dump path\n";
            return 1;
        }
        output.write(reinterpret_cast<const char*>(vsh_bytes.data()),
                     static_cast<std::streamsize>(vsh_bytes.size()));
        if (!output) {
            std::cerr << "could not write VSH dump\n";
            return 1;
        }
    }
    std::cout << "PS3 VSH bootstrap\n"
              << "  PUP version: 4.93\n"
              << "  package: " << package_entry->name << '\n'
              << "  decrypted sections: " << package.sections.size() << '\n'
              << "  vsh.self size: " << vsh->data_length << " bytes\n"
              << "  SELF: parsing and decrypting metadata\n";
    const auto self = vshift::loader::ParsePs3Self(vsh_bytes);
    if (!self.ok()) {
        std::cerr << "PS3 SELF parse failed: " << self.error << '\n';
        return 1;
    }
    vshift::memory::GuestMemory memory;
    const auto loaded = vshift::loader::LoadPs3SelfIntoMemory(
        self.image, memory);
    if (!loaded.ok()) {
        std::cerr << "PS3 SELF load failed: " << loaded.error << '\n';
        return 1;
    }
    std::cout << "  ELF: " << (self.image.elf_class == 2 ? "64-bit" : "32-bit")
              << " big-endian PowerPC\n"
              << "  entry: 0x" << std::hex << self.image.entry_point << std::dec << '\n'
              << "  mapped PT_LOAD segments: " << loaded.loaded_segments << '\n'
              << "  entry bytes:";
    std::array<std::uint8_t, 16> entry_bytes{};
    if (memory.Read(self.image.entry_point, entry_bytes).ok()) {
        for (const auto byte : entry_bytes) {
            std::cout << ' ' << std::hex << static_cast<unsigned>(byte);
        }
        std::cout << std::dec;
    } else {
        std::cout << " unavailable";
    }
    std::cout << '\n'
              << "  entry descriptor target:";
    std::array<std::uint8_t, 16> code_bytes{};
    std::uint64_t code_address = 0;
    if (entry_bytes.size() >= 4) {
        for (const auto byte : std::span<const std::uint8_t>(entry_bytes).subspan(0, 4)) {
            code_address = (code_address << 8) | byte;
        }
    }
    if (memory.Read(code_address, code_bytes).ok()) {
        std::cout << " 0x" << std::hex << code_address;
        for (const auto byte : code_bytes) {
            std::cout << ' ' << static_cast<unsigned>(byte);
        }
        std::cout << std::dec;
    } else {
        std::cout << " unavailable";
    }
    std::cout << '\n'
              << "  PPU: starting VSH entry descriptor\n";
    const auto stack_map = memory.Map({
        0x0c000000, 0x01000000,
        vshift::memory::kPermissionRead | vshift::memory::kPermissionWrite});
    if (!stack_map.ok()) {
        std::cerr << "PPU stack map failed: " << stack_map.error << '\n';
        return 1;
    }
    std::uint32_t descriptor_code = 0;
    std::uint32_t descriptor_toc = 0;
    if (memory.Read(self.image.entry_point,
                    std::span<std::uint8_t>(entry_bytes).subspan(0, 8)).ok()) {
        descriptor_code = (static_cast<std::uint32_t>(entry_bytes[0]) << 24) |
                          (static_cast<std::uint32_t>(entry_bytes[1]) << 16) |
                          (static_cast<std::uint32_t>(entry_bytes[2]) << 8) |
                          entry_bytes[3];
        descriptor_toc = (static_cast<std::uint32_t>(entry_bytes[4]) << 24) |
                        (static_cast<std::uint32_t>(entry_bytes[5]) << 16) |
                        (static_cast<std::uint32_t>(entry_bytes[6]) << 8) |
                        entry_bytes[7];
    } else {
        std::cerr << "PPU entry descriptor is unreadable\n";
        return 1;
    }
    vshift::cpu::PpuRuntime ppu(memory);
    ppu.registers().pc = descriptor_code;
    ppu.registers().gpr[1] = 0x0cfff000;
    ppu.registers().gpr[2] = descriptor_toc;
    const auto ppu_result = ppu.Run(100000);
    const auto reason = [&]() {
        switch (ppu_result.reason) {
        case vshift::cpu::PpuStopReason::StepLimit: return "step-limit";
        case vshift::cpu::PpuStopReason::Syscall: return "syscall";
        case vshift::cpu::PpuStopReason::UnsupportedInstruction: return "unsupported";
        case vshift::cpu::PpuStopReason::MemoryFault: return "memory-fault";
        case vshift::cpu::PpuStopReason::Halted: return "halted";
        }
        return "unknown";
    }();
    std::cout << "  PPU result: " << reason
              << " after " << ppu_result.instructions << " instructions\n"
              << "  PPU PC: 0x" << std::hex << ppu_result.registers.pc
              << " instruction: 0x" << ppu_result.instruction
              << " syscall/r11: 0x" << ppu_result.registers.gpr[11]
              << std::dec << '\n';
    if (!ppu_result.error.empty()) {
        std::cout << "  PPU detail: " << ppu_result.error << '\n';
    }
    std::cout << "  result: VSH image loaded; first PPU run attempted; LV2/RSX framebuffer is next\n";
    return 0;
}
