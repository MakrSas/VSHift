#include "core/hle/kernel.h"

#include <limits>
#include <utility>

namespace vshift::hle {

void SyscallDispatcher::Register(std::uint64_t number,
                                 SyscallHandler handler) {
    if (!handler) {
        handlers_.erase(number);
        return;
    }
    handlers_[number] = std::move(handler);
}

bool SyscallDispatcher::Dispatch(SyscallContext& context,
                                 cpu::GuestRegisters& registers) const {
    const auto handler = handlers_.find(registers.rax());
    if (handler == handlers_.end()) {
        return false;
    }
    return handler->second(context, registers);
}

bool PresentRgba8Frame(SyscallContext& context,
                       cpu::GuestRegisters& registers) {
    if (registers.rsi() > std::numeric_limits<std::uint32_t>::max() ||
        registers.rdx() > std::numeric_limits<std::uint32_t>::max() ||
        registers.rcx() > std::numeric_limits<std::uint32_t>::max()) {
        registers.rax() = static_cast<std::uint64_t>(-1);
        return false;
    }

    const video::FrameDescription description{
        static_cast<std::uint32_t>(registers.rsi()),
        static_cast<std::uint32_t>(registers.rdx()),
        static_cast<std::uint32_t>(registers.rcx()),
        video::PixelFormat::Rgba8,
    };
    const auto result = context.video_output.CopyFromGuest(
        context.memory, registers.rdi(), description);
    registers.rax() = result.ok() ? 0 : static_cast<std::uint64_t>(-1);
    return result.ok();
}

} // namespace vshift::hle
