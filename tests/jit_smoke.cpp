#include "core/cpu/arm64_jit.h"
#include "core/cpu/x86_decoder.h"

#include <cassert>
#include <cstdint>
#include <iostream>

int main() {
    constexpr std::uint8_t program[] = {
        0xB8, 0x28, 0x00, 0x00, 0x00, // mov eax, 40
        0x83, 0xC0, 0x02,             // add eax, 2
        0xC3,                         // ret
    };

    const auto decoded = vshift::cpu::Decode(program);
    assert(decoded.ok());
    assert(decoded.instructions.size() == 3);

    const auto compiled =
        vshift::cpu::Arm64Jit::Compile(decoded.instructions);
    assert(compiled.ok());
    assert(compiled.jit->code().size() == 3);
    assert(compiled.jit->code()[0] == 0x52800500u); // mov w0, #40
    assert(compiled.jit->code()[1] == 0x11000800u); // add w0, w0, #2
    assert(compiled.jit->code()[2] == 0xD65F03C0u); // ret

    std::uint32_t result = 0;
    if (compiled.jit->Execute(result)) {
        assert(result == 42);
        std::cout << "ARM64 JIT execution result: " << result << '\n';
    } else {
        std::cout << "ARM64 JIT bytes generated; execution skipped on non-ARM64 host\n";
    }

    return 0;
}
