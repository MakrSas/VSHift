#include "core/firmware/ps3_pup.h"
#include "core/loader/ps3_sce.h"

#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

std::uint64_t ReadU64BE(const std::vector<std::uint8_t>& bytes,
                        std::size_t offset) noexcept {
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        value = (value << 8) | bytes[offset + index];
    }
    return value;
}

bool ReadExactly(std::ifstream& input,
                 std::vector<std::uint8_t>& bytes) {
    input.read(reinterpret_cast<char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    return input.gcount() == static_cast<std::streamsize>(bytes.size());
}

bool ReadEntryPrefix(std::ifstream& input,
                     const vshift::firmware::Ps3PupFileEntry& entry,
                     std::vector<std::uint8_t>& prefix) {
    if (entry.data_length < prefix.size()) {
        return false;
    }
    input.clear();
    input.seekg(static_cast<std::streamoff>(entry.data_offset));
    return ReadExactly(input, prefix);
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "usage: vshift_ps3_firmware_inspect PS3UPDAT.PUP\n";
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
    if (file_size < vshift::firmware::kPs3PupHeaderSize) {
        std::cerr << "PS3 PUP is smaller than its fixed header\n";
        return 1;
    }

    input.seekg(0);
    std::vector<std::uint8_t> fixed_header(
        vshift::firmware::kPs3PupHeaderSize);
    if (!ReadExactly(input, fixed_header)) {
        std::cerr << "cannot read PS3 PUP fixed header\n";
        return 1;
    }

    constexpr std::uint64_t kMaximumHeaderBytes = 16ull * 1024ull * 1024ull;
    const auto header_length = ReadU64BE(fixed_header, 0x20);
    if (header_length < vshift::firmware::kPs3PupHeaderSize ||
        header_length > kMaximumHeaderBytes || header_length > file_size ||
        header_length >
            static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        std::cerr << "PS3 PUP header length is invalid\n";
        return 1;
    }

    std::vector<std::uint8_t> header(static_cast<std::size_t>(header_length));
    input.seekg(0);
    if (!ReadExactly(input, header)) {
        std::cerr << "cannot read PS3 PUP header tables\n";
        return 1;
    }

    const auto parsed = vshift::firmware::ParsePs3PupHeaders(header, file_size);
    if (!parsed.ok()) {
        std::cerr << "PS3 PUP parse failed: " << parsed.error << '\n';
        return 1;
    }

    std::cout << "PS3 PUP container\n"
              << "  size: 0x" << std::hex << file_size << std::dec
              << " bytes\n"
              << "  package version: 0x" << std::hex
              << parsed.header.package_version << "\n"
              << "  image version: 0x" << parsed.header.image_version << "\n"
              << "  header length: 0x" << parsed.header.header_length << "\n"
              << "  data length: 0x" << parsed.header.data_length << "\n"
              << "  entries: " << std::dec << parsed.entries.size() << '\n';
    for (const auto& entry : parsed.entries) {
        std::cout << "  - id=0x" << std::hex << entry.entry_id
                  << ": offset=0x" << entry.data_offset
                  << ", size=0x" << entry.data_length;
        std::vector<std::uint8_t> prefix(
            vshift::loader::kPs3SceHeaderSize);
        if (ReadEntryPrefix(input, entry, prefix)) {
            const auto sce = vshift::loader::ParsePs3SceHeader(
                prefix, entry.data_length);
            if (sce.ok()) {
                std::cout << ", SCE type=0x" << sce.header.type
                          << ", metadata="
                          << (sce.header.metadata_encrypted()
                                  ? "encrypted"
                                  : "plaintext");
            }
        }
        std::cout << '\n';
    }
    std::cout << std::dec
              << "  note: structural validation only; payload hashes and "
                 "package decryption are not attempted\n";
    return 0;
}
