#include "core/hle/kernel.h"

#include <array>
#include <cassert>

int main() {
    vshift::memory::GuestMemory memory;
    assert(memory.Map({0x700000, 0x1000,
                       vshift::memory::kPermissionRead |
                           vshift::memory::kPermissionWrite})
               .ok());

    bool presented = false;
    vshift::video::FrameBuffer output(
        [&](const vshift::video::GuestFrame& frame) {
            presented = frame.description.width == 2 &&
                        frame.description.height == 1;
            return presented;
        });
    vshift::hle::SyscallContext context{memory, output};
    vshift::cpu::GuestRegisters registers;
    registers.rax() = 0xfeed;
    registers.rdi() = 0x700000;
    registers.rsi() = 2;
    registers.rdx() = 1;
    registers.rcx() = 8;

    const std::array<std::uint8_t, 8> pixels = {255, 0, 0, 255,
                                                 0, 0, 255, 255};
    assert(memory.Initialize(0x700000, pixels).ok());

    vshift::hle::SyscallDispatcher dispatcher;
    dispatcher.Register(0xfeed, vshift::hle::PresentRgba8Frame);
    assert(dispatcher.Dispatch(context, registers));
    assert(presented);
    assert(registers.rax() == 0);

    registers.rax() = 0xbeef;
    assert(!dispatcher.Dispatch(context, registers));
    return 0;
}
