#include "core/cpu/x86_runtime.h"

#include <array>
#include <cassert>

int main() {
    vshift::memory::GuestMemory memory;
    const auto mapped = memory.Map({
        0x401000,
        0x1000,
        vshift::memory::kPermissionRead |
            vshift::memory::kPermissionExecute,
    });
    assert(mapped.ok());
    const std::array<std::uint8_t, 15> program = {
        0x48, 0x83, 0xec, 0x08,       // sub rsp, 8
        0xb8, 0x2a, 0x00, 0x00, 0x00, // mov eax, 42
        0x48, 0x83, 0xc4, 0x08,       // add rsp, 8
        0xc3,                         // ret
    };
    assert(memory.Initialize(0x401000, program).ok());
    const auto result = vshift::cpu::RunGuest(memory, 0x401000);
    assert(result.ok());
    assert(result.registers.rax() == 42);
    assert(result.instructions == 4);
    return 0;
}
