#include "core/loader/ps3_self.h"

#include "core/crypto/aes.h"

#include <algorithm>
#include <array>
#include <limits>
#include <zlib.h>

namespace vshift::loader {

namespace {

constexpr std::size_t kSelfFixedHeaderSize = 0x20;
constexpr std::size_t kExtensionHeaderSize = 0x50;
constexpr std::size_t kProgramIdentificationSize = 0x20;
constexpr std::size_t kSegmentExtensionSize = 0x20;
constexpr std::size_t kMetadataInfoSize = 0x40;
constexpr std::size_t kMetadataHeaderSize = 0x20;
constexpr std::size_t kMetadataSectionSize = 0x30;
constexpr std::uint32_t kElfMagic = 0x7f454c46;
constexpr std::uint32_t kPtLoad = 1;
constexpr std::uint16_t kPpc64Machine = 0x15;
constexpr std::uint8_t kElfClass32 = 1;
constexpr std::uint8_t kElfClass64 = 2;
constexpr std::uint8_t kElfDataBigEndian = 2;
constexpr std::uint32_t kMaximumHeaders = 4096;

bool AddWouldOverflow(std::uint64_t left, std::uint64_t right) noexcept {
    return right > std::numeric_limits<std::uint64_t>::max() - left;
}

bool RangeWithin(std::size_t total,
                 std::uint64_t offset,
                 std::uint64_t size) noexcept {
    return offset <= total && !AddWouldOverflow(offset, size) &&
           offset + size <= total;
}

std::uint16_t ReadU16BE(const std::uint8_t* bytes) noexcept {
    return static_cast<std::uint16_t>(bytes[0] << 8) |
           static_cast<std::uint16_t>(bytes[1]);
}

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

bool AllZero(std::span<const std::uint8_t> bytes) noexcept {
    return std::all_of(bytes.begin(), bytes.end(),
                       [](std::uint8_t value) { return value == 0; });
}

bool DecompressZlib(std::span<const std::uint8_t> input,
                    std::vector<std::uint8_t>& output,
                    std::string& error) {
    z_stream stream{};
    stream.next_in = const_cast<Bytef*>(input.data());
    stream.avail_in = static_cast<uInt>(input.size());
    if (inflateInit(&stream) != Z_OK) {
        error = "PS3 SELF zlib initialization failed";
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
            error = "PS3 SELF decompressed section is too large";
            output.clear();
            return false;
        }
    }
    inflateEnd(&stream);
    if (status != Z_STREAM_END) {
        error = "PS3 SELF zlib decompression failed";
        output.clear();
        return false;
    }
    return true;
}

Ps3SelfParseResult Fail(Ps3SelfParseResult result, const char* message) {
    result.image = {};
    result.error = message;
    return result;
}

} // namespace

const Ps3SelfKeys& DefaultPs3VshSelfKeys() noexcept {
    // RPCS3 Crypto/key_vault.cpp, revision
    // 12b1efc26601b20f85f4d040582e7473388bb553, KEY_APP revision 0x1c.
    // This independent loader only uses the public PS3 VSH metadata key.
    static const Ps3SelfKeys keys{
        {0xcf, 0xf0, 0x25, 0x37, 0x5b, 0xa0, 0x07, 0x92,
         0x26, 0xbe, 0x01, 0xf4, 0xa3, 0x1f, 0x34, 0x6d,
         0x79, 0xf6, 0x2c, 0xfb, 0x64, 0x3c, 0xa9, 0x10,
         0xe1, 0x6c, 0xf6, 0x0b, 0xd9, 0x09, 0x27, 0x52},
        {0xfd, 0x40, 0x66, 0x4e, 0x2e, 0xbb, 0xa0, 0x1b,
         0xf3, 0x59, 0xb0, 0xdc, 0xdf, 0x54, 0x3d, 0xa4},
    };
    return keys;
}

