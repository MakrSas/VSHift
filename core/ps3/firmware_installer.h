#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

namespace vshift::ps3 {

struct FirmwareInstallReport final {
    std::filesystem::path emulator_directory;
    std::filesystem::path dev_flash_directory;
    std::string firmware_version;
    std::size_t package_count = 0;
    std::string error;

    bool ok() const noexcept { return error.empty() && package_count != 0; }
};

// Uses RPCS3's PUP, SELF package, and TAR implementations. It installs only
// into the caller-provided emulator directory and never bundles or uploads the
// user-selected PS3UPDAT.PUP.
class Rpcs3FirmwareInstaller final {
public:
    FirmwareInstallReport Install(const std::filesystem::path& pup_path,
                                  const std::filesystem::path& emulator_directory) const;
};

} // namespace vshift::ps3
