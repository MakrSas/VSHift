#include "core/boot/ps3_runtime.h"
#include "core/firmware/ps3_pup.h"
#include "core/firmware/ps3_tar.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <span>
#include <string>
#include <stdexcept>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 2 && argc != 3) {
        std::cerr << "usage: vshift_ps3_runtime_probe PS3UPDAT.PUP [max-instructions]\n";
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
    {
        std::array<std::uint8_t, 16> bytes_at_stub{};
        if (runtime.memory().Read(0x6d5a80, bytes_at_stub).ok()) {
            std::cout << "before run 0x6d5a80:";
            for (const auto byte : bytes_at_stub) {
                std::cout << " " << std::hex << static_cast<unsigned>(byte);
            }
            std::cout << std::dec << '\n';
        }
    }
    if (runtime.lv2() != nullptr) {
        std::cout << "VSH linker imports: " << runtime.lv2()->imported_function_count()
                  << " exports: " << runtime.lv2()->exported_function_count() << '\n';
    }
    for (const auto& [path, file] : runtime.firmware_files()) {
        if (path.find("fs") != std::string::npos ||
            path.find("utility") != std::string::npos) {
            std::cout << "  filesystem candidate " << path
                      << " size=" << file.size() << '\n';
        }
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
    std::size_t max_instructions = 100000;
    if (argc == 3) {
        try {
            const auto parsed = std::stoull(argv[2]);
            if (parsed == 0 || parsed > std::numeric_limits<std::size_t>::max()) {
                throw std::out_of_range("instruction limit");
            }
            max_instructions = static_cast<std::size_t>(parsed);
        } catch (const std::exception&) {
            std::cerr << "invalid max-instructions\n";
            return 2;
        }
    }
    const auto result = runtime.Run(max_instructions);
    {
        std::array<std::uint8_t, 16> bytes_at_stub{};
        if (runtime.memory().Read(0x6d5a80, bytes_at_stub).ok()) {
            std::cout << "after run 0x6d5a80:";
            for (const auto byte : bytes_at_stub) {
                std::cout << " " << std::hex << static_cast<unsigned>(byte);
            }
            std::cout << std::dec << '\n';
        }
    }
    if (runtime.lv2() != nullptr) {
        std::cout << "VSH linker imports after run: "
                  << runtime.lv2()->imported_function_count()
                  << " exports: " << runtime.lv2()->exported_function_count() << '\n';
        for (const auto& call : runtime.lv2()->trace()) {
            if (call.syscall != 497) continue;
            std::array<std::uint8_t, 256> path_bytes{};
            std::string path;
            if (runtime.memory().Read(call.arguments[0], path_bytes).ok()) {
                for (const auto byte : path_bytes) {
                    if (byte == 0) break;
                    path.push_back(static_cast<char>(byte));
                }
            }
            std::cout << "  prx load result=0x" << std::hex << call.result
                      << std::dec << " path=" << path;
            if (!runtime.lv2()->last_prx_load_error().empty()) {
                std::cout << " error=" << runtime.lv2()->last_prx_load_error();
            }
            std::cout << '\n';
        }
        for (const auto& call : runtime.lv2()->trace()) {
            if (call.syscall != 52 && call.syscall != 53) continue;
            std::cout << "  ppu thread "
                      << vshift::hle::Ps3Lv2::Name(call.syscall)
                      << " result=0x" << std::hex << call.result
                      << " args=0x" << call.arguments[0]
                      << ",0x" << call.arguments[1]
                      << ",0x" << call.arguments[2]
                      << ",0x" << call.arguments[3]
                      << std::dec << '\n';
        }
    }
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
              << " r6: 0x" << result.registers.gpr[6]
              << " r7: 0x" << result.registers.gpr[7]
              << " r8: 0x" << result.registers.gpr[8]
              << " r0: 0x" << result.registers.gpr[0]
              << " r2: 0x" << result.registers.gpr[2]
              << " r9: 0x" << result.registers.gpr[9]
              << " r12: 0x" << result.registers.gpr[12]
              << " ctr: 0x" << result.registers.ctr
              << " lr: 0x" << result.registers.lr << std::dec << '\n'
              << "  lv2 calls: "
              << (runtime.lv2() == nullptr ? 0 : runtime.lv2()->trace().size()) << '\n'
              << "  result: " << (result.error.empty() ? "execution stopped" : result.error)
              << '\n';
    if (!result.error.empty() && result.registers.gpr[9] != 0) {
        std::array<std::uint8_t, 16> descriptor{};
        if (runtime.memory().Read(result.registers.gpr[9], descriptor).ok()) {
            std::cout << "  fault descriptor 0x" << std::hex << result.registers.gpr[9]
                      << ":";
            for (const auto byte : descriptor) {
                std::cout << " " << static_cast<unsigned>(byte);
            }
            std::cout << std::dec << '\n';
        }
    }
    const auto& ppu_trace = runtime.ppu().trace();
    const auto ppu_tail = ppu_trace.size() > 64 ? ppu_trace.size() - 64 : 0;
    for (std::size_t index = ppu_tail; index < ppu_trace.size(); ++index) {
        std::cout << "  ppu #" << index << " pc=0x" << std::hex
                  << ppu_trace[index].pc << " instruction=0x"
                  << ppu_trace[index].instruction
                  << " r0=0x" << ppu_trace[index].r0
                  << " r2=0x" << ppu_trace[index].r2
                  << " r3=0x" << ppu_trace[index].r3
                  << " r4=0x" << ppu_trace[index].r4
                  << " r5=0x" << ppu_trace[index].r5
                  << " r9=0x" << ppu_trace[index].r9
                  << " ctr=0x" << ppu_trace[index].ctr
                  << std::dec << '\n';
    }
    // The first VSH shared-memory failure currently reads its allocator state
    // through lwz r8,-31976(r2).  Report every code reference to that TOC slot
    // and its live value so the producer can be found without altering guest
    // control flow or mapping a bogus null page.
    constexpr std::array<std::uint8_t, 4> allocator_slot_load{
        0x81, 0x02, 0x83, 0x18};
    std::cout << "  allocator-slot references:";
    for (const auto& mapping : runtime.memory().Mappings()) {
        if ((mapping.permissions & vshift::memory::kPermissionExecute) == 0 ||
            mapping.size < allocator_slot_load.size()) {
            continue;
        }
        std::array<std::uint8_t, 4> word{};
        for (std::uint64_t offset = 0;
             offset + word.size() <= mapping.size; offset += 4) {
            if (!runtime.memory().Read(mapping.guest_address + offset, word).ok()) {
                break;
            }
            if (word == allocator_slot_load) {
                std::cout << " 0x" << std::hex
                          << mapping.guest_address + offset;
            }
        }
    }
    std::cout << std::dec << '\n';
    std::cout << "  TOC displacement 0x8318 users:";
    for (const auto& mapping : runtime.memory().Mappings()) {
        if ((mapping.permissions & vshift::memory::kPermissionExecute) == 0 ||
            mapping.size < 4) continue;
        std::array<std::uint8_t, 4> bytes{};
        for (std::uint64_t offset = 0; offset + 4 <= mapping.size; offset += 4) {
            if (!runtime.memory().Read(mapping.guest_address + offset, bytes).ok()) {
                break;
            }
            const auto instruction =
                (static_cast<std::uint32_t>(bytes[0]) << 24) |
                (static_cast<std::uint32_t>(bytes[1]) << 16) |
                (static_cast<std::uint32_t>(bytes[2]) << 8) | bytes[3];
            if ((instruction & 0x001fffffu) == 0x00028318u) {
                std::cout << " 0x" << std::hex << mapping.guest_address + offset
                          << "=0x" << instruction;
            }
        }
    }
    std::cout << std::dec << '\n';
    constexpr std::array<std::uint8_t, 4> missing_table_address{
        0x01, 0x0c, 0x3b, 0x10};
    std::cout << "  references to 0x10c3b10:";
    for (const auto& mapping : runtime.memory().Mappings()) {
        if (mapping.size < missing_table_address.size()) continue;
        std::array<std::uint8_t, 4> word{};
        for (std::uint64_t offset = 0;
             offset + word.size() <= mapping.size; offset += 4) {
            if (!runtime.memory().Read(mapping.guest_address + offset, word).ok()) {
                break;
            }
            if (word == missing_table_address) {
                std::cout << " 0x" << std::hex
                          << mapping.guest_address + offset;
            }
        }
    }
    std::cout << std::dec << '\n';
    for (const auto center : {0x0cffd69cull, 0x0cffd79cull}) {
        std::array<std::uint8_t, 0x60> bytes{};
        if (!runtime.memory().Read(center - 0x30, bytes).ok()) continue;
        std::cout << "  stack record around 0x" << std::hex << center << ":";
        for (std::size_t offset = 0; offset < bytes.size(); offset += 4) {
            const auto value = (static_cast<std::uint32_t>(bytes[offset]) << 24) |
                               (static_cast<std::uint32_t>(bytes[offset + 1]) << 16) |
                               (static_cast<std::uint32_t>(bytes[offset + 2]) << 8) |
                               bytes[offset + 3];
            std::cout << " " << value;
        }
        std::cout << std::dec << '\n';
    }
    if (result.registers.gpr[2] >= 31976) {
        const auto allocator_slot = result.registers.gpr[2] - 31976;
        std::array<std::uint8_t, 16> slot_bytes{};
        if (runtime.memory().Read(allocator_slot, slot_bytes).ok()) {
            std::cout << "  allocator slot 0x" << std::hex << allocator_slot
                      << ":";
            for (const auto byte : slot_bytes) {
                std::cout << " " << static_cast<unsigned>(byte);
            }
            std::cout << std::dec << '\n';
        }
    }
    if (runtime.lv2() != nullptr) {
        std::string module;
        std::uint32_t nid = 0;
        if (runtime.lv2()->LookupImport(0x010c3b10, module, nid)) {
            std::cout << "  unresolved import slot 0x10c3b10: " << module
                      << " nid=0x" << std::hex << nid << std::dec << '\n';
        }
        std::uint32_t location = 0;
        bool variable = false;
        if (runtime.lv2()->LookupNearestImport(0x010c3b10, location, variable,
                                               module, nid)) {
            std::cout << "  nearest import to 0x10c3b10: 0x" << std::hex
                      << location << " " << (variable ? "variable " : "function ")
                      << module << " nid=0x" << nid << std::dec << '\n';
        }
        std::cout << "  variable imports: "
                  << runtime.lv2()->imported_variable_count() << '\n';
    }
    if (runtime.lv2() != nullptr) {
        const auto& trace = runtime.lv2()->trace();
        std::map<std::uint64_t, std::size_t> syscall_counts;
        for (const auto& call : trace) {
            ++syscall_counts[call.syscall];
        }
        std::vector<std::pair<std::uint64_t, std::size_t>> sorted_counts(
            syscall_counts.begin(), syscall_counts.end());
        std::sort(sorted_counts.begin(), sorted_counts.end(),
                  [](const auto& left, const auto& right) {
                      return left.second != right.second
                          ? left.second > right.second
                          : left.first < right.first;
                  });
        std::cout << "  top LV2 calls:";
        for (std::size_t index = 0;
             index < std::min<std::size_t>(12, sorted_counts.size()); ++index) {
            const auto [syscall, count] = sorted_counts[index];
            std::cout << " " << vshift::hle::Ps3Lv2::Name(syscall)
                      << "=" << count;
        }
        std::cout << '\n';
        std::map<std::pair<std::uint64_t, std::uint64_t>, std::size_t> sleep_sites;
        for (const auto& call : trace) {
            if (call.syscall == 141 || call.syscall == 142) {
                ++sleep_sites[{call.pc, call.arguments[0]}];
            }
        }
        std::vector<std::pair<std::pair<std::uint64_t, std::uint64_t>, std::size_t>>
            sorted_sleep_sites(sleep_sites.begin(), sleep_sites.end());
        std::sort(sorted_sleep_sites.begin(), sorted_sleep_sites.end(),
                  [](const auto& left, const auto& right) {
                      return left.second != right.second
                          ? left.second > right.second
                          : left.first < right.first;
                  });
        if (!sorted_sleep_sites.empty()) {
            std::cout << "  top timer wait sites:";
            for (std::size_t index = 0;
                 index < std::min<std::size_t>(6, sorted_sleep_sites.size()); ++index) {
                const auto& [site, count] = sorted_sleep_sites[index];
                std::cout << " pc=0x" << std::hex << site.first
                          << " arg=0x" << site.second << std::dec
                          << " count=" << count;
            }
            std::cout << '\n';
            const auto wait_pc = sorted_sleep_sites.front().first.first;
            constexpr std::uint64_t kWindowBefore = 0x20;
            std::array<std::uint8_t, 0x44> code_window{};
            if (wait_pc >= kWindowBefore &&
                runtime.memory().Read(wait_pc - kWindowBefore, code_window).ok()) {
                std::cout << "  hottest timer code:";
                for (std::size_t offset = 0; offset < code_window.size(); offset += 4) {
                    const auto opcode =
                        (static_cast<std::uint32_t>(code_window[offset]) << 24) |
                        (static_cast<std::uint32_t>(code_window[offset + 1]) << 16) |
                        (static_cast<std::uint32_t>(code_window[offset + 2]) << 8) |
                        code_window[offset + 3];
                    std::cout << " 0x" << std::hex << (wait_pc - kWindowBefore + offset)
                              << "=0x" << opcode;
                }
                std::cout << std::dec << '\n';
            }
        }

        // Full import/PRX/filesystem diagnostics are useful while isolating a
        // short boot failure but become both noisy and misleading when VSH is
        // alive long enough to poll its services. Keep the normal probe output
        // bounded; opt in to the detailed trace with an environment variable.
        const bool verbose_trace = std::getenv("VSHIFT_PS3_PROBE_VERBOSE") != nullptr;
        if (verbose_trace) {
        for (const auto& call : trace) {
            bool touches_global = false;
            for (const auto argument : call.arguments) {
                touches_global |= argument >= 0x010c3b00 &&
                                  argument < 0x010c3c00;
            }
            if (!touches_global) continue;
            std::cout << "  global-state LV2 #" << call.ordinal << " "
                      << vshift::hle::Ps3Lv2::Name(call.syscall)
                      << " result=0x" << std::hex << call.result
                      << " args=0x" << call.arguments[0] << ",0x"
                      << call.arguments[1] << ",0x" << call.arguments[2]
                      << ",0x" << call.arguments[3] << std::dec << '\n';
        }
        for (const auto& call : trace) {
            if (call.syscall != 805 && call.syscall != 806 && call.syscall != 807 &&
                call.syscall != 808 && call.syscall != 811 &&
                call.syscall != 482) continue;
            std::string path;
            std::array<std::uint8_t, 256> path_bytes{};
            if ((call.syscall == 805 || call.syscall == 808 ||
                 call.syscall == 811) &&
                runtime.memory().Read(call.arguments[0], path_bytes).ok()) {
                for (const auto byte : path_bytes) {
                    if (byte == 0) break;
                    path.push_back(static_cast<char>(byte));
                }
            }
            std::cout << "  fs/prx boundary " << vshift::hle::Ps3Lv2::Name(call.syscall)
                      << " result=0x" << std::hex << call.result << std::dec
                      << " args=0x" << std::hex << call.arguments[0]
                      << ",0x" << call.arguments[1] << ",0x" << call.arguments[2]
                      << ",0x" << call.arguments[3] << std::dec;
            if (!path.empty()) std::cout << " path=" << path;
            std::cout << '\n';
            if (call.syscall == 482) {
                for (const auto address : {call.arguments[2], call.arguments[3]}) {
                    std::array<std::uint8_t, 0x30> option{};
                    if (!runtime.memory().Read(address, option).ok()) continue;
                    std::cout << "    stop option 0x" << std::hex << address << ":";
                    for (std::size_t offset = 0; offset + 8 <= option.size(); offset += 8) {
                        std::uint64_t value = 0;
                        for (std::size_t byte = 0; byte < 8; ++byte) {
                            value = (value << 8) | option[offset + byte];
                        }
                        std::cout << " 0x" << value;
                    }
                    std::cout << std::dec << '\n';
                }
            }
        }
        for (const auto& call : trace) {
            if (call.syscall != 484 && call.syscall != 486) continue;
            std::cout << "  prx registration #" << call.ordinal << ' '
                      << vshift::hle::Ps3Lv2::Name(call.syscall)
                      << " args=0x" << std::hex << call.arguments[0]
                      << ",0x" << call.arguments[1]
                      << ",0x" << call.arguments[2]
                      << ",0x" << call.arguments[3] << std::dec << '\n';
            if (call.syscall == 484) {
                std::array<std::uint8_t, 0x30> option{};
                if (runtime.memory().Read(call.arguments[1], option).ok()) {
                    std::cout << "    register option:";
                    for (std::size_t offset = 0; offset + 8 <= option.size(); offset += 8) {
                        std::uint64_t value = 0;
                        for (std::size_t byte = 0; byte < 8; ++byte) {
                            value = (value << 8) | option[offset + byte];
                        }
                        std::cout << " 0x" << std::hex << value;
                    }
                    std::cout << std::dec << '\n';
                }
            }
            if (call.syscall == 486) {
                std::array<std::uint8_t, 0x1c> library{};
                if (runtime.memory().Read(call.arguments[0], library).ok()) {
                    std::cout << "    library record:";
                    for (std::size_t offset = 0; offset + 4 <= library.size(); offset += 4) {
                        const auto value = (static_cast<std::uint32_t>(library[offset]) << 24) |
                                           (static_cast<std::uint32_t>(library[offset + 1]) << 16) |
                                           (static_cast<std::uint32_t>(library[offset + 2]) << 8) |
                                           library[offset + 3];
                        std::cout << " 0x" << std::hex << value;
                    }
                    std::cout << std::dec << '\n';
                }
            }
        }
        const auto start = trace.size() > 8 ? trace.size() - 8 : 0;
        for (std::size_t index = start; index < trace.size(); ++index) {
            std::cout << "  lv2 #" << trace[index].ordinal << ' '
                      << vshift::hle::Ps3Lv2::Name(trace[index].syscall)
                      << " pc=0x" << std::hex << trace[index].pc << std::dec
                      << " args=0x" << std::hex << trace[index].arguments[0]
                      << ",0x" << trace[index].arguments[1]
                      << ",0x" << trace[index].arguments[2]
                      << ",0x" << trace[index].arguments[3] << std::dec
                      << '\n';
            if (trace[index].syscall == 481) {
                std::array<std::uint8_t, 0x28> option{};
                if (runtime.memory().Read(trace[index].arguments[2], option).ok()) {
                    std::cout << "    start option:";
                    std::uint64_t entry_address = 0;
                    for (std::size_t offset = 0; offset + 8 <= option.size(); offset += 8) {
                        std::uint64_t value = 0;
                        for (std::size_t byte = 0; byte < 8; ++byte) {
                            value = (value << 8) | option[offset + byte];
                        }
                        if (offset == 0x10) entry_address = value;
                        std::cout << " 0x" << std::hex << value;
                    }
                    std::cout << std::dec << '\n';
                    if (entry_address != 0 && entry_address != ~std::uint64_t{0}) {
                        std::array<std::uint8_t, 0x20> descriptor{};
                        if (runtime.memory().Read(entry_address, descriptor).ok()) {
                            std::cout << "    entry descriptor:";
                            for (std::size_t offset = 0; offset + 8 <= descriptor.size(); offset += 8) {
                                std::uint64_t value = 0;
                                for (std::size_t byte = 0; byte < 8; ++byte) {
                                    value = (value << 8) | descriptor[offset + byte];
                                }
                                std::cout << " 0x" << std::hex << value;
                            }
                            std::cout << std::dec << '\n';
                        }
                    }
                }
            }
        }
        }

        // Always retain the terminal boundary: it is the one call that
        // explains why a bounded real-VSH run stopped, without turning the
        // normal probe output into a full syscall log.
        if (!trace.empty()) {
            const auto& last_call = trace.back();
            std::cout << "  terminal LV2 #" << last_call.ordinal << ' '
                      << vshift::hle::Ps3Lv2::Name(last_call.syscall)
                      << " pc=0x" << std::hex << last_call.pc
                      << " args=0x" << last_call.arguments[0]
                      << ",0x" << last_call.arguments[1]
                      << ",0x" << last_call.arguments[2]
                      << ",0x" << last_call.arguments[3] << std::dec
                      << '\n';
        }
    }
    return 0;
}
