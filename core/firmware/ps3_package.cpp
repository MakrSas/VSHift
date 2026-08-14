#include "core/firmware/ps3_package.h"

#include "core/crypto/aes.h"

#include <algorithm>
#include <array>
#include <limits>
#include <zlib.h>

namespace vshift::firmware {

namespace {

std::uint32_t ReadU32BE(const std::uint8_t* bytes) noexcept {
    return (static_cast<std::uint32_t>(bytes[0]) << 24) |
           (static_cast<std::uint32_t>(bytes[1]) << 16) |
           (static_cast<std::uint32_t>(bytes[2]) << 8) |
           static_cast<std::uint32_t>(bytes[3]);
}

std::uint64_t ReadU64BE(const std::uint8_t* bytes) noexcept {
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        value = (value << 8) | bytes[index];
    }
    return value;
}

bool AddWouldOverflow(std::uint64_t left, std::uint64_t right) noexcept {
    return right > std::numeric_limits<std::uint64_t>::max() - left;
}

bool DecompressZlib(std::span<const std::uint8_t> input,
                    std::vector<std::uint8_t>& output,
                    std::string& error) {
    z_stream stream{};
    stream.next_in = const_cast<Bytef*>(input.data());
    stream.avail_in = static_cast<uInt>(input.size());
    if (inflateInit(&stream) != Z_OK) {
        error = "zlib initialization failed";
        return false;
    }

    output.clear();
    std::array<std::uint8_t, 64 * 1024> chunk{};
    int status = Z_OK;
    while (status == Z_OK) {
        stream.next_out = chunk.data();
        stream.avail_out = static_cast<uInt>(chunk.size());
        status = inflate(&stream, Z_NO_FLUSH);
        const auto produced = chunk.size() - stream.avail_out;
        output.insert(output.end(), chunk.begin(),
                      chunk.begin() + static_cast<std::ptrdiff_t>(produced));
        if (output.size() > 512ull * 1024ull * 1024ull) {
            inflateEnd(&stream);
            error = "zlib output exceeds the PS3 package limit";
            return false;
        }
    }
    inflateEnd(&stream);
    if (status != Z_STREAM_END) {
        error = "zlib decompression failed";
        output.clear();
        return false;
    }
    return true;
}

} // namespace

