#pragma once

#include "core/loader/ps3_sce.h"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace vshift::firmware {

struct Ps3PackageSection final {
    std::uint64_t data_offset = 0;
    std::uint64_t data_length = 0;
    std::uint32_t type = 0;
    std::uint32_t program_index = 0;
    std::uint32_t encrypted = 0;
    std::uint32_t compressed = 0;
    std::vector<std::uint8_t> bytes;
};

struct Ps3PackageParseResult final {
    loader::Ps3SceHeader sce_header;
    std::vector<Ps3PackageSection> sections;
    std::string error;

    bool ok() const noexcept { return error.empty(); }
};

struct Ps3PackageKeys final {
    std::array<std::uint8_t, 32> metadata_key{};
    std::array<std::uint8_t, 16> metadata_iv{};
};

// The default package key is the public PS3 firmware package key used by
// RPCS3. It is kept isolated from the runtime so the package reader can later
// accept a user-supplied key set without changing its parsing boundary.
const Ps3PackageKeys& DefaultPs3PackageKeys() noexcept;

// Decrypts the package metadata and data sections using the keys contained in
// the package metadata. This covers the plaintext-metadata dev_flash package
// format used by the user-provided PS3 PUP; packages with externally encrypted
// metadata stop with an explicit error instead of guessing a key.
Ps3PackageParseResult DecryptPs3ScePackage(
    std::span<const std::uint8_t> package_bytes,
    const Ps3PackageKeys& keys);

Ps3PackageParseResult DecryptPs3ScePackage(
    std::span<const std::uint8_t> package_bytes);

} // namespace vshift::firmware