Ps3SelfParseResult ParsePs3Self(std::span<const std::uint8_t> self_bytes,
                                const Ps3SelfKeys& keys) {
    Ps3SelfParseResult result;
    const auto sce = ParsePs3SceHeader(self_bytes, self_bytes.size());
    if (!sce.ok()) {
        result.error = sce.error;
        return result;
    }
    result.image.sce_header = sce.header;
    if (!RangeWithin(self_bytes.size(), sce.header.metadata_offset +
                                         kSelfFixedHeaderSize,
                     kMetadataInfoSize) ||
        sce.header.metadata_offset + kSelfFixedHeaderSize +
                kMetadataInfoSize > sce.header.header_size) {
        return Fail(std::move(result), "PS3 SELF metadata info range is invalid");
    }
    if (!RangeWithin(self_bytes.size(), kSelfFixedHeaderSize,
                     kExtensionHeaderSize)) {
        return Fail(std::move(result), "PS3 SELF extension header is truncated");
    }

    const auto* bytes = self_bytes.data();
    const auto* extension = bytes + kSelfFixedHeaderSize;
    result.image.extension.version = ReadU64BE(extension + 0x00);
    result.image.extension.program_identification_offset = ReadU64BE(extension + 0x08);
    result.image.extension.elf_header_offset = ReadU64BE(extension + 0x10);
    result.image.extension.program_header_offset = ReadU64BE(extension + 0x18);
    result.image.extension.section_header_offset = ReadU64BE(extension + 0x20);
    result.image.extension.segment_extension_offset = ReadU64BE(extension + 0x28);
    result.image.extension.version_header_offset = ReadU64BE(extension + 0x30);
    result.image.extension.supplemental_header_offset = ReadU64BE(extension + 0x38);
    result.image.extension.supplemental_header_size = ReadU64BE(extension + 0x40);
    result.image.extension.padding = ReadU64BE(extension + 0x48);
    if (result.image.extension.version == 0) {
        return Fail(std::move(result), "PS3 SELF extension version is invalid");
    }

    const auto program_offset = result.image.extension.program_identification_offset;
    if (!RangeWithin(self_bytes.size(), program_offset,
                     kProgramIdentificationSize)) {
        return Fail(std::move(result), "PS3 SELF program identification is truncated");
    }
    const auto* program = bytes + static_cast<std::size_t>(program_offset);
    result.image.program_identification.authority_id = ReadU64BE(program + 0x00);
    result.image.program_identification.vendor_id = ReadU32BE(program + 0x08);
    result.image.program_identification.program_type = ReadU32BE(program + 0x0c);
    result.image.program_identification.sce_version = ReadU64BE(program + 0x10);
    result.image.program_identification.padding = ReadU64BE(program + 0x18);

    const auto elf_offset = result.image.extension.elf_header_offset;
    if (!RangeWithin(self_bytes.size(), elf_offset, 0x20)) {
        return Fail(std::move(result), "PS3 SELF ELF header is truncated");
    }
    const auto* elf = bytes + static_cast<std::size_t>(elf_offset);
    if (ReadU32BE(elf) != kElfMagic) {
        return Fail(std::move(result), "PS3 SELF does not contain an ELF header");
    }
    result.image.elf_class = elf[4];
    result.image.elf_data = elf[5];
    if ((result.image.elf_class != kElfClass32 &&
         result.image.elf_class != kElfClass64) ||
        result.image.elf_data != kElfDataBigEndian) {
        return Fail(std::move(result), "PS3 SELF ELF format is not big-endian 32/64-bit");
    }

    const bool elf64 = result.image.elf_class == kElfClass64;
    const auto ehdr_size = elf64 ? 0x40u : 0x34u;
    const auto phdr_size = elf64 ? 0x38u : 0x20u;
    if (!RangeWithin(self_bytes.size(), elf_offset, ehdr_size)) {
        return Fail(std::move(result), "PS3 SELF ELF header size is invalid");
    }
    result.image.elf_type = ReadU16BE(elf + 0x10);
    result.image.elf_machine = ReadU16BE(elf + 0x12);
    if (result.image.elf_machine != kPpc64Machine) {
        return Fail(std::move(result), "PS3 SELF ELF machine is not PowerPC");
    }
    const auto phentsize = ReadU16BE(elf + (elf64 ? 0x36 : 0x2a));
    const auto phnum = ReadU16BE(elf + (elf64 ? 0x38 : 0x2c));
    if (phentsize != phdr_size || phnum == 0 || phnum > kMaximumHeaders) {
        return Fail(std::move(result), "PS3 SELF ELF program header table is invalid");
    }
    result.image.entry_point = elf64 ? ReadU64BE(elf + 0x18)
                                     : ReadU32BE(elf + 0x18);
    const auto phdr_offset = result.image.extension.program_header_offset;
    const auto phdr_bytes = static_cast<std::uint64_t>(phnum) * phentsize;
    if (!RangeWithin(self_bytes.size(), phdr_offset, phdr_bytes)) {
        return Fail(std::move(result), "PS3 SELF program headers are truncated");
    }
    result.image.program_headers.reserve(phnum);
    for (std::uint16_t index = 0; index < phnum; ++index) {
        const auto* header = bytes + static_cast<std::size_t>(
            phdr_offset + static_cast<std::uint64_t>(index) * phentsize);
        Ps3SelfProgramHeader parsed;
        parsed.type = ReadU32BE(header + 0x00);
        if (elf64) {
            parsed.flags = ReadU32BE(header + 0x04);
            parsed.offset = ReadU64BE(header + 0x08);
            parsed.virtual_address = ReadU64BE(header + 0x10);
            parsed.physical_address = ReadU64BE(header + 0x18);
            parsed.file_size = ReadU64BE(header + 0x20);
            parsed.memory_size = ReadU64BE(header + 0x28);
            parsed.alignment = ReadU64BE(header + 0x30);
        } else {
            parsed.offset = ReadU32BE(header + 0x04);
            parsed.virtual_address = ReadU32BE(header + 0x08);
            parsed.physical_address = ReadU32BE(header + 0x0c);
            parsed.file_size = ReadU32BE(header + 0x10);
            parsed.memory_size = ReadU32BE(header + 0x14);
            parsed.flags = ReadU32BE(header + 0x18);
            parsed.alignment = ReadU32BE(header + 0x1c);
        }
        result.image.program_headers.push_back(parsed);
    }

    const auto segment_offset = result.image.extension.segment_extension_offset;
    const auto segment_bytes = static_cast<std::uint64_t>(phnum) *
                               kSegmentExtensionSize;
    if (!RangeWithin(self_bytes.size(), segment_offset, segment_bytes)) {
        return Fail(std::move(result), "PS3 SELF segment extension table is truncated");
    }
    result.image.segment_extensions.reserve(phnum);
    for (std::uint16_t index = 0; index < phnum; ++index) {
        const auto* segment = bytes + static_cast<std::size_t>(
            segment_offset + static_cast<std::uint64_t>(index) *
                                 kSegmentExtensionSize);
        result.image.segment_extensions.push_back(Ps3SelfSegmentExtension{
            ReadU64BE(segment + 0x00), ReadU64BE(segment + 0x08),
            ReadU32BE(segment + 0x10), ReadU32BE(segment + 0x14),
            ReadU64BE(segment + 0x18)});
    }

    const auto info_offset = static_cast<std::uint64_t>(kSelfFixedHeaderSize) +
                             result.image.sce_header.metadata_offset;
    std::vector<std::uint8_t> metadata_info(
        bytes + static_cast<std::size_t>(info_offset),
        bytes + static_cast<std::size_t>(info_offset + kMetadataInfoSize));
    if (result.image.sce_header.metadata_encrypted()) {
        std::vector<std::uint8_t> decrypted;
        const auto crypto = crypto::AesCbcDecrypt(keys.metadata_key,
                                                   keys.metadata_iv,
                                                   metadata_info, decrypted);
        if (!crypto.ok()) {
            return Fail(std::move(result), crypto.error.c_str());
        }
        metadata_info = std::move(decrypted);
    }
    if (!AllZero(std::span<const std::uint8_t>(metadata_info).subspan(0x10, 0x10)) ||
        !AllZero(std::span<const std::uint8_t>(metadata_info).subspan(0x30, 0x10))) {
        return Fail(std::move(result), "PS3 SELF metadata key/IV padding is invalid");
    }

    const auto metadata_header_offset = info_offset + kMetadataInfoSize;
    const auto metadata_header_size = result.image.sce_header.header_size -
                                      metadata_header_offset;
    if (!RangeWithin(self_bytes.size(), metadata_header_offset,
                     metadata_header_size) || metadata_header_size < kMetadataHeaderSize) {
        return Fail(std::move(result), "PS3 SELF metadata headers are truncated");
    }
    std::vector<std::uint8_t> metadata(
        bytes + static_cast<std::size_t>(metadata_header_offset),
        bytes + static_cast<std::size_t>(result.image.sce_header.header_size));
    std::vector<std::uint8_t> decrypted_metadata;
    const auto metadata_crypto = crypto::AesCtrCrypt(
        std::span<const std::uint8_t>(metadata_info).subspan(0, 0x10),
        std::span<const std::uint8_t>(metadata_info).subspan(0x20, 0x10),
        metadata, decrypted_metadata);
    if (!metadata_crypto.ok()) {
        return Fail(std::move(result), metadata_crypto.error.c_str());
    }

    const auto section_count = ReadU32BE(decrypted_metadata.data() + 0x0c);
    const auto key_count = ReadU32BE(decrypted_metadata.data() + 0x10);
    if (section_count > kMaximumHeaders || key_count > kMaximumHeaders) {
        return Fail(std::move(result), "PS3 SELF metadata counts are unreasonable");
    }
    const auto section_bytes = static_cast<std::uint64_t>(section_count) *
                               kMetadataSectionSize;
    const auto keys_offset = static_cast<std::uint64_t>(kMetadataHeaderSize) +
                             section_bytes;
    const auto keys_bytes = static_cast<std::uint64_t>(key_count) * 16;
    if (!RangeWithin(decrypted_metadata.size(), keys_offset, keys_bytes)) {
        return Fail(std::move(result), "PS3 SELF metadata key table is truncated");
    }

    result.image.sections.reserve(section_count);
    for (std::uint32_t index = 0; index < section_count; ++index) {
        const auto offset = static_cast<std::size_t>(
            kMetadataHeaderSize + static_cast<std::uint64_t>(index) *
                                      kMetadataSectionSize);
        if (offset + kMetadataSectionSize > decrypted_metadata.size()) {
            return Fail(std::move(result), "PS3 SELF metadata section is truncated");
        }
        const auto* section = decrypted_metadata.data() + offset;
        Ps3SelfSection parsed;
        parsed.data_offset = ReadU64BE(section + 0x00);
        parsed.data_size = ReadU64BE(section + 0x08);
        parsed.type = ReadU32BE(section + 0x10);
        parsed.program_index = ReadU32BE(section + 0x14);
        parsed.hashed = ReadU32BE(section + 0x18);
        parsed.sha1_index = ReadU32BE(section + 0x1c);
        parsed.encrypted = ReadU32BE(section + 0x20);
        parsed.key_index = ReadU32BE(section + 0x24);
        parsed.iv_index = ReadU32BE(section + 0x28);
        parsed.compressed = ReadU32BE(section + 0x2c);
        if (!RangeWithin(self_bytes.size(), parsed.data_offset,
                         parsed.data_size)) {
            return Fail(std::move(result), "PS3 SELF section range is invalid");
        }
        std::vector<std::uint8_t> section_bytes_data(
            bytes + static_cast<std::size_t>(parsed.data_offset),
            bytes + static_cast<std::size_t>(parsed.data_offset + parsed.data_size));
        if (parsed.encrypted == 3) {
            if (parsed.key_index >= key_count || parsed.iv_index >= key_count) {
                return Fail(std::move(result), "PS3 SELF section key index is invalid");
            }
            const auto key_offset = static_cast<std::size_t>(
                keys_offset + static_cast<std::uint64_t>(parsed.key_index) * 16);
            const auto iv_offset = static_cast<std::size_t>(
                keys_offset + static_cast<std::uint64_t>(parsed.iv_index) * 16);
            std::vector<std::uint8_t> decrypted_section;
            const auto data_crypto = crypto::AesCtrCrypt(
                std::span<const std::uint8_t>(decrypted_metadata).subspan(key_offset, 16),
                std::span<const std::uint8_t>(decrypted_metadata).subspan(iv_offset, 16),
                section_bytes_data, decrypted_section);
            if (!data_crypto.ok()) {
                return Fail(std::move(result), data_crypto.error.c_str());
            }
            section_bytes_data = std::move(decrypted_section);
        } else if (parsed.encrypted != 0 && parsed.encrypted != 1) {
            return Fail(std::move(result), "PS3 SELF section encryption mode is unsupported");
        }
        if (parsed.compressed == 2) {
            std::vector<std::uint8_t> decompressed;
            if (!DecompressZlib(section_bytes_data, decompressed, result.error)) {
                result.image = {};
                return result;
            }
            section_bytes_data = std::move(decompressed);
        } else if (parsed.compressed != 1 && parsed.compressed != 0) {
            return Fail(std::move(result), "PS3 SELF section compression mode is unsupported");
        }
        parsed.bytes = std::move(section_bytes_data);
        result.image.sections.push_back(std::move(parsed));
    }
    return result;
}

