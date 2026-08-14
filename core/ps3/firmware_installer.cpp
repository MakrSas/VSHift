#include "core/ps3/firmware_installer.h"

#if defined(VSHIFT_HAS_RPCS3_CORE)

#include "Crypto/key_vault.h"
#include "Crypto/unself.h"
#include "Emu/VFS.h"
#include "Loader/PUP.h"
#include "Loader/TAR.h"
#include "Utilities/File.h"

#include <algorithm>
#include <string_view>
#include <utility>

namespace {

std::string EnsureTrailingSeparator(std::filesystem::path path) {
    std::string value = path.lexically_normal().string();
    if (value.empty() || (value.back() != '/' && value.back() != '\\')) {
        value.push_back('/');
    }
    return value;
}

std::string PupErrorText(pup_error error) {
    switch (error) {
    case pup_error::ok:
        return {};
    case pup_error::header_read:
        return "PS3 PUP header is truncated";
    case pup_error::header_magic:
        return "selected file is not a PS3 PUP";
    case pup_error::header_file_count:
        return "PS3 PUP file table is invalid";
    case pup_error::expected_size:
        return "PS3 PUP is incomplete";
    case pup_error::file_entries:
        return "PS3 PUP contains an invalid file entry";
    case pup_error::hash_mismatch:
        return "PS3 PUP hash validation failed";
    case pup_error::stream:
        return "PS3 PUP could not be read";
    }
    return "PS3 PUP validation failed";
}

bool IsSafeArchivePath(std::string_view path) {
    if (path.empty() || path.front() == '/' || path.front() == '\\' ||
        (path.size() >= 2 && path[1] == ':')) {
        return false;
    }

    std::size_t component_start = 0;
    while (component_start < path.size()) {
        const auto separator = path.find_first_of("/\\", component_start);
        const auto component_end = separator == std::string_view::npos
                                       ? path.size()
                                       : separator;
        const auto component = path.substr(component_start,
                                            component_end - component_start);
        if (component.empty() || component == "." || component == "..") {
            return false;
        }
        if (separator == std::string_view::npos) {
            break;
        }
        component_start = separator + 1;
    }
    return true;
}

} // namespace

#endif

namespace vshift::ps3 {

FirmwareInstallReport Rpcs3FirmwareInstaller::Install(
    const std::filesystem::path& pup_path,
    const std::filesystem::path& emulator_directory) const {
    FirmwareInstallReport report;
    report.emulator_directory = emulator_directory;
    report.dev_flash_directory = emulator_directory / "dev_flash";

#if defined(VSHIFT_HAS_RPCS3_CORE)
    constexpr std::uintmax_t kMaximumPupBytes = 1ull * 1024ull * 1024ull * 1024ull;
    std::error_code file_size_error;
    const auto pup_size = std::filesystem::file_size(pup_path, file_size_error);
    if (file_size_error) {
        report.error = "cannot inspect PS3UPDAT.PUP size";
        return report;
    }
    if (pup_size > kMaximumPupBytes) {
        report.error = "PS3UPDAT.PUP exceeds the mobile installer size limit";
        return report;
    }

    const auto emulator_root = EnsureTrailingSeparator(emulator_directory);
    const auto dev_flash = EnsureTrailingSeparator(report.dev_flash_directory);

    if (!fs::create_path(emulator_root)) {
        report.error = "cannot create RPCS3 emulator directory";
        return report;
    }
    if (!fs::create_path(dev_flash)) {
        report.error = "cannot create PS3 dev_flash directory";
        return report;
    }

    fs::file pup_file(pup_path.string(), fs::read);
    if (!pup_file) {
        report.error = "cannot open PS3UPDAT.PUP";
        return report;
    }

    pup_object pup(std::move(pup_file));
    const auto validation_error = static_cast<pup_error>(pup);
    if (validation_error != pup_error::ok) {
        report.error = PupErrorText(validation_error);
        if (!pup.get_formatted_error().empty()) {
            report.error += ": ";
            report.error += pup.get_formatted_error();
        }
        return report;
    }

    if (fs::file version_file = pup.get_file(0x100)) {
        report.firmware_version = version_file.to_string();
        if (const auto newline = report.firmware_version.find('\n');
            newline != std::string::npos) {
            report.firmware_version.erase(newline);
        }
    }
    if (report.firmware_version.empty()) {
        report.error = "PS3 PUP has no firmware version entry";
        return report;
    }

    fs::file update_files_file = pup.get_file(0x300);
    if (!update_files_file) {
        report.error = "PS3 PUP has no installation package database";
        return report;
    }

    tar_object update_files(update_files_file);
    auto update_filenames = update_files.get_filenames();
    update_filenames.erase(
        std::remove_if(update_filenames.begin(), update_filenames.end(),
                       [](const std::string& name) {
                           return !name.starts_with("dev_flash_");
                       }),
        update_filenames.end());
    if (update_filenames.empty()) {
        report.error = "PS3 PUP has no dev_flash packages";
        return report;
    }

    if (!vfs::mount("/dev_flash", dev_flash)) {
        report.error = "cannot mount dev_flash installation directory";
        return report;
    }

    for (const auto& update_filename : update_filenames) {
        auto update_file_stream = update_files.get_file(update_filename);
        if (!update_file_stream) {
            report.error = "cannot read a dev_flash package from the PUP";
            vfs::unmount("/dev_flash");
            return report;
        }

        // Force the stream-backed package to materialize before handing it to
        // the upstream SELF decrypter, matching RPCS3's installer path.
        if (update_file_stream->m_file_handler) {
            update_file_stream->m_file_handler->handle_file_op(
                *update_file_stream, 0, update_file_stream->get_size(umax), nullptr);
        }

        fs::file update_file = fs::make_stream(std::move(update_file_stream->data));
        SCEDecrypter self_decrypter(update_file);
        if (!self_decrypter.LoadHeaders() ||
            !self_decrypter.LoadMetadata(SCEPKG_ERK, SCEPKG_RIV) ||
            !self_decrypter.DecryptData()) {
            report.error = "RPCS3 could not decrypt a PS3 dev_flash package";
            vfs::unmount("/dev_flash");
            return report;
        }

        const auto package_files = self_decrypter.MakeFile();
        if (package_files.size() < 3 || !package_files[2]) {
            report.error = "PS3 dev_flash package has an invalid TAR payload";
            vfs::unmount("/dev_flash");
            return report;
        }

        tar_object dev_flash_tar(package_files[2]);
        const auto dev_flash_entries = dev_flash_tar.get_filenames();
        if (std::ranges::any_of(dev_flash_entries,
                                [](const std::string& name) {
                                    return !IsSafeArchivePath(name);
                                })) {
            report.error = "PS3 dev_flash TAR contains an unsafe path";
            vfs::unmount("/dev_flash");
            return report;
        }
        if (!dev_flash_tar.extract()) {
            report.error = "RPCS3 could not extract a PS3 dev_flash package";
            vfs::unmount("/dev_flash");
            return report;
        }
        ++report.package_count;
    }

    vfs::unmount("/dev_flash");
    return report;
#else
    report.error = "RPCS3 core was not enabled in this build";
    return report;
#endif
}

} // namespace vshift::ps3
