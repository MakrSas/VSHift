#include "core/cpu/ppu_runtime.h"

#include <array>
#include <bit>
#include <cassert>
#include <cstdint>
#include <limits>

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

std::uint32_t Mcrf(unsigned destination_field, unsigned source_field) {
    return (0x13u << 26) | (destination_field << 23) |
           (source_field << 18);
}

std::uint32_t Fcfid(unsigned fd, unsigned fb) {
    return (0x3fu << 26) | (fd << 21) | (fb << 11) | (0x34eu << 1);
}

std::uint32_t Stdx(unsigned rs, unsigned ra, unsigned rb) {
    return (0x1fu << 26) | (rs << 21) | (ra << 16) | (rb << 11) |
           (0x095u << 1);
}

std::uint32_t Ldx(unsigned rt, unsigned ra, unsigned rb) {
    return (0x1fu << 26) | (rt << 21) | (ra << 16) | (rb << 11) |
           (0x015u << 1);
}

std::uint32_t Mulhdu(unsigned rt, unsigned ra, unsigned rb) {
    return (0x1fu << 26) | (rt << 21) | (ra << 16) | (rb << 11) |
           (0x009u << 1);
}

std::uint32_t Mulhd(unsigned rt, unsigned ra, unsigned rb, bool record = false) {
    return (0x1fu << 26) | (rt << 21) | (ra << 16) | (rb << 11) |
           (0x049u << 1) | (record ? 1u : 0u);
}

std::uint32_t Mulhw(unsigned rt, unsigned ra, unsigned rb, bool record = false) {
    return (0x1fu << 26) | (rt << 21) | (ra << 16) | (rb << 11) |
           (0x04bu << 1) | (record ? 1u : 0u);
}

std::uint32_t Mulhwu(unsigned rt, unsigned ra, unsigned rb) {
    return (0x1fu << 26) | (rt << 21) | (ra << 16) | (rb << 11) |
           (0x00bu << 1);
}

std::uint32_t Sradi(unsigned ra, unsigned rs, unsigned shift,
                    bool record = false) {
    return (0x1fu << 26) | (rs << 21) | (ra << 16) |
           ((shift & 0x1fu) << 11) |
           ((0x33au | (shift >> 5)) << 1) | (record ? 1u : 0u);
}

std::uint32_t Divd(unsigned rt, unsigned ra, unsigned rb) {
    return (0x1fu << 26) | (rt << 21) | (ra << 16) | (rb << 11) |
           (0x1e9u << 1);
}

std::uint32_t Divdu(unsigned rt, unsigned ra, unsigned rb) {
    return (0x1fu << 26) | (rt << 21) | (ra << 16) | (rb << 11) |
           (0x1c9u << 1);
}

std::uint32_t Divw(unsigned rt, unsigned ra, unsigned rb) {
    return (0x1fu << 26) | (rt << 21) | (ra << 16) | (rb << 11) |
           (0x1ebu << 1);
}

std::uint32_t Addze(unsigned rt, unsigned ra) {
    return (0x1fu << 26) | (rt << 21) | (ra << 16) | (0x0cau << 1);
}

std::uint32_t Lvebx(unsigned vd, unsigned ra, unsigned rb) {
    return (0x1fu << 26) | (vd << 21) | (ra << 16) | (rb << 11) |
           (0x007u << 1);
}

std::uint32_t Vspltb(unsigned vd, unsigned vb, unsigned element) {
    return (0x04u << 26) | (vd << 21) | (element << 16) | (vb << 11) | 0x20cu;
}

std::uint32_t Vxor(unsigned vd, unsigned va, unsigned vb) {
    return (0x04u << 26) | (vd << 21) | (va << 16) | (vb << 11) | 0x4c4u;
}

std::uint32_t Stvx(unsigned vs, unsigned ra, unsigned rb) {
    return (0x1fu << 26) | (vs << 21) | (ra << 16) | (rb << 11) |
           (0x0e7u << 1);
}

