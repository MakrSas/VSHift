#include "core/firmware/slb2.h"

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

namespace {

void WriteU32LE(std::vector<std::uint8_t>& bytes,
                std::size_t offset,
                std::uint32_t value) {
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        bytes[offset + index] =
            static_cast<std::uint8_t>((value >> (index * 8)) & 0xFFu);
    }
}

void WriteName(std::vector<std::uint8_t>& bytes,
               std::size_t offset,
               const std::string& name) {
    for (std::size_t index = 0; index < name.size(); ++index) {
        bytes[offset + index] = static_cast<std::uint8_t>(name[index]);
    }
}

} // namespace

int main() {
    constexpr std::uint32_t kEntryCount = 2;
    std::vector<std::uint8_t> table(
        vshift::firmware::kSlb2HeaderSize +
            kEntryCount * vshift::firmware::kSlb2EntrySize,
        0);
    table[0] = 'S';
    table[1] = 'L';
    table[2] = 'B';
    table[3] = '2';
    WriteU32LE(table, 0x04, 1);
    WriteU32LE(table, 0x08, 0xA5);
    WriteU32LE(table, 0x0C, kEntryCount);
    WriteU32LE(table, 0x10, 0x80);

    const auto first = vshift::firmware::kSlb2HeaderSize;
    WriteU32LE(table, first + 0x00, 4);
    WriteU32LE(table, first + 0x04, 0x1234);
    WriteName(table, first + 0x10, "system_b");

    const auto second = first + vshift::firmware::kSlb2EntrySize;
    WriteU32LE(table, second + 0x00, 20);
    WriteU32LE(table, second + 0x04, 0x5678);
    WriteName(table, second + 0x10, "system_ex_b");

    const auto parsed = vshift::firmware::ParseSlb2Table(table, 0x20000);
    assert(parsed.ok());
    assert(parsed.package.version == 1);
    assert(parsed.package.flags == 0xA5);
    assert(parsed.package.entries.size() == 2);
    assert(parsed.package.entries[0].offset == 4 * 0x200);
    assert(parsed.package.entries[0].size == 0x1234);
    assert(parsed.package.entries[0].name == "system_b");
    assert(parsed.package.entries[1].name == "system_ex_b");

    const auto out_of_bounds =
        vshift::firmware::ParseSlb2Table(table, 0x1000);
    assert(!out_of_bounds.ok());

    table[0] = 'X';
    const auto bad_magic = vshift::firmware::ParseSlb2Table(table, 0x20000);
    assert(!bad_magic.ok());
    return 0;
}
