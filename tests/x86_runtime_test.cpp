#include "core/cpu/x86_runtime.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <span>

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

    assert(memory.Map({0x600000, 0x1000,
                       vshift::memory::kPermissionRead |
                           vshift::memory::kPermissionWrite})
               .ok());
    const std::uint64_t tlsValue = 99;
    assert(memory.Initialize(
                       0x600000,
                       std::span<const std::uint8_t>(
                           reinterpret_cast<const std::uint8_t*>(&tlsValue),
                           sizeof(tlsValue)))
               .ok());
    const std::array<std::uint8_t, 10> fsProgram = {
        0x64, 0x48, 0x8b, 0x04, 0x25, 0x00, 0x00, 0x00, 0x00,
        0xc3, // mov rax, qword ptr fs:[0]; ret
    };
    assert(memory.Initialize(0x401100, fsProgram).ok());
    const vshift::cpu::GuestCpuConfig fsConfig{
        .max_instructions = 1'000'000,
        .stack_top = 0x7fff'ff80'0000ull,
        .stack_size = 0x20'0000,
        .fs_base = 0x600000,
    };
    const auto fsResult = vshift::cpu::RunGuest(
        memory, 0x401100, fsConfig);
    assert(fsResult.ok());
    assert(fsResult.registers.rax() == tlsValue);
    return 0;
}
