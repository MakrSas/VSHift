#include "core/firmware/ps3_pup.h"

#include <cassert>
#include <cstdint>
#include <vector>

namespace {

void WriteU64BE(std::vector<std::uint8_t>& bytes,
                std::size_t offset,
                std::uint64_t value) {
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        const auto shift = (sizeof(value) - index - 1) * 8;
        bytes[offset + index] =
            static_cast<std::uint8_t>(value >> shift);
    }
}

std::vector<std::uint8_t> BuildHeader(std::uint64_t file_size) {
    constexpr std::uint64_t file_count = 2;
    constexpr std::size_t header_size = 0xb0;
    std::vector<std::uint8_t> bytes(header_size, 0);
    bytes[0x00] = 'S';
    bytes[0x01] = 'C';
    bytes[0x02] = 'E';
    bytes[0x03] = 'U';
    bytes[0x04] = 'F';
    WriteU64BE(bytes, 0x08, 1);
    WriteU64BE(bytes, 0x10, 0x10b94);
    WriteU64BE(bytes, 0x18, file_count);
    WriteU64BE(bytes, 0x20, header_size);
    WriteU64BE(bytes, 0x28, file_size - header_size);

    WriteU64BE(bytes, 0x30, 0x100);
    WriteU64BE(bytes, 0x38, header_size);
    WriteU64BE(bytes, 0x40, 5);
    WriteU64BE(bytes, 0x50, 0x300);
    WriteU64BE(bytes, 0x58, header_size + 5);
    WriteU64BE(bytes, 0x60, file_size - header_size - 5);
    return bytes;
}

} // namespace

int main() {
    constexpr std::uint64_t file_size = 0x500;
    const auto header = BuildHeader(file_size);
    const auto parsed = vshift::firmware::ParsePs3PupHeaders(
        header, file_size);
    assert(parsed.ok());
    assert(parsed.header.package_version == 1);
    assert(parsed.header.image_version == 0x10b94);
    assert(parsed.header.file_count == 2);
    assert(parsed.entries.size() == 2);
    assert(parsed.entries[0].entry_id == 0x100);
    assert(parsed.entries[0].data_offset == 0xb0);
    assert(parsed.entries[0].data_length == 5);
    assert(parsed.entries[1].entry_id == 0x300);

    auto invalid = header;
    invalid[0] = 'X';
    assert(!vshift::firmware::ParsePs3PupHeaders(invalid, file_size).ok());

    invalid = header;
    WriteU64BE(invalid, 0x58, file_size - 2);
    WriteU64BE(invalid, 0x60, 3);
    assert(!vshift::firmware::ParsePs3PupHeaders(invalid, file_size).ok());

    invalid = header;
    WriteU64BE(invalid, 0x50, 0x100);
    assert(!vshift::firmware::ParsePs3PupHeaders(invalid, file_size).ok());

    invalid = header;
    invalid.resize(0x30);
    assert(!vshift::firmware::ParsePs3PupHeaders(invalid, file_size).ok());
    return 0;
}