Ps3SelfLoadResult LoadPs3SelfIntoMemory(const Ps3SelfImage& image,
                                        memory::GuestMemory& memory) {
    Ps3SelfLoadResult result;
    result.entry_point = image.entry_point;
    for (std::size_t index = 0; index < image.program_headers.size(); ++index) {
        const auto& program = image.program_headers[index];
        if (program.type != kPtLoad || program.memory_size == 0) {
            continue;
        }
        if (program.file_size > program.memory_size) {
            result.error = "PS3 SELF PT_LOAD file size exceeds memory size";
            return result;
        }
        const Ps3SelfSection* section = nullptr;
        for (const auto& candidate : image.sections) {
            if (candidate.type == 2 && candidate.program_index == index) {
                section = &candidate;
                break;
            }
        }
        if (program.file_size != 0 && section == nullptr) {
            result.error = "PS3 SELF PT_LOAD has no decrypted data section";
            return result;
        }
        if (section != nullptr && section->bytes.size() < program.file_size) {
            result.error = "PS3 SELF decrypted data section is shorter than PT_LOAD";
            return result;
        }
        std::uint32_t permissions = memory::kPermissionRead;
        if ((program.flags & 0x2u) != 0) permissions |= memory::kPermissionWrite;
        if ((program.flags & 0x1u) != 0) permissions |= memory::kPermissionExecute;
        const auto mapped = memory.Map({program.virtual_address,
                                        program.memory_size,
                                        permissions});
        if (!mapped.ok()) {
            result.error = mapped.error;
            return result;
        }
        if (program.file_size != 0) {
            const auto initialized = memory.Initialize(
                program.virtual_address,
                std::span<const std::uint8_t>(section->bytes).subspan(
                    0, static_cast<std::size_t>(program.file_size)));
            if (!initialized.ok()) {
                result.error = initialized.error;
                return result;
            }
        }
        ++result.loaded_segments;
    }
    if (result.loaded_segments == 0) {
        result.error = "PS3 SELF has no loadable program segments";
    }
    return result;
}

} // namespace vshift::loader
