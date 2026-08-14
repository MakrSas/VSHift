#pragma once

#include "core/cpu/x86_runtime.h"
#include "core/memory/guest_memory.h"
#include "core/video/framebuffer.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

namespace vshift::hle {

struct SyscallContext final {
    memory::GuestMemory& memory;
    video::FrameBuffer& video_output;
};

using SyscallHandler =
    std::function<bool(SyscallContext&, cpu::GuestRegisters&)>;

// Explicit syscall registry used by the guest CPU. It deliberately does not
// invent PS4 syscall numbers: callers register only ABI entries they have
// identified, and an unknown number is a hard stop rather than an emulation
// guess.
class SyscallDispatcher final {
public:
    void Register(std::uint64_t number, SyscallHandler handler);

    bool Dispatch(SyscallContext& context,
                  cpu::GuestRegisters& registers) const;

    std::size_t size() const noexcept { return handlers_.size(); }

private:
    std::unordered_map<std::uint64_t, SyscallHandler> handlers_;
};

// A small, explicit ABI helper for tests and bring-up. It is not a claimed
// Sony syscall number. A real PS4 ABI adapter can be registered later after
// the corresponding import and argument convention are identified.
bool PresentRgba8Frame(SyscallContext& context,
                       cpu::GuestRegisters& registers);

} // namespace vshift::hle
