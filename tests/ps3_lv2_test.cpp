#include "core/cpu/ppu_runtime.h"
#include "core/hle/ps3_lv2.h"

#include <array>
#include <cassert>
#include <cstdint>

namespace {

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

std::uint32_t Addi(unsigned rt, unsigned ra, std::int16_t value) {
    return (0x0eu << 26) | (rt << 21) | (ra << 16) |
           static_cast<std::uint16_t>(value);
}

std::uint32_t Sc() { return 0x44000002u; }

} // namespace

int main() {
    vshift::memory::GuestMemory memory;
    assert(memory.Map({0x1000, 0x100, vshift::memory::kPermissionRead |
                       vshift::memory::kPermissionExecute}).ok());
    assert(memory.Map({0x2000, 0x100, vshift::memory::kPermissionRead |
                       vshift::memory::kPermissionWrite}).ok());
    WriteInstruction(memory, 0x1000, Addi(11, 0, 100));
    WriteInstruction(memory, 0x1004, Sc());
    WriteInstruction(memory, 0x1008, Addi(11, 0, 101));
    WriteInstruction(memory, 0x100c, Addi(3, 0, 0));
    WriteInstruction(memory, 0x1010, Sc());

    vshift::hle::Ps3Lv2 lv2(memory);
    vshift::cpu::PpuRuntime runtime(memory);
    runtime.registers().pc = 0x1000;
    runtime.registers().gpr[3] = 0x2000;
    const auto result = runtime.Run(20, [&](auto& registers, auto& error) {
        return lv2.Dispatch(registers, error);
    });
    assert(result.reason == vshift::cpu::PpuStopReason::UnsupportedInstruction);
    assert(result.instructions == 6);
    assert(lv2.trace().size() == 2);
    std::array<std::uint8_t, 4> object{};
    assert(memory.Read(0x2000, object).ok());
    assert(object[0] == 0 && object[1] == 0 && object[2] == 0x10);
    assert(object[3] == 0);
    runtime.registers().gpr[11] = 988;
    runtime.registers().gpr[3] = 4;
    std::string error;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    runtime.registers().gpr[11] = 30;
    runtime.registers().gpr[3] = 0x2000;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellEnoent);
    std::array<std::uint8_t, 0x40> paramsfo{};
    assert(memory.Read(0x2000, paramsfo).ok());
    for (const auto byte : paramsfo) assert(byte == 0);

    std::array<std::uint8_t, 0x30> prx_option{};
    prx_option[7] = 0x30;
    prx_option[15] = 1;
    assert(memory.Write(0x2040, prx_option).ok());
    runtime.registers().gpr[11] = 484;
    runtime.registers().gpr[3] = 0x2000;
    runtime.registers().gpr[4] = 0x2040;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    runtime.registers().gpr[11] = 380;
    runtime.registers().gpr[3] = 0x2010;
    runtime.registers().gpr[4] = 0x2011;
    runtime.registers().gpr[5] = 0x2012;
    runtime.registers().gpr[6] = 0x2016;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    std::array<std::uint8_t, 1> parameter_byte{};
    assert(memory.Read(0x2010, parameter_byte).ok());
    assert(parameter_byte[0] == 0);
    assert(memory.Read(0x2011, parameter_byte).ok());
    assert(parameter_byte[0] == 0);
    std::array<std::uint8_t, 4> memory_parameter{};
    assert(memory.Read(0x2012, memory_parameter).ok());
    assert(memory_parameter[0] == 0 && memory_parameter[1] == 0 &&
           memory_parameter[2] == 2 && memory_parameter[3] == 0);
    std::array<std::uint8_t, 8> boot_parameter{};
    assert(memory.Read(0x2016, boot_parameter).ok());
    assert(boot_parameter[7] == 7);
    runtime.registers().gpr[11] = 324;
    runtime.registers().gpr[3] = 0x2020;
    runtime.registers().gpr[4] = 0x300000;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    runtime.registers().gpr[11] = 486;
    runtime.registers().gpr[3] = 0x2000;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    const std::array<std::uint8_t, 11> module_path{
        'l', 'i', 'b', 'x', '.', 's', 'p', 'r', 'x', 0, 0};
    assert(memory.Write(0x2080, module_path).ok());
    runtime.registers().gpr[11] = 497;
    runtime.registers().gpr[3] = 0x2080;
    runtime.registers().gpr[4] = 0x1000;
    runtime.registers().gpr[5] = 0;
    runtime.registers().gpr[6] = 0;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == 0x23000000);
    std::array<std::uint8_t, 0x20> start_option{};
    start_option[7] = 0x20;
    start_option[15] = 1;
    assert(memory.Write(0x20a0, start_option).ok());
    runtime.registers().gpr[11] = 481;
    runtime.registers().gpr[3] = 0x23000000;
    runtime.registers().gpr[4] = 0;
    runtime.registers().gpr[5] = 0x20a0;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    return 0;
}
