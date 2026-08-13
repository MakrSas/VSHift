#include "core/firmware/slb2.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

std::uint32_t ReadU32LE(const std::vector<std::uint8_t>& bytes,
                        std::size_t offset) noexcept {
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "usage: vshift_firmware_inspect PS5UPDATE.PUP\n";
        return 2;
    }

    const std::string path = argv[1];
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        std::cerr << "cannot open firmware file: " << path << '\n';
        return 1;
    }

    const auto end = input.tellg();
    if (end < 0) {
        std::cerr << "cannot determine firmware file size\n";
        return 1;
    }
    const auto file_size = static_cast<std::uint64_t>(end);
    if (file_size < vshift::firmware::kSlb2HeaderSize) {
        std::cerr << "firmware file is smaller than an SLB2 header\n";
        return 1;
    }

    input.seekg(0);
    std::vector<std::uint8_t> header(vshift::firmware::kSlb2HeaderSize);
    input.read(reinterpret_cast<char*>(header.data()),
               static_cast<std::streamsize>(header.size()));
    if (!input) {
        std::cerr << "cannot read SLB2 header\n";
        return 1;
    }
    if (header[0] != 'S' || header[1] != 'L' || header[2] != 'B' ||
        header[3] != '2') {
        std::cerr << "not an SLB2 PS5 firmware container\n";
        return 1;
    }

    const auto entry_count = ReadU32LE(header, 0x0C);
    constexpr std::uint32_t kMaximumEntries = 1'000'000;
    if (entry_count > kMaximumEntries) {
        std::cerr << "SLB2 entry count is unreasonably large\n";
        return 1;
    }

    const auto table_size = vshift::firmware::kSlb2HeaderSize +
                            static_cast<std::size_t>(entry_count) *
                                vshift::firmware::kSlb2EntrySize;
    if (table_size > file_size ||
        table_size > static_cast<std::size_t>(
                          std::numeric_limits<std::streamsize>::max())) {
        std::cerr << "SLB2 file table is outside the firmware file\n";
        return 1;
    }

    std::vector<std::uint8_t> table(table_size);
    input.seekg(0);
    input.read(reinterpret_cast<char*>(table.data()),
               static_cast<std::streamsize>(table.size()));
    if (!input) {
        std::cerr << "cannot read SLB2 file table\n";
        return 1;
    }

    const auto parsed = vshift::firmware::ParseSlb2Table(table, file_size);
    if (!parsed.ok()) {
        std::cerr << "SLB2 parse failed: " << parsed.error << '\n';
        return 1;
    }

    std::cout << "SLB2 firmware container\n"
              << "  size: " << file_size << " bytes\n"
              << "  version: " << parsed.package.version << '\n'
              << "  flags: 0x" << std::hex << parsed.package.flags << std::dec
              << '\n'
              << "  entries: " << parsed.package.entries.size() << '\n';
    for (const auto& entry : parsed.package.entries) {
        std::cout << "  - " << entry.name << ": offset=" << entry.offset
                  << ", size=" << entry.size << '\n';
    }
    return 0;
}
