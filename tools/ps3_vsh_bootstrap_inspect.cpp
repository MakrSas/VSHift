#include "core/firmware/ps3_package.h"
#include "core/firmware/ps3_pup.h"
#include "core/firmware/ps3_tar.h"
#include "core/loader/ps3_sce.h"

#include <algorithm>
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
    if (argc != 2) {
        std::cerr << "usage: vshift_ps3_vsh_bootstrap_inspect PS3UPDAT.PUP\n";
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
    vshift::firmware::Ps3TarParseResult parsed_tar;
    for (const auto& section : package.sections) {
        parsed_tar = vshift::firmware::ParsePs3Tar(section.bytes);
        if (parsed_tar.ok() && !parsed_tar.entries.empty()) {
            vsh_tar = &parsed_tar;
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
    std::cout << "PS3 VSH bootstrap\n"
              << "  PUP version: 4.93\n"
              << "  package: " << package_entry->name << '\n'
              << "  decrypted sections: " << package.sections.size() << '\n'
              << "  vsh.self size: " << vsh->data_length << " bytes\n"
              << "  result: VSH package prepared; PPU/LV2/RSX execution is next\n";
    return 0;
}
