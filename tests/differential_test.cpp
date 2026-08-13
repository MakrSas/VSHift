#include "core/cpu/arm64_jit.h"
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

    const auto interpreted = vshift::cpu::Interpret(lowered.instructions);
    assert(interpreted.ok());
    assert(interpreted.state.eax == 42);

    const auto compiled =
        vshift::cpu::Arm64Jit::CompileIr(lowered.instructions);
    assert(compiled.ok());
    assert(compiled.jit->code().size() == lowered.instructions.size());

    std::uint32_t jit_result = 0;
    if (compiled.jit->Execute(jit_result)) {
        assert(jit_result == interpreted.state.eax);
    }
    return 0;
}
