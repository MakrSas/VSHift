#include "core/boot/ps3_runtime.h"
#include "core/firmware/ps3_pup.h"
#include "core/firmware/ps3_tar.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: vshift_ps3_runtime_probe PS3UPDAT.PUP\n";
        return 2;
    }
    std::ifstream input(argv[1], std::ios::binary);
    if (!input) {
        std::cerr << "could not open PS3 PUP\n";
        return 1;
    }
    const auto begin = std::istreambuf_iterator<char>(input);
    const auto end = std::istreambuf_iterator<char>();
    std::vector<std::uint8_t> bytes(begin, end);
    const auto pup_header_size = [&]() {
        std::uint64_t value = 0;
        for (std::size_t index = 0; index < 8; ++index) {
            value = (value << 8) | bytes[0x20 + index];
        }
        return value;
    }();
    const auto pup = vshift::firmware::ParsePs3PupHeaders(
        std::span<const std::uint8_t>(bytes).first(
            static_cast<std::size_t>(pup_header_size)), bytes.size());
    if (pup.ok()) {
        const auto update = std::find_if(
            pup.entries.begin(), pup.entries.end(),
            [](const auto& entry) { return entry.entry_id == 0x300; });
        if (update != pup.entries.end()) {
            const auto tar = vshift::firmware::ParsePs3Tar(
                std::span<const std::uint8_t>(bytes).subspan(
                    static_cast<std::size_t>(update->data_offset),
                    static_cast<std::size_t>(update->data_length)));
            if (tar.ok()) {
                for (const auto& entry : tar.entries) {
                    if (entry.regular_file &&
                        (entry.name.find("dev_flash") == 0 ||
                         entry.name.find("libfs") != std::string::npos)) {
                        std::cout << "  update entry " << entry.name << " size="
                                  << entry.data_length << '\n';
                    }
                }
            }
        }
    }
    vshift::boot::Ps3Runtime runtime;
    const auto loaded = runtime.LoadFirmware(bytes);
    if (!loaded.ok()) {
        std::cerr << "PS3 runtime load failed: " << loaded.error << '\n';
        return 1;
    }
    for (const auto& [path, file] : runtime.firmware_files()) {
        std::cout << "  file " << path << " size=" << file.size() << '\n';
        if (path.find("libfs_utility2.sprx") == std::string::npos) continue;
        const auto parsed = vshift::loader::ParsePs3Self(file);
        std::cout << "  module " << path << " size=" << file.size()
                  << " parse=" << (parsed.ok() ? "ok" : parsed.error) << '\n';
        if (parsed.ok()) {
            std::cout << "    entry=0x" << std::hex << parsed.image.entry_point
                      << " segments=" << std::dec << parsed.image.program_headers.size()
                      << '\n';
            for (const auto& header : parsed.image.program_headers) {
                std::cout << "    ph type=" << header.type << " va=0x" << std::hex
                          << header.virtual_address << " offset=0x" << header.offset
                          << " file=0x" << header.file_size
                          << " mem=0x" << header.memory_size << " flags=0x"
                          << header.flags << " paddr=0x" << header.physical_address
                          << std::dec << '\n';
            }
            for (const auto& section : parsed.image.sections) {
                std::cout << "    section type=" << section.type
                          << " program=" << section.program_index
                          << " offset=0x" << std::hex << section.data_offset
                          << " size=0x" << section.data_size
                          << " bytes=0x" << section.bytes.size()
                          << " compressed=" << std::dec << section.compressed << '\n';
            }
            if (!parsed.image.program_headers.empty() &&
                !parsed.image.sections.empty()) {
                const auto& header = parsed.image.program_headers.front();
                const auto& section = parsed.image.sections.front();
                const auto info_offset = header.physical_address - header.offset;
                if (info_offset + 0x40 <= section.bytes.size()) {
                    const auto read_u32 = [&](std::size_t offset) {
                        std::uint32_t value = 0;
                        for (std::size_t index = 0; index < 4; ++index) {
                            value = (value << 8) | section.bytes[
                                static_cast<std::size_t>(info_offset + offset + index)];
                        }
                        return value;
                    };
                    std::cout << "    library info toc=0x" << std::hex
                              << read_u32(0x20) << " exports=0x" << read_u32(0x24)
                              << "..0x" << read_u32(0x28) << " imports=0x"
                              << read_u32(0x2c) << "..0x" << read_u32(0x30)
                              << std::dec << '\n';
                }
            }
        }
    }
    const auto result = runtime.Run(100000);
    std::cout << "PS3 runtime loaded\n"
              << "  files: " << loaded.info.firmware_file_count << '\n'
              << "  vsh.self: " << loaded.info.vsh_size << " bytes\n"
              << "  mapped PT_LOAD: " << loaded.info.mapped_segments << '\n'
              << "  instructions: " << result.instructions << '\n'
              << "  pc: 0x" << std::hex << result.registers.pc << std::dec << '\n'
              << "  instruction: 0x" << std::hex << result.instruction << std::dec
              << '\n'
              << "  r3: 0x" << std::hex << result.registers.gpr[3]
              << " r4: 0x" << result.registers.gpr[4]
              << " r5: 0x" << result.registers.gpr[5]
              << " r6: 0x" << result.registers.gpr[6] << std::dec << '\n'
              << "  lv2 calls: "
              << (runtime.lv2() == nullptr ? 0 : runtime.lv2()->trace().size()) << '\n'
              << "  result: " << (result.error.empty() ? "execution stopped" : result.error)
              << '\n';
    if (runtime.lv2() != nullptr) {
        const auto& trace = runtime.lv2()->trace();
        const auto start = trace.size() > 8 ? trace.size() - 8 : 0;
        for (std::size_t index = start; index < trace.size(); ++index) {
            std::cout << "  lv2 #" << trace[index].ordinal << ' '
                      << vshift::hle::Ps3Lv2::Name(trace[index].syscall)
                      << " pc=0x" << std::hex << trace[index].pc << std::dec
                      << '\n';
        }
    }
    return 0;
}