std::uint32_t Rlwnm(unsigned rs, unsigned ra, unsigned rb,
                    unsigned mb, unsigned me) {
    return (0x17u << 26) | (rs << 21) | (ra << 16) | (rb << 11) |
           (mb << 6) | (me << 1);
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

    vshift::memory::GuestMemory mcrf_memory;
    assert(mcrf_memory.Map({0x3900, 0x100, vshift::memory::kPermissionRead |
                            vshift::memory::kPermissionExecute}).ok());
    WriteInstruction(mcrf_memory, 0x3900, Mcrf(1, 7));
    WriteInstruction(mcrf_memory, 0x3904, Sc());
    vshift::cpu::PpuRuntime mcrf_runtime(mcrf_memory);
    mcrf_runtime.registers().pc = 0x3900;
    mcrf_runtime.registers().condition_register = 0x0000000au;
    const auto mcrf_result = mcrf_runtime.Run(4);
    assert(mcrf_result.reason == vshift::cpu::PpuStopReason::Syscall);
    assert((mcrf_result.registers.condition_register & 0x0f000000u) ==
           0x0a000000u);

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

    vshift::memory::GuestMemory ldx_memory;
    assert(ldx_memory.Map({0x9800, 0x100,
                           vshift::memory::kPermissionRead |
                           vshift::memory::kPermissionExecute}).ok());
    assert(ldx_memory.Map({0x9900, 0x100,
                           vshift::memory::kPermissionRead |
                           vshift::memory::kPermissionWrite}).ok());
    WriteInstruction(ldx_memory, 0x9800, Ldx(3, 1, 2));
    WriteInstruction(ldx_memory, 0x9804, Sc());
    const std::array<std::uint8_t, 8> ldx_value{
        0xde, 0xad, 0xbe, 0xef, 0x11, 0x22, 0x33, 0x44};
    assert(ldx_memory.Write(0x9908, ldx_value).ok());
    vshift::cpu::PpuRuntime ldx_runtime(ldx_memory);
    ldx_runtime.registers().pc = 0x9800;
    ldx_runtime.registers().gpr[1] = 0x9900;
    ldx_runtime.registers().gpr[2] = 8;
    const auto ldx_result = ldx_runtime.Run(4);
    assert(ldx_result.reason == vshift::cpu::PpuStopReason::Syscall);
    assert(ldx_result.registers.gpr[3] == UINT64_C(0xdeadbeef11223344));

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

    vshift::memory::GuestMemory mulhw_memory;
    assert(mulhw_memory.Map({0xa100, 0x100,
        vshift::memory::kPermissionRead |
        vshift::memory::kPermissionExecute}).ok());
    WriteInstruction(mulhw_memory, 0xa100, Mulhw(3, 1, 2, true));
    WriteInstruction(mulhw_memory, 0xa104, Mulhwu(4, 1, 2));
    WriteInstruction(mulhw_memory, 0xa108, Sc());
    vshift::cpu::PpuRuntime mulhw_runtime(mulhw_memory);
    mulhw_runtime.registers().pc = 0xa100;
    mulhw_runtime.registers().gpr[1] = UINT32_MAX;
    mulhw_runtime.registers().gpr[2] = UINT32_MAX;
    const auto mulhw_result = mulhw_runtime.Run(5);
    assert(mulhw_result.reason == vshift::cpu::PpuStopReason::Syscall);
    // Signed (-1 * -1) has a zero high word; unsigned max*max has fffffffe.
    assert(mulhw_result.registers.gpr[3] == 0);
    assert(mulhw_result.registers.gpr[4] == UINT32_C(0xfffffffe));
    assert((mulhw_result.registers.condition_register & 0xf0000000u) ==
           0x20000000u);

    vshift::memory::GuestMemory mulhd_memory;
    assert(mulhd_memory.Map({0xa200, 0x100,
        vshift::memory::kPermissionRead |
        vshift::memory::kPermissionExecute}).ok());
    WriteInstruction(mulhd_memory, 0xa200, Mulhd(3, 1, 2, true));
    WriteInstruction(mulhd_memory, 0xa204, Sc());
    vshift::cpu::PpuRuntime mulhd_runtime(mulhd_memory);
    mulhd_runtime.registers().pc = 0xa200;
    mulhd_runtime.registers().gpr[1] = UINT64_C(0x8000000000000000);
    mulhd_runtime.registers().gpr[2] = 2;
    const auto mulhd_result = mulhd_runtime.Run(4);
    assert(mulhd_result.reason == vshift::cpu::PpuStopReason::Syscall);
    assert(mulhd_result.registers.gpr[3] == UINT64_MAX);
    assert((mulhd_result.registers.condition_register & 0xf0000000u) ==
           0x80000000u);

    vshift::memory::GuestMemory sradi_memory;
    assert(sradi_memory.Map({0xa300, 0x100,
        vshift::memory::kPermissionRead |
        vshift::memory::kPermissionExecute}).ok());
    WriteInstruction(sradi_memory, 0xa300, Sradi(3, 1, 4, true));
    WriteInstruction(sradi_memory, 0xa304, Sradi(4, 2, 36));
    WriteInstruction(sradi_memory, 0xa308, Sc());
    vshift::cpu::PpuRuntime sradi_runtime(sradi_memory);
    sradi_runtime.registers().pc = 0xa300;
    sradi_runtime.registers().gpr[1] = UINT64_C(0xfffffffffffffff1);
    sradi_runtime.registers().gpr[2] = UINT64_C(0x7000000000);
    const auto sradi_result = sradi_runtime.Run(5);
    assert(sradi_result.reason == vshift::cpu::PpuStopReason::Syscall);
    assert(sradi_result.registers.gpr[3] == UINT64_MAX);
    assert(sradi_result.registers.gpr[4] == 7);
    assert((sradi_result.registers.condition_register & 0xf0000000u) ==
           0x80000000u);

    vshift::memory::GuestMemory divd_memory;
    assert(divd_memory.Map({0xb000, 0x100,
                            vshift::memory::kPermissionRead |
                            vshift::memory::kPermissionExecute}).ok());
    WriteInstruction(divd_memory, 0xb000, Divd(3, 1, 2));
    WriteInstruction(divd_memory, 0xb004, Sc());
    vshift::cpu::PpuRuntime divd_runtime(divd_memory);
    divd_runtime.registers().pc = 0xb000;
    divd_runtime.registers().gpr[1] = static_cast<std::uint64_t>(-10);
    divd_runtime.registers().gpr[2] = 3;
    const auto divd_result = divd_runtime.Run(4);
    assert(divd_result.reason == vshift::cpu::PpuStopReason::Syscall);
    assert(static_cast<std::int64_t>(divd_result.registers.gpr[3]) == -3);

    vshift::memory::GuestMemory divdu_memory;
    assert(divdu_memory.Map({0xb080, 0x100,
                             vshift::memory::kPermissionRead |
                             vshift::memory::kPermissionExecute}).ok());
    WriteInstruction(divdu_memory, 0xb080, Divdu(3, 1, 2));
    WriteInstruction(divdu_memory, 0xb084, Sc());
    vshift::cpu::PpuRuntime divdu_runtime(divdu_memory);
    divdu_runtime.registers().pc = 0xb080;
    divdu_runtime.registers().gpr[1] = 42;
    divdu_runtime.registers().gpr[2] = 0;
    const auto divdu_result = divdu_runtime.Run(4);
    assert(divdu_result.reason == vshift::cpu::PpuStopReason::Syscall);
    assert(divdu_result.registers.gpr[3] == 0);

    vshift::memory::GuestMemory divw_memory;
    assert(divw_memory.Map({0xb100, 0x100,
                            vshift::memory::kPermissionRead |
                            vshift::memory::kPermissionExecute}).ok());
    WriteInstruction(divw_memory, 0xb100, Divw(3, 1, 2));
    WriteInstruction(divw_memory, 0xb104, Sc());
    vshift::cpu::PpuRuntime divw_runtime(divw_memory);
    divw_runtime.registers().pc = 0xb100;
    divw_runtime.registers().gpr[1] = static_cast<std::uint64_t>(
        static_cast<std::int64_t>(-10));
    divw_runtime.registers().gpr[2] = 3;
    const auto divw_result = divw_runtime.Run(4);
    assert(divw_result.reason == vshift::cpu::PpuStopReason::Syscall);
    assert(static_cast<std::int64_t>(divw_result.registers.gpr[3]) == -3);

    vshift::memory::GuestMemory addze_memory;
    assert(addze_memory.Map({0xc000, 0x100,
                             vshift::memory::kPermissionRead |
                             vshift::memory::kPermissionExecute}).ok());
    WriteInstruction(addze_memory, 0xc000, Addze(3, 1));
    WriteInstruction(addze_memory, 0xc004, Sc());
    vshift::cpu::PpuRuntime addze_runtime(addze_memory);
    addze_runtime.registers().pc = 0xc000;
    addze_runtime.registers().gpr[1] = 41;
    addze_runtime.registers().xer = 1u << 29;
    const auto addze_result = addze_runtime.Run(4);
    assert(addze_result.reason == vshift::cpu::PpuStopReason::Syscall);
    assert(addze_result.registers.gpr[3] == 42);

    vshift::memory::GuestMemory lvebx_memory;
    assert(lvebx_memory.Map({0xd000, 0x100,
                             vshift::memory::kPermissionRead |
                             vshift::memory::kPermissionExecute}).ok());
    assert(lvebx_memory.Map({0xe000, 0x100,
                             vshift::memory::kPermissionRead |
                             vshift::memory::kPermissionWrite}).ok());
    WriteInstruction(lvebx_memory, 0xd000, Lvebx(0, 1, 2));
    WriteInstruction(lvebx_memory, 0xd004, Vspltb(0, 0, 0x0b));
    WriteInstruction(lvebx_memory, 0xd008, Stvx(0, 1, 2));
    WriteInstruction(lvebx_memory, 0xd00c, Sc());
    const std::array<std::uint8_t, 1> lvebx_byte{0xa5};
    assert(lvebx_memory.Write(0xe00b, lvebx_byte).ok());
    vshift::cpu::PpuRuntime lvebx_runtime(lvebx_memory);
    lvebx_runtime.registers().pc = 0xd000;
    lvebx_runtime.registers().gpr[1] = 0xe000;
    lvebx_runtime.registers().gpr[2] = 0x0b;
    const auto lvebx_result = lvebx_runtime.Run(5);
    assert(lvebx_result.reason == vshift::cpu::PpuStopReason::Syscall);
    for (const auto byte : lvebx_result.registers.vr[0]) assert(byte == 0xa5);
    std::array<std::uint8_t, 16> stored_vector{};
    assert(lvebx_memory.Read(0xe000, stored_vector).ok());
    for (const auto byte : stored_vector) assert(byte == 0xa5);
    WriteInstruction(lvebx_memory, 0xd010, Rlwnm(0, 0, 11, 0, 31));
    WriteInstruction(lvebx_memory, 0xd014, Sc());
    lvebx_runtime.registers().pc = 0xd010;
    lvebx_runtime.registers().gpr[0] = 0x12345678;
    lvebx_runtime.registers().gpr[11] = 4;
    const auto rlwnm_result = lvebx_runtime.Run(4);
    assert(rlwnm_result.reason == vshift::cpu::PpuStopReason::Syscall);
    assert(rlwnm_result.registers.gpr[0] == 0x23456781);

    vshift::memory::GuestMemory vxor_memory;
    assert(vxor_memory.Map({0xf000, 0x100,
                            vshift::memory::kPermissionRead |
                                vshift::memory::kPermissionExecute}).ok());
    WriteInstruction(vxor_memory, 0xf000, Vxor(2, 0, 1));
    WriteInstruction(vxor_memory, 0xf004, Sc());
    vshift::cpu::PpuRuntime vxor_runtime(vxor_memory);
    vxor_runtime.registers().pc = 0xf000;
    for (std::size_t lane = 0; lane < 16; ++lane) {
        vxor_runtime.registers().vr[0][lane] = static_cast<std::uint8_t>(lane);
        vxor_runtime.registers().vr[1][lane] = static_cast<std::uint8_t>(0xf0u - lane);
    }
    const auto vxor_result = vxor_runtime.Run(4);
    assert(vxor_result.reason == vshift::cpu::PpuStopReason::Syscall);
    for (std::size_t lane = 0; lane < 16; ++lane) {
        assert(vxor_result.registers.vr[2][lane] ==
               static_cast<std::uint8_t>(lane) ^ static_cast<std::uint8_t>(0xf0u - lane));
    }
    return 0;
}