Ps3PackageParseResult DecryptPs3ScePackage(
    std::span<const std::uint8_t> package_bytes,
    const Ps3PackageKeys& keys) {
    Ps3PackageParseResult result;
    const auto sce = loader::ParsePs3SceHeader(package_bytes,
                                               package_bytes.size());
    if (!sce.ok()) {
        result.error = sce.error;
        return result;
    }
    result.sce_header = sce.header;
    constexpr std::size_t kMetadataInfoSize = 0x40;
    constexpr std::size_t kMetadataHeaderSize = 0x20;
    constexpr std::size_t kMetadataSectionSize = 0x30;
    const auto info_offset = static_cast<std::uint64_t>(0x20) +
                             sce.header.metadata_offset;
    if (AddWouldOverflow(info_offset, kMetadataInfoSize) ||
        info_offset + kMetadataInfoSize > sce.header.header_size ||
        info_offset + kMetadataInfoSize > package_bytes.size()) {
        result.error = "PS3 SCE metadata info range is invalid";
        return result;
    }

    const auto* encrypted_info = package_bytes.data() +
                                 static_cast<std::size_t>(info_offset);
    std::vector<std::uint8_t> info_bytes(
        encrypted_info, encrypted_info + kMetadataInfoSize);
    std::vector<std::uint8_t> decrypted_info;
    if (sce.header.metadata_encrypted()) {
        const auto info_crypto = crypto::AesCbcDecrypt(
            keys.metadata_key, keys.metadata_iv, info_bytes, decrypted_info);
        if (!info_crypto.ok()) {
            result.error = info_crypto.error;
            return result;
        }
    } else {
        decrypted_info = std::move(info_bytes);
    }
    const auto* info = decrypted_info.data();
    if (info[0x10] != 0 || info[0x11] != 0 || info[0x12] != 0 ||
        info[0x13] != 0 || info[0x14] != 0 || info[0x15] != 0 ||
        info[0x16] != 0 || info[0x17] != 0 || info[0x18] != 0 ||
        info[0x19] != 0 || info[0x1a] != 0 || info[0x1b] != 0 ||
        info[0x1c] != 0 || info[0x1d] != 0 || info[0x1e] != 0 ||
        info[0x1f] != 0 || info[0x30] != 0 || info[0x31] != 0 ||
        info[0x32] != 0 || info[0x33] != 0 || info[0x34] != 0 ||
        info[0x35] != 0 || info[0x36] != 0 || info[0x37] != 0 ||
        info[0x38] != 0 || info[0x39] != 0 || info[0x3a] != 0 ||
        info[0x3b] != 0 || info[0x3c] != 0 || info[0x3d] != 0 ||
        info[0x3e] != 0 || info[0x3f] != 0) {
        result.error = "PS3 SCE metadata info padding is invalid";
        return result;
    }

    const auto metadata_offset = info_offset + kMetadataInfoSize;
    if (metadata_offset > sce.header.header_size ||
        sce.header.header_size > package_bytes.size()) {
        result.error = "PS3 SCE metadata header range is invalid";
        return result;
    }
    std::vector<std::uint8_t> metadata(
        package_bytes.begin() + static_cast<std::ptrdiff_t>(metadata_offset),
        package_bytes.begin() + static_cast<std::ptrdiff_t>(
            sce.header.header_size));
    std::vector<std::uint8_t> decrypted_metadata;
    const auto metadata_crypto = crypto::AesCtrCrypt(
        std::span<const std::uint8_t>(info, 16),
        std::span<const std::uint8_t>(info + 0x20, 16), metadata,
        decrypted_metadata);
    if (!metadata_crypto.ok()) {
        result.error = metadata_crypto.error;
        return result;
    }
    if (decrypted_metadata.size() < kMetadataHeaderSize) {
        result.error = "PS3 SCE metadata header is truncated";
        return result;
    }

    const auto section_count = ReadU32BE(decrypted_metadata.data() + 0x0c);
    const auto key_count = ReadU32BE(decrypted_metadata.data() + 0x10);
    constexpr std::uint32_t kMaximumSections = 4096;
    constexpr std::uint32_t kMaximumKeys = 4096;
    if (section_count > kMaximumSections || key_count > kMaximumKeys) {
        result.error = "PS3 SCE metadata counts are unreasonable";
        return result;
    }
    const auto section_bytes = static_cast<std::uint64_t>(section_count) *
                               kMetadataSectionSize;
    const auto keys_offset = static_cast<std::uint64_t>(kMetadataHeaderSize) +
                             section_bytes;
    const auto keys_bytes = static_cast<std::uint64_t>(key_count) * 16;
    if (AddWouldOverflow(keys_offset, keys_bytes) ||
        keys_offset + keys_bytes > decrypted_metadata.size()) {
        result.error = "PS3 SCE metadata key table is truncated";
        return result;
    }

    result.sections.reserve(section_count);
    for (std::uint32_t index = 0; index < section_count; ++index) {
        const auto offset = static_cast<std::size_t>(
            kMetadataHeaderSize + static_cast<std::uint64_t>(index) *
                                      kMetadataSectionSize);
        const auto* section = decrypted_metadata.data() + offset;
        Ps3PackageSection parsed;
        parsed.data_offset = ReadU64BE(section + 0x00);
        parsed.data_length = ReadU64BE(section + 0x08);
        parsed.type = ReadU32BE(section + 0x10);
        parsed.program_index = ReadU32BE(section + 0x14);
        parsed.encrypted = ReadU32BE(section + 0x20);
        const auto key_index = ReadU32BE(section + 0x24);
        const auto iv_index = ReadU32BE(section + 0x28);
        parsed.compressed = ReadU32BE(section + 0x2c);
        if (AddWouldOverflow(parsed.data_offset, parsed.data_length) ||
            parsed.data_offset + parsed.data_length > package_bytes.size()) {
            result.error = "PS3 SCE section range is invalid";
            result.sections.clear();
            return result;
        }
        if (parsed.encrypted == 3 &&
            (key_index >= key_count || iv_index >= key_count)) {
            result.error = "PS3 SCE section key index is invalid";
            result.sections.clear();
            return result;
        }

        std::vector<std::uint8_t> bytes(
            package_bytes.begin() + static_cast<std::ptrdiff_t>(parsed.data_offset),
            package_bytes.begin() + static_cast<std::ptrdiff_t>(
                parsed.data_offset + parsed.data_length));
        if (parsed.encrypted == 3) {
            const auto* section_key = decrypted_metadata.data() +
                static_cast<std::size_t>(keys_offset + key_index * 16);
            const auto* section_iv = decrypted_metadata.data() +
                static_cast<std::size_t>(keys_offset + iv_index * 16);
            std::vector<std::uint8_t> decrypted;
            const auto data_crypto = crypto::AesCtrCrypt(
                std::span<const std::uint8_t>(section_key, 16),
                std::span<const std::uint8_t>(section_iv, 16), bytes,
                decrypted);
            if (!data_crypto.ok()) {
                result.error = data_crypto.error;
                result.sections.clear();
                return result;
            }
            bytes = std::move(decrypted);
        }
        if (parsed.compressed == 2) {
            std::vector<std::uint8_t> decompressed;
            if (!DecompressZlib(bytes, decompressed, result.error)) {
                result.sections.clear();
                return result;
            }
            bytes = std::move(decompressed);
        }
        parsed.bytes = std::move(bytes);
        result.sections.push_back(std::move(parsed));
    }
    return result;
}

const Ps3PackageKeys& DefaultPs3PackageKeys() noexcept {
    // RPCS3 Crypto/key_vault.h, revision 12b1efc26601b20f85f4d040582e7473388bb553,
    // SCEPKG_ERK and SCEPKG_RIV. The surrounding reader is an independent
    // adaptation limited to PS3 firmware package metadata and data sections.
    static const Ps3PackageKeys keys{
        {0xa9, 0x78, 0x18, 0xbd, 0x19, 0x3a, 0x67, 0xa1,
         0x6f, 0xe8, 0x3a, 0x85, 0x5e, 0x1b, 0xe9, 0xfb,
         0x56, 0x40, 0x93, 0x8d, 0x4d, 0xbc, 0xb2, 0xcb,
         0x52, 0xc5, 0xa2, 0xf8, 0xb0, 0x2b, 0x10, 0x31},
        {0x4a, 0xce, 0xf0, 0x12, 0x24, 0xfb, 0xee, 0xdf,
         0x82, 0x45, 0xf8, 0xff, 0x10, 0x21, 0x1e, 0x6e},
    };
    return keys;
}

Ps3PackageParseResult DecryptPs3ScePackage(
    std::span<const std::uint8_t> package_bytes) {
    return DecryptPs3ScePackage(package_bytes, DefaultPs3PackageKeys());
}

} // namespace vshift::firmware
