#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace vshift::crypto {

// Small, dependency-free AES boundary used by the PS3 SCE package reader.
// It intentionally exposes only the modes needed for package metadata and
// section data; platform UI and Android crypto backends are not involved.
struct AesResult final {
    std::string error;

    bool ok() const noexcept { return error.empty(); }
};

AesResult AesCbcDecrypt(std::span<const std::uint8_t> key,
                        std::span<const std::uint8_t> iv,
                        std::span<const std::uint8_t> input,
                        std::vector<std::uint8_t>& output);

AesResult AesCtrCrypt(std::span<const std::uint8_t> key,
                      std::span<const std::uint8_t> counter,
                      std::span<const std::uint8_t> input,
                      std::vector<std::uint8_t>& output);

} // namespace vshift::crypto
