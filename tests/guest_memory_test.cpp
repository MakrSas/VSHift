#include "core/memory/guest_memory.h"

#include <array>
#include <cassert>
#include <cstdint>

int main() {
    vshift::memory::GuestMemory memory;
    assert(memory.Map({0x1000, 0x100, vshift::memory::kPermissionRead |
                       vshift::memory::kPermissionWrite})
               .ok());
    assert(!memory.Map({0x1080, 0x100, vshift::memory::kPermissionRead})
                .ok());

    const std::array<std::uint8_t, 3> bytes = {1, 2, 3};
    assert(memory.Write(0x1010, bytes).ok());

    std::array<std::uint8_t, 3> output = {};
    assert(memory.Read(0x1010, output).ok());
    assert(output == bytes);

    assert(!memory.Read(0x10ff, output).ok());
    assert(!memory.Write(0x10ff, bytes).ok());

    assert(memory.Map({0x2000, 0x100, vshift::memory::kPermissionExecute})
               .ok());
    assert(!memory.Write(0x2000, bytes).ok());
    assert(memory.Initialize(0x2000, bytes).ok());
    assert(!memory.Read(0x2000, output).ok());
    return 0;
}
