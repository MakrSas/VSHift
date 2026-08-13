#include "core/cpu/ir.h"

#include <cassert>
#include <cstdint>

int main() {
    constexpr std::uint8_t program[] = {
        0xB8, 0x28, 0x00, 0x00, 0x00,
        0x83, 0xC0, 0x02,
        0xC3,
    };
    const auto decoded = vshift::cpu::Decode(program);
    assert(decoded.ok());

    const auto lowered = vshift::cpu::LowerToIr(decoded.instructions);
    assert(lowered.ok());
    assert(lowered.instructions.size() == 3);
    assert(lowered.instructions[0].opcode ==
           vshift::cpu::IrOpcode::SetEaxImm32);

    const auto interpreted = vshift::cpu::Interpret(lowered.instructions);
    assert(interpreted.ok());
    assert(interpreted.state.eax == 42);

    constexpr std::uint8_t wrapping_program[] = {
        0xB8, 0x00, 0x00, 0x00, 0x00,
        0x83, 0xC0, 0xFF,
        0xC3,
    };
    const auto wrapping_decoded = vshift::cpu::Decode(wrapping_program);
    const auto wrapping_ir = vshift::cpu::LowerToIr(wrapping_decoded.instructions);
    const auto wrapping_result = vshift::cpu::Interpret(wrapping_ir.instructions);
    assert(wrapping_result.ok());
    assert(wrapping_result.state.eax == 0xFFFFFFFFu);
    return 0;
}
