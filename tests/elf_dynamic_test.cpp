#include "core/loader/elf.h"
#include "core/loader/elf_dynamic.h"

#include <cassert>
#include <cstdint>
#include <vector>

namespace {

void Write16(std::vector<std::uint8_t>& bytes,
             std::size_t offset,
             std::uint16_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void Write32(std::vector<std::uint8_t>& bytes,
             std::size_t offset,
             std::uint32_t value) {
    for (std::size_t index = 0; index < 4; ++index) {
        bytes[offset + index] = static_cast<std::uint8_t>(value >> (index * 8));
    }
}

void Write64(std::vector<std::uint8_t>& bytes,
             std::size_t offset,
             std::uint64_t value) {
    for (std::size_t index = 0; index < 8; ++index) {
        bytes[offset + index] = static_cast<std::uint8_t>(value >> (index * 8));
    }
}

} // namespace

int main() {
    std::vector<std::uint8_t> bytes(0x1400, 0);
    bytes[0] = 0x7f;
    bytes[1] = 'E';
    bytes[2] = 'L';
    bytes[3] = 'F';
    bytes[4] = 2;
    bytes[5] = 1;
    bytes[6] = 1;
    Write16(bytes, 0x10, 3);
    Write16(bytes, 0x12, vshift::loader::kElfMachineX86_64);
    Write32(bytes, 0x14, 1);
    Write64(bytes, 0x18, 0x401000);
    Write64(bytes, 0x20, 0x40);
    Write16(bytes, 0x34, 0x40);
    Write16(bytes, 0x36, 0x38);
    Write16(bytes, 0x38, 2);

    Write32(bytes, 0x40, vshift::loader::kElfProgramLoad);
    Write32(bytes, 0x44, 6);
    Write64(bytes, 0x48, 0x1000);
    Write64(bytes, 0x50, 0x401000);
    Write64(bytes, 0x60, 0x400);
    Write64(bytes, 0x68, 0x1000);
    Write64(bytes, 0x70, 0x1000);

    Write32(bytes, 0x78, vshift::loader::kElfProgramDynamic);
    Write64(bytes, 0x80, 0x1200);
    Write64(bytes, 0x88, 0x401200);
    Write64(bytes, 0x98, 0x60);
    Write64(bytes, 0xa0, 0x60);

    const auto parsed = vshift::loader::ParseElf64Headers(bytes, bytes.size());
    assert(parsed.ok());

    // PT_LOAD file offset 0x1000 maps guest 0x401000, so the dynamic table
    // at guest 0x401200 is at file offset 0x1200.
    Write64(bytes, 0x1200, 5);
    Write64(bytes, 0x1208, 0x401300);
    Write64(bytes, 0x1210, 10);
    Write64(bytes, 0x1218, 17);
    Write64(bytes, 0x1220, 1);
    Write64(bytes, 0x1228, 1);
    Write64(bytes, 0x1230, 0);

    bytes[0x1300] = 0;
    bytes[0x1301] = 'l';
    bytes[0x1302] = 'i';
    bytes[0x1303] = 'b';
    bytes[0x1304] = 'S';
    bytes[0x1305] = 'c';
    bytes[0x1306] = 'e';
    bytes[0x1307] = 'L';
    bytes[0x1308] = 'i';
    bytes[0x1309] = 'b';
    bytes[0x130a] = 'c';
    bytes[0x130b] = '.';
    bytes[0x130c] = 's';
    bytes[0x130d] = 'p';
    bytes[0x130e] = 'r';
    bytes[0x130f] = 'x';
    bytes[0x1310] = 0;

    const auto dynamic = vshift::loader::ParseElf64Dynamic(parsed, bytes);
    assert(dynamic.ok());
    assert(dynamic.present);
    assert(dynamic.needed_libraries.size() == 1);
    assert(dynamic.needed_libraries.front() == "libSceLibc.sprx");
    return 0;
}
