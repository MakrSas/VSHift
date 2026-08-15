#include "core/firmware/ps3_package.h"
#include "core/firmware/ps3_pup.h"
#include "core/firmware/ps3_tar.h"
#include "core/cpu/ppu_runtime.h"
#include "core/hle/ps3_lv2.h"
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
    if (argc != 2 && argc != 3 && argc != 4) {
        std::cerr << "usage: vshift_ps3_vsh_bootstrap_inspect PS3UPDAT.PUP [--inspect-vsh|--dump-vsh path]\n";
        return 2;
    }
    const bool inspect_vsh = argc == 3 && std::string(argv[2]) == "--inspect-vsh";
    const bool dump_vsh = argc == 4 && std::string(argv[2]) == "--dump-vsh";
    if ((argc == 3 && !inspect_vsh) || (argc == 4 && !dump_vsh)) {
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
    if (inspect_vsh) {
        std::cout << "VSH program headers\n";
        for (std::size_t index = 0; index < self.image.program_headers.size(); ++index) {
            const auto& header = self.image.program_headers[index];
            std::cout << "  #" << index << " type=0x" << std::hex << header.type
                      << " flags=0x" << header.flags
                      << " offset=0x" << header.offset
                      << " va=0x" << header.virtual_address
                      << " pa=0x" << header.physical_address
                      << " file=0x" << header.file_size
                      << " mem=0x" << header.memory_size << std::dec << '\n';
            const auto raw_offset = self.image.extension.program_header_offset +
                                    index * (self.image.elf_class == 2 ? 0x38ull : 0x20ull);
            if (raw_offset + (self.image.elf_class == 2 ? 0x38ull : 0x20ull) <=
                vsh_bytes.size()) {
                std::cout << "    raw:";
                const auto raw_size = self.image.elf_class == 2 ? 0x38u : 0x20u;
                for (std::size_t offset = 0; offset < raw_size; ++offset) {
                    if ((offset % 4) == 0) std::cout << ' ';
                    std::cout << std::hex << static_cast<unsigned>(
                        vsh_bytes[static_cast<std::size_t>(raw_offset + offset)]);
                }
                std::cout << std::dec << '\n';
            }
            const auto section = std::find_if(
                self.image.sections.begin(), self.image.sections.end(),
                [index](const auto& candidate) {
                    return candidate.type == 2 && candidate.program_index == index;
                });
            if (section == self.image.sections.end() || section->bytes.empty()) continue;
            const auto limit = std::min<std::size_t>(section->bytes.size(), 0x40);
            std::cout << "    bytes:";
            for (std::size_t offset = 0; offset < limit; ++offset) {
                if ((offset % 4) == 0) std::cout << ' ';
                std::cout << std::hex << static_cast<unsigned>(section->bytes[offset]);
            }
            std::cout << std::dec << '\n';
        }
        const auto mapped_segment = std::find_if(
            self.image.program_headers.begin(), self.image.program_headers.end(),
            [](const auto& header) {
                return header.type == 1 && header.virtual_address == 0x6c0000;
            });
        if (mapped_segment != self.image.program_headers.end()) {
            const auto mapped_index = static_cast<std::size_t>(std::distance(
                self.image.program_headers.begin(), mapped_segment));
            const auto section = std::find_if(
                self.image.sections.begin(), self.image.sections.end(),
                [mapped_index](const auto& candidate) {
                    return candidate.type == 2 && candidate.program_index == mapped_index;
                });
            if (section != self.image.sections.end() && section->bytes.size() > 0x15aa0) {
                std::cout << "VSH data at 0x6d5a80:";
                for (std::size_t offset = 0x15a80; offset < 0x15ac0; offset += 8) {
                    std::uint64_t value = 0;
                    for (std::size_t byte = 0; byte < 8; ++byte) {
                        value = (value << 8) | section->bytes[offset + byte];
                    }
                    std::cout << " 0x" << std::hex << value;
                }
                std::cout << std::dec << '\n';
            }
        }
        vshift::memory::GuestMemory inspect_memory;
        const auto inspect_loaded = vshift::loader::LoadPs3SelfIntoMemory(
            self.image, inspect_memory);
        if (inspect_loaded.ok()) {
            const auto read_u32 = [&](std::uint64_t address,
                                      std::uint32_t& value) {
                std::array<std::uint8_t, 4> bytes{};
                if (!inspect_memory.Read(address, bytes).ok()) return false;
                value = (static_cast<std::uint32_t>(bytes[0]) << 24) |
                        (static_cast<std::uint32_t>(bytes[1]) << 16) |
                        (static_cast<std::uint32_t>(bytes[2]) << 8) | bytes[3];
                return true;
            };
            for (const auto address : {0x367b80u, 0x367ba0u, 0x367bc0u,
                                       0x367be0u, 0x367bfcu, 0x617000u,
                                       0x61701cu}) {
                std::cout << "guest bytes at 0x" << std::hex << address << ":";
                for (std::size_t offset = 0; offset < 32; offset += 4) {
                    std::uint32_t value = 0;
                    if (!read_u32(address + offset, value)) break;
                    std::cout << " 0x" << value;
                }
                std::cout << std::dec << '\n';
            }
            std::size_t pointer_hits = 0;
            for (std::size_t program_index = 0;
                 program_index < self.image.program_headers.size();
                 ++program_index) {
                const auto& program = self.image.program_headers[program_index];
                if (program.type != 1 || program.file_size < 4) continue;
                const auto section = std::find_if(
                    self.image.sections.begin(), self.image.sections.end(),
                    [program_index](const auto& candidate) {
                        return candidate.type == 2 &&
                               candidate.program_index == program_index;
                    });
                if (section == self.image.sections.end() ||
                    section->bytes.size() < program.file_size) continue;
                for (std::uint64_t offset = 0; offset + 4 <= program.file_size;
                     offset += 4) {
                    const auto bytes = std::span<const std::uint8_t>(section->bytes)
                        .subspan(static_cast<std::size_t>(offset), 4);
                    const auto value = (static_cast<std::uint32_t>(bytes[0]) << 24) |
                                       (static_cast<std::uint32_t>(bytes[1]) << 16) |
                                       (static_cast<std::uint32_t>(bytes[2]) << 8) |
                                       bytes[3];
                    if (value != 0x367bfcu) continue;
                    std::cout << "pointer 0x367bfc at guest 0x" << std::hex
                              << (program.virtual_address + offset) << std::dec << '\n';
                    if (++pointer_hits >= 32) break;
                }
                if (pointer_hits >= 32) break;
            }
            const auto read_string = [&](std::uint64_t address) {
                std::string value;
                for (std::size_t index = 0; index < 64; ++index) {
                    std::uint32_t word = 0;
                    const auto byte_address = address + index;
                    std::array<std::uint8_t, 1> byte{};
                    if (!inspect_memory.Read(byte_address, byte).ok() || byte[0] == 0) break;
                    value.push_back(static_cast<char>(byte[0]));
                }
                return value;
            };
            std::cout << "VSH process-info candidates\n";
            for (const auto address : {0x1008cull, 0x101dcull, 0x6ec840ull,
                                       0x6ec844ull, 0x6ec848ull, 0x6ec84cull,
                                       0x6ec850ull, 0x6ec854ull}) {
                std::uint32_t value = 0;
                if (!read_u32(address, value)) continue;
                std::cout << "  [0x" << std::hex << address << "] = 0x" << value;
                std::uint32_t pointed = 0;
                if (read_u32(value, pointed)) {
                    std::cout << " -> 0x" << pointed;
                }
                std::cout << std::dec << '\n';
            }
            for (const auto address : {0x6c0984ull, 0x6c0988ull, 0x6c098cull,
                                       0x6c0990ull, 0x6c0994ull, 0x6c0998ull,
                                       0x6c099cull}) {
                std::uint32_t value = 0;
                if (!read_u32(address, value)) continue;
                std::cout << "  [0x" << std::hex << address << "] = 0x" << value
                          << std::dec << '\n';
            }
            std::cout << "candidate import records:" << '\n';
            for (const auto range : {std::pair<std::uint64_t, std::uint64_t>{0x10000, 0x6bbea8},
                                     {0x6c0000, 0x6f5558}}) {
                for (auto address = range.first; address + 0x2c <= range.second;
                     address += 4) {
                    std::uint32_t value = 0;
                    if (!read_u32(address, value) || value != 0x2c000001u) continue;
                    std::uint32_t name = 0;
                    std::uint32_t nids = 0;
                    std::uint32_t addrs = 0;
                    std::uint32_t counts = 0;
                    read_u32(address + 0x10, name);
                    read_u32(address + 0x14, nids);
                    read_u32(address + 0x18, addrs);
                    read_u32(address + 0x08, counts);
                    const auto function_count = (counts >> 16) & 0xffffu;
                    const auto variable_count = counts & 0xffffu;
                    std::cout << "  record 0x" << std::hex << address
                              << " name=0x" << name << " (" << read_string(name)
                              << ") nids=0x" << nids << " addrs=0x" << addrs
                              << " funcs=" << std::dec << function_count
                              << " vars=" << variable_count << '\n';
                    for (std::uint32_t index = 0; index < function_count && index < 3;
                         ++index) {
                        std::uint32_t nid = 0;
                        std::uint32_t addr_value = 0;
                        read_u32(nids + index * 4, nid);
                        read_u32(addrs + index * 4, addr_value);
                        std::cout << "    fn[" << index << "] nid=0x" << std::hex << nid
                                  << " addr=0x" << addr_value << std::dec << '\n';
                    }
                }
            }
        }
        return 0;
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
    for (const auto address : {0x609100ull, 0x609200ull, 0x609300ull,
                               0x60d000ull, 0x60d100ull,
                               0x9d8c0ull, 0x9d9c0ull, 0x9b300ull,
                               0x9b400ull, 0x9b500ull,
                               0x9b560ull,
                               0x60d240ull, 0x60d700ull, 0x60d800ull,
                               0xc9480ull,
                               0x60bdc0ull,
                               0x6180c0ull, 0x618100ull, 0x618140ull,
                               0x618180ull, 0x6181c0ull}) {
        std::array<std::uint8_t, 0x100> bytes{};
        if (memory.Read(address, bytes).ok()) {
            std::cout << "  code 0x" << std::hex << address << ":";
            for (const auto byte : bytes) {
                std::cout << ' ' << static_cast<unsigned>(byte);
            }
                std::cout << std::dec << '\n';
            }
        }
    const auto stack_map = memory.Map({
        0x0c000000, 0x01000000,
        vshift::memory::kPermissionRead | vshift::memory::kPermissionWrite});
    if (!stack_map.ok()) {
        std::cerr << "PPU stack map failed: " << stack_map.error << '\n';
        return 1;
    }
    const auto write_guest_u64 = [&](std::uint64_t address, std::uint64_t value) {
        std::array<std::uint8_t, 8> bytes{};
        for (std::size_t index = 0; index < bytes.size(); ++index) {
            bytes[index] = static_cast<std::uint8_t>(
                value >> ((bytes.size() - index - 1) * 8));
        }
        return memory.Write(address, bytes).ok();
    };
    constexpr std::uint64_t argv_address = 0x0cffe000;
    constexpr std::uint64_t envp_address = 0x0cffe020;
    constexpr std::uint64_t process_name_address = 0x0cffe100;
    const std::array<std::uint8_t, 12> process_name{
        'v', 's', 'h', '.', 's', 'e', 'l', 'f', 0, 0, 0, 0};
    if (!memory.Write(process_name_address, process_name).ok() ||
        !write_guest_u64(argv_address, process_name_address) ||
        !write_guest_u64(argv_address + 8, 0) ||
        !write_guest_u64(envp_address, 0)) {
        std::cerr << "PPU process argument setup failed\n";
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
    vshift::hle::Ps3Lv2 lv2(memory);
    vshift::cpu::PpuRuntime ppu(memory);
    ppu.registers().pc = descriptor_code;
    ppu.registers().gpr[1] = 0x0cfff000;
    ppu.registers().gpr[2] = descriptor_toc;
    // ELFv1 function-descriptor calls enter the function with r12 carrying
    // the code address. VSH preserves it during its startup trampoline.
    ppu.registers().gpr[12] = descriptor_code;
    ppu.registers().gpr[3] = 1;
    ppu.registers().gpr[4] = argv_address;
    ppu.registers().gpr[5] = envp_address;
    vshift::cpu::PpuRunResult ppu_result;
    std::size_t total_instructions = 0;
    bool abort_entry_reported = false;
    std::size_t lwmutex_watch_count = 0;
    bool lwmutex_result_reported = false;
    bool abort_call_site_reported = false;
    std::size_t observed_lv2_calls = 0;
    while (total_instructions < 5000000) {
        if (!abort_call_site_reported && ppu.registers().pc == 0x9b598) {
            abort_call_site_reported = true;
            std::cout << "  abort-check callsite r31=0x" << std::hex
                      << ppu.registers().gpr[31] << "\n" << std::dec;
        }
        if (ppu.registers().lr == 0x9b434 &&
            ppu.registers().pc >= 0x60d2a0 &&
            ppu.registers().pc <= 0x60d2e4) {
            std::cout << "  lwmutex step pc=0x" << std::hex << ppu.registers().pc
                      << " r3=0x" << ppu.registers().gpr[3]
                      << " r9=0x" << ppu.registers().gpr[9]
                      << " r10=0x" << ppu.registers().gpr[10]
                      << " r11=0x" << ppu.registers().gpr[11]
                      << " cr=0x" << ppu.registers().condition_register
                      << "\n" << std::dec;
        }
        if (ppu.registers().pc == 0x60d100 ||
            ppu.registers().pc == 0x60d110) {
            std::cout << "  lwmutex init pc=0x" << std::hex
                      << ppu.registers().pc << " r0=0x"
                      << ppu.registers().gpr[0] << " r3=0x"
                      << ppu.registers().gpr[3] << " r31=0x"
                      << ppu.registers().gpr[31] << "\n" << std::dec;
        }
        if (ppu.registers().pc == 0x9b51c ||
            ppu.registers().pc == 0x9b52c) {
            std::array<std::uint8_t, 0x14> bytes{};
            memory.Read(0x70d148, bytes);
            std::cout << "  mutex-check pc=0x" << std::hex << ppu.registers().pc
                      << " r3=0x" << ppu.registers().gpr[3]
                      << " tls=0x" << ppu.registers().gpr[13]
                      << " data:";
            for (const auto byte : bytes) {
                std::cout << ' ' << static_cast<unsigned>(byte);
            }
            std::cout << std::dec << '\n';
        }
        if (ppu.registers().pc == 0x60d2a0 &&
            (lwmutex_watch_count < 12 || ppu.registers().lr == 0x9b5a0 ||
             ppu.registers().gpr[3] == 0x70d148)) {
            ++lwmutex_watch_count;
            std::cout << "  lwmutex helper entry r3=0x" << std::hex
                      << ppu.registers().gpr[3] << " r13=0x"
                      << ppu.registers().gpr[13] << " r0=0x"
                      << ppu.registers().gpr[0] << " lr=0x"
                      << ppu.registers().lr << "\n";
        }
        if (!lwmutex_result_reported && ppu.registers().pc == 0x9b5d0) {
            lwmutex_result_reported = true;
            std::cout << "  lwmutex helper result r3=0x" << std::hex
                      << ppu.registers().gpr[3] << "\n" << std::dec;
        }
        const auto slice = ppu.Run(1, [&](auto& registers, auto& error) {
            return lv2.Dispatch(registers, error);
        }, [&](auto& registers) { lv2.PrepareThread(registers); });
        while (observed_lv2_calls < lv2.trace().size()) {
            const auto& call = lv2.trace()[observed_lv2_calls++];
            if (call.syscall == 95) {
                std::array<std::uint8_t, 0x14> bytes{};
                if (memory.Read(call.arguments[2], bytes).ok()) {
                    std::cout << "  lwmutex control after create 0x" << std::hex
                              << call.arguments[2] << ":";
                    for (const auto byte : bytes) {
                        std::cout << ' ' << static_cast<unsigned>(byte);
                    }
                    std::cout << std::dec << '\n';
                }
            }
            if (call.syscall == 497) {
                std::array<std::uint8_t, 0x80> path_bytes{};
                if (memory.Read(call.arguments[0], path_bytes).ok()) {
                    std::cout << "  PRX path: ";
                    for (const auto byte : path_bytes) {
                        if (byte == 0) break;
                        std::cout << static_cast<char>(byte);
                    }
                    std::cout << '\n';
                }
            }
            if (call.syscall == 481) {
                std::array<std::uint8_t, 0x28> option_bytes{};
                if (memory.Read(call.arguments[2], option_bytes).ok()) {
                    std::cout << "  PRX start option:";
                    for (std::size_t offset = 0; offset + 8 <= option_bytes.size();
                         offset += 8) {
                        std::uint64_t value = 0;
                        for (std::size_t index = 0; index < 8; ++index) {
                            value = (value << 8) | option_bytes[offset + index];
                        }
                        std::cout << " 0x" << std::hex << value;
                    }
                    std::cout << std::dec << '\n';
                }
            }
        }
        total_instructions += slice.instructions;
        if (!abort_entry_reported && ppu.registers().pc == 0x9d8c4) {
            abort_entry_reported = true;
            std::cout << "  abort handler entered; caller LR=0x" << std::hex
                      << ppu.registers().lr << " r3=0x" << ppu.registers().gpr[3]
                      << " r4=0x" << ppu.registers().gpr[4]
                      << " r13=0x" << ppu.registers().gpr[13]
                      << " r31=0x" << ppu.registers().gpr[31]
                      << std::dec << '\n';
            for (const auto address : {ppu.registers().gpr[13] - 0x7030,
                                       ppu.registers().gpr[31] + 0x18,
                                       0x70d148ull, 0x70d188ull,
                                       0x70d318ull}) {
                std::array<std::uint8_t, 0x10> bytes{};
                if (memory.Read(address, bytes).ok()) {
                    std::cout << "    data 0x" << std::hex << address << ":";
                    for (const auto byte : bytes) {
                        std::cout << ' ' << static_cast<unsigned>(byte);
                    }
                    std::cout << std::dec << '\n';
                }
            }
        }
        if (slice.reason != vshift::cpu::PpuStopReason::StepLimit) {
            ppu_result = slice;
            ppu_result.instructions = total_instructions;
            break;
        }
        ppu_result = slice;
    }
    ppu_result.instructions = total_instructions;
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
              << " r3: 0x" << ppu_result.registers.gpr[3]
              << " r4: 0x" << ppu_result.registers.gpr[4]
              << " r5: 0x" << ppu_result.registers.gpr[5]
              << " r6: 0x" << ppu_result.registers.gpr[6]
              << " r9: 0x" << ppu_result.registers.gpr[9]
              << " r28: 0x" << ppu_result.registers.gpr[28]
              << " r2: 0x" << ppu_result.registers.gpr[2]
              << " r0: 0x" << ppu_result.registers.gpr[0]
              << " r31: 0x" << ppu_result.registers.gpr[31]
              << " cr: 0x" << ppu_result.registers.condition_register
              << std::dec << '\n';
    std::cout << "  LV2 calls handled: " << lv2.trace().size() << '\n';
    for (std::size_t index = 0; index < lv2.trace().size(); ++index) {
        const auto& call = lv2.trace()[index];
        std::cout << "    #" << call.ordinal << " "
                  << vshift::hle::Ps3Lv2::Name(call.syscall)
                  << " @0x" << std::hex << call.pc
                  << " (0x" << call.syscall << ") -> 0x"
                  << call.result << " args=0x" << call.arguments[0]
                  << ",0x" << call.arguments[1] << ",0x"
                  << call.arguments[2] << ",0x" << call.arguments[3]
                  << std::dec << '\n';
    }
    std::cout << "  TTY output:" << '\n';
    for (const auto& call : lv2.trace()) {
        if (call.syscall != 403 || call.arguments[2] == 0 || call.arguments[2] > 0x1000) {
            continue;
        }
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(call.arguments[2]));
        if (!memory.Read(call.arguments[1], bytes).ok()) continue;
        std::cout << "    ";
        for (const auto byte : bytes) {
            if (byte >= 0x20 && byte < 0x7f) std::cout << static_cast<char>(byte);
            else if (byte == '\n') std::cout << "\\n";
            else if (byte == '\r') std::cout << "\\r";
            else std::cout << "\\x" << std::hex << static_cast<unsigned>(byte) << std::dec;
        }
        std::cout << '\n';
    }
    if (!ppu_result.error.empty()) {
        std::cout << "  PPU detail: " << ppu_result.error << '\n';
    }
    std::cout << "  PPU tail:" << '\n';
    const auto& ppu_trace = ppu.trace();
    const auto ppu_trace_start = ppu_trace.size() > 12 ? ppu_trace.size() - 12 : 0;
    for (std::size_t index = ppu_trace_start; index < ppu_trace.size(); ++index) {
        std::cout << "    0x" << std::hex << ppu_trace[index].pc
                  << ": 0x" << ppu_trace[index].instruction << std::dec << '\n';
    }
    std::cout << "  result: VSH image loaded; PPU/LV2 execution reached the next runtime boundary\n";
    return 0;
}
