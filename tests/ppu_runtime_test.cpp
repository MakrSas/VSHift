#include "core/cpu/ppu_runtime.h"

#include <array>
#include <cassert>
#include <cstdint>

namespace {

std::uint32_t Addi(unsigned rt, unsigned ra, std::int16_t immediate) {
    return (0x0eu << 26) | (rt << 21) | (ra << 16) |
           static_cast<std::uint16_t>(immediate);
}

std::uint32_t Stw(unsigned rs, unsigned ra, std::int16_t immediate) {
    return (0x24u << 26) | (rs << 21) | (ra << 16) |
           static_cast<std::uint16_t>(immediate);
}

std::uint32_t Sc() { return 0x44000002u; }

void WriteInstruction(vshift::memory::GuestMemory& memory,
                      std::uint64_t address,
                      std::uint32_t instruction) {
    const std::array<std::uint8_t, 4> bytes{
        static_cast<std::uint8_t>(instruction >> 24),
        static_cast<std::uint8_t>(instruction >> 16),
        static_cast<std::uint8_t>(instruction >> 8),
        static_cast<std::uint8_t>(instruction)};
    assert(memory.Initialize(address, bytes).ok());
}

} // namespace

int main() {
    vshift::memory::GuestMemory memory;
    assert(memory.Map({0x1000, 0x100, vshift::memory::kPermissionRead |
                       vshift::memory::kPermissionExecute}).ok());
    assert(memory.Map({0x2000, 0x100, vshift::memory::kPermissionRead |
                       vshift::memory::kPermissionWrite}).ok());
    WriteInstruction(memory, 0x1000, Addi(3, 0, 42));
    WriteInstruction(memory, 0x1004, Stw(3, 1, 0));
    WriteInstruction(memory, 0x1008, Sc());

    vshift::cpu::PpuRuntime runtime(memory);
    runtime.registers().pc = 0x1000;
    runtime.registers().gpr[1] = 0x2000;
    const auto result = runtime.Run(10);
    assert(result.reason == vshift::cpu::PpuStopReason::Syscall);
    assert(result.instructions == 3);
    assert(result.registers.gpr[3] == 42);
    std::array<std::uint8_t, 4> stored{};
    assert(memory.Read(0x2000, stored).ok());
    assert(stored[0] == 0 && stored[3] == 42);
    return 0;
}
