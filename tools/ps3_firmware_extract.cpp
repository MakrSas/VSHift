#include "core/firmware/ps3_package.h"
#include "core/firmware/ps3_pup.h"
#include "core/firmware/ps3_tar.h"
#if VSHIFT_HAVE_RPCS3_DECRYPT
#include "Crypto/unself.h"
#include "Utilities/File.h"
#endif

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace std_fs = std::filesystem;

#if VSHIFT_HAVE_RPCS3_DECRYPT
extern "C" void vshift_rpcs3_headless_anchor();
#endif

namespace {

bool read_file(const std_fs::path& path, std::vector<std::uint8_t>& bytes) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) return false;
    const auto size = input.tellg();
    if (size <= 0) return false;
    bytes.resize(static_cast<std::size_t>(size));
    input.seekg(0);
    return static_cast<bool>(input.read(reinterpret_cast<char*>(bytes.data()), size));
}

bool write_entry(const std_fs::path& root, const vshift::firmware::Ps3TarEntry& entry,
                 std::span<const std::uint8_t> tar, std::size_t& decrypted_files) {
    if (!entry.regular_file || entry.name.empty() || entry.name.find("..") != std::string::npos) return true;
    const std_fs::path relative = std_fs::path(entry.name).lexically_normal();
    if (relative.is_absolute() || relative.string().starts_with("..")) return false;
    if (entry.data_offset > tar.size() || entry.data_length > tar.size() - entry.data_offset) return false;
    const std_fs::path destination = (root / relative).lexically_normal();
    std_fs::create_directories(destination.parent_path());
    const auto begin = tar.begin() + static_cast<std::ptrdiff_t>(entry.data_offset);
    std::vector<std::uint8_t> payload(begin, begin + static_cast<std::ptrdiff_t>(entry.data_length));
#if VSHIFT_HAVE_RPCS3_DECRYPT
    if (payload.size() >= 4 && payload[0] == 'S' && payload[1] == 'C' && payload[2] == 'E' && payload[3] == 0) {
        auto input = fs::make_stream(std::move(payload));
        if (auto elf = decrypt_self(input, nullptr)) {
            payload = elf.to_vector<std::uint8_t>();
            ++decrypted_files;
        }
    }
#endif
    std::ofstream output(destination, std::ios::binary | std::ios::trunc);
    if (!output) return false;
    output.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
    return static_cast<bool>(output);
}

} // namespace

int main(int argc, char** argv) {
#if VSHIFT_HAVE_RPCS3_DECRYPT
    vshift_rpcs3_headless_anchor();
#endif
    if (argc != 3) {
        std::cerr << "usage: vshift_ps3_firmware_extract PS3UPDAT.PUP output-root\n";
        return 2;
    }

    std::vector<std::uint8_t> pup;
    if (!read_file(argv[1], pup)) {
        std::cerr << "could not read PUP\n";
        return 1;
    }
    if (pup.size() < vshift::firmware::kPs3PupHeaderSize) {
        std::cerr << "PUP is too small\n";
        return 1;
    }
    const auto header_length = [&] {
        std::uint64_t value = 0;
        for (std::size_t i = 0; i < 8; ++i) value = (value << 8) | pup[0x20 + i];
        return value;
    }();
    const auto parsed = vshift::firmware::ParsePs3PupHeaders(
        std::span<const std::uint8_t>(pup.data(), static_cast<std::size_t>(header_length)), pup.size());
    if (!parsed.ok()) {
        std::cerr << parsed.error << '\n';
        return 1;
    }
    const auto update = std::find_if(parsed.entries.begin(), parsed.entries.end(),
        [](const auto& entry) { return entry.entry_id == 0x300; });
    if (update == parsed.entries.end()) {
        std::cerr << "update TAR entry (0x300) is missing\n";
        return 1;
    }
    const auto update_tar = std::span<const std::uint8_t>(
        pup.data() + static_cast<std::size_t>(update->data_offset), static_cast<std::size_t>(update->data_length));
    const auto outer = vshift::firmware::ParsePs3Tar(update_tar);
    if (!outer.ok()) {
        std::cerr << outer.error << '\n';
        return 1;
    }
    const std_fs::path root = std_fs::path(argv[2]);
    std_fs::create_directories(root);
    std::size_t files = 0;
    std::size_t decrypted_files = 0;
    std::size_t packages = 0;
    for (const auto& package_entry : outer.entries) {
        if (!package_entry.regular_file || package_entry.name.rfind("dev_flash", 0) != 0) continue;
        if (package_entry.data_offset > update_tar.size() ||
            package_entry.data_length > update_tar.size() - package_entry.data_offset) continue;
        const auto package = vshift::firmware::DecryptPs3ScePackage(update_tar.subspan(
            static_cast<std::size_t>(package_entry.data_offset), static_cast<std::size_t>(package_entry.data_length)));
        if (!package.ok()) {
            std::cerr << "skip " << package_entry.name << ": " << package.error << '\n';
            continue;
        }
        ++packages;
        for (const auto& section : package.sections) {
            const auto tar = vshift::firmware::ParsePs3Tar(section.bytes);
            if (!tar.ok()) continue;
            for (const auto& entry : tar.entries) {
                if (entry.regular_file && entry.name.starts_with("dev_flash")) {
                    if (!write_entry(root, entry, section.bytes, decrypted_files)) {
                        std::cerr << "failed to write " << entry.name << '\n';
                        return 1;
                    }
                    ++files;
                }
            }
        }
    }
    std::cout << "packages=" << packages << " extracted_files=" << files
              << " decrypted_files=" << decrypted_files << " root=" << root.string() << '\n';
    return files == 0 ? 1 : 0;
}
