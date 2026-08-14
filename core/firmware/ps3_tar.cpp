#include "core/firmware/ps3_tar.h"

#include <algorithm>
#include <limits>

namespace vshift::firmware {

namespace {

constexpr std::size_t kTarBlockSize = 512;

bool AddWouldOverflow(std::uint64_t left, std::uint64_t right) noexcept {
    return right > std::numeric_limits<std::uint64_t>::max() - left;
}

std::string ReadName(const std::uint8_t* header) {
    std::size_t length = 0;
    while (length < 100 && header[length] != 0) ++length;
    return std::string(reinterpret_cast<const char*>(header), length);
}

bool ReadOctal(const std::uint8_t* bytes,
               std::size_t length,
               std::uint64_t& value) {
    value = 0;
    std::size_t index = 0;
    while (index < length && (bytes[index] == ' ' || bytes[index] == '\0')) {
        ++index;
    }
    bool found_digit = false;
    for (; index < length && bytes[index] != '\0' && bytes[index] != ' '; ++index) {
        if (bytes[index] < '0' || bytes[index] > '7') return false;
        found_digit = true;
        const auto digit = static_cast<std::uint64_t>(bytes[index] - '0');
        if (value > (std::numeric_limits<std::uint64_t>::max() - digit) / 8) {
            return false;
        }
        value = value * 8 + digit;
    }
    return found_digit || value == 0;
}

bool IsZeroBlock(std::span<const std::uint8_t> block) {
    return std::all_of(block.begin(), block.end(), [](std::uint8_t value) {
        return value == 0;
    });
}

} // namespace

Ps3TarParseResult ParsePs3Tar(std::span<const std::uint8_t> bytes) {
    Ps3TarParseResult result;
    std::uint64_t offset = 0;
    constexpr std::size_t kMaximumEntries = 65536;
    while (offset + kTarBlockSize <= bytes.size()) {
        const auto* header = bytes.data() + static_cast<std::size_t>(offset);
        const auto block = bytes.subspan(static_cast<std::size_t>(offset),
                                         kTarBlockSize);
        if (IsZeroBlock(block)) {
            return result;
        }
        if (std::string(reinterpret_cast<const char*>(header + 257), 5) !=
                "ustar") {
            result.error = "PS3 TAR header has invalid ustar magic";
            result.entries.clear();
            return result;
        }

        std::uint64_t size = 0;
        if (!ReadOctal(header + 124, 12, size)) {
            result.error = "PS3 TAR entry size is invalid";
            result.entries.clear();
            return result;
        }
        const auto data_offset = offset + kTarBlockSize;
        const auto padded_size = ((size + kTarBlockSize - 1) /
                                  kTarBlockSize) * kTarBlockSize;
        if (AddWouldOverflow(data_offset, padded_size) ||
            data_offset + padded_size > bytes.size()) {
            result.error = "PS3 TAR entry range is outside the archive";
            result.entries.clear();
            return result;
        }
        if (result.entries.size() >= kMaximumEntries) {
            result.error = "PS3 TAR contains too many entries";
            result.entries.clear();
            return result;
        }

        const auto type = header[156];
        result.entries.push_back(Ps3TarEntry{
            ReadName(header), data_offset, size,
            type == 0 || type == '0',
        });
        offset = data_offset + padded_size;
    }
    result.error = "PS3 TAR ends in a truncated header";
    result.entries.clear();
    return result;
}

} // namespace vshift::firmware
