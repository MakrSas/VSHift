#include "core/cpu/ppu_runtime.h"

#include <array>
#include <bit>
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

std::uint32_t Cmpwi(unsigned bf, unsigned ra, std::int16_t immediate) {
    return (0x0bu << 26) | (bf << 23) | (ra << 16) |
           static_cast<std::uint16_t>(immediate);
}

std::uint32_t Andis(unsigned rt, unsigned ra, std::uint16_t immediate) {
    return (0x1du << 26) | (rt << 21) | (ra << 16) | immediate;
}

std::uint32_t Stwcx(unsigned rs, unsigned ra, unsigned rb) {
    return (0x1fu << 26) | (rs << 21) | (ra << 16) | (rb << 11) |
           (0x096u << 1) | 1u;
}

std::uint32_t Fcfid(unsigned fd, unsigned fb) {
    return (0x3fu << 26) | (fd << 21) | (fb << 11) | (0x34eu << 1);
}

std::uint32_t Stdx(unsigned rs, unsigned ra, unsigned rb) {
    return (0x1fu << 26) | (rs << 21) | (ra << 16) | (rb << 11) |
           (0x095u << 1);
}

std::uint32_t Mulhdu(unsigned rt, unsigned ra, unsigned rb) {
    return (0x1fu << 26) | (rt << 21) | (ra << 16) | (rb << 11) |
           (0x009u << 1);
}

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

    vshift::memory::GuestMemory compare_memory;
    assert(compare_memory.Map({0x3000, 0x100, vshift::memory::kPermissionRead |
                                vshift::memory::kPermissionExecute}).ok());
    WriteInstruction(compare_memory, 0x3000, Cmpwi(7, 9, -1));
    WriteInstruction(compare_memory, 0x3004, Sc());
    vshift::cpu::PpuRuntime compare_runtime(compare_memory);
    compare_runtime.registers().pc = 0x3000;
    compare_runtime.registers().gpr[9] = 0xffffffffu;
    const auto compare_result = compare_runtime.Run(4);
    assert(compare_result.reason == vshift::cpu::PpuStopReason::Syscall);
    assert((compare_result.registers.condition_register & 0xfu) == 0x2u);

    vshift::memory::GuestMemory andis_memory;
    assert(andis_memory.Map({0x4000, 0x100,
                             vshift::memory::kPermissionRead |
                             vshift::memory::kPermissionExecute}).ok());
    WriteInstruction(andis_memory, 0x4000, Andis(4, 3, 0xffff));
    WriteInstruction(andis_memory, 0x4004, Sc());
    vshift::cpu::PpuRuntime andis_runtime(andis_memory);
    andis_runtime.registers().pc = 0x4000;
    andis_runtime.registers().gpr[4] = 0x12345678;
    const auto andis_result = andis_runtime.Run(4);
    assert(andis_result.reason == vshift::cpu::PpuStopReason::Syscall);
    assert(andis_result.registers.gpr[3] == 0x12340000);

    vshift::memory::GuestMemory store_conditional_memory;
    assert(store_conditional_memory.Map({0x5000, 0x100,
                                         vshift::memory::kPermissionRead |
                                         vshift::memory::kPermissionExecute}).ok());
    assert(store_conditional_memory.Map({0x6000, 0x100,
                                         vshift::memory::kPermissionRead |
                                         vshift::memory::kPermissionWrite}).ok());
    WriteInstruction(store_conditional_memory, 0x5000, Stwcx(3, 1, 0));
    WriteInstruction(store_conditional_memory, 0x5004, Sc());
    vshift::cpu::PpuRuntime store_conditional_runtime(store_conditional_memory);
    store_conditional_runtime.registers().pc = 0x5000;
    store_conditional_runtime.registers().gpr[1] = 0x6000;
    store_conditional_runtime.registers().gpr[3] = 0x1234;
    const auto store_conditional_result = store_conditional_runtime.Run(4);
    assert(store_conditional_result.reason == vshift::cpu::PpuStopReason::Syscall);
    assert(((store_conditional_result.registers.condition_register >> 28) & 0xfu) == 0x2u);

    vshift::memory::GuestMemory fcfid_memory;
    assert(fcfid_memory.Map({0x7000, 0x100,
                             vshift::memory::kPermissionRead |
                             vshift::memory::kPermissionExecute}).ok());
    WriteInstruction(fcfid_memory, 0x7000, Fcfid(1, 0));
    WriteInstruction(fcfid_memory, 0x7004, Sc());
    vshift::cpu::PpuRuntime fcfid_runtime(fcfid_memory);
    fcfid_runtime.registers().pc = 0x7000;
    fcfid_runtime.registers().fpr[0] = static_cast<std::uint64_t>(-42);
    const auto fcfid_result = fcfid_runtime.Run(4);
    assert(fcfid_result.reason == vshift::cpu::PpuStopReason::Syscall);
    assert(std::bit_cast<double>(fcfid_result.registers.fpr[1]) == -42.0);

    vshift::memory::GuestMemory stdx_memory;
    assert(stdx_memory.Map({0x8000, 0x100,
                            vshift::memory::kPermissionRead |
                            vshift::memory::kPermissionExecute}).ok());
    assert(stdx_memory.Map({0x9000, 0x100,
                            vshift::memory::kPermissionRead |
                            vshift::memory::kPermissionWrite}).ok());
    WriteInstruction(stdx_memory, 0x8000, Stdx(3, 1, 2));
    WriteInstruction(stdx_memory, 0x8004, Sc());
    vshift::cpu::PpuRuntime stdx_runtime(stdx_memory);
    stdx_runtime.registers().pc = 0x8000;
    stdx_runtime.registers().gpr[1] = 0x9000;
    stdx_runtime.registers().gpr[2] = 8;
    stdx_runtime.registers().gpr[3] = UINT64_C(0x1122334455667788);
    const auto stdx_result = stdx_runtime.Run(4);
    assert(stdx_result.reason == vshift::cpu::PpuStopReason::Syscall);
    std::array<std::uint8_t, 8> double_word{};
    assert(stdx_memory.Read(0x9008, double_word).ok());
    assert(double_word[0] == 0x11 && double_word[7] == 0x88);

    vshift::memory::GuestMemory mulhdu_memory;
    assert(mulhdu_memory.Map({0xa000, 0x100,
                              vshift::memory::kPermissionRead |
                              vshift::memory::kPermissionExecute}).ok());
    WriteInstruction(mulhdu_memory, 0xa000, Mulhdu(3, 1, 2));
    WriteInstruction(mulhdu_memory, 0xa004, Sc());
    vshift::cpu::PpuRuntime mulhdu_runtime(mulhdu_memory);
    mulhdu_runtime.registers().pc = 0xa000;
    mulhdu_runtime.registers().gpr[1] = UINT64_MAX;
    mulhdu_runtime.registers().gpr[2] = 2;
    const auto mulhdu_result = mulhdu_runtime.Run(4);
    assert(mulhdu_result.reason == vshift::cpu::PpuStopReason::Syscall);
    assert(mulhdu_result.registers.gpr[3] == 1);
    return 0;
}
