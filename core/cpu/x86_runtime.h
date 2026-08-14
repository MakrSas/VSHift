#pragma once

#include "core/memory/guest_memory.h"

#include <cstdint>
#include <functional>
#include <string>

namespace vshift::cpu {

constexpr std::size_t kGuestRegisterCount = 16;

struct GuestRegisters final {
    std::uint64_t general[kGuestRegisterCount] = {};
    std::uint64_t rip = 0;
    std::uint64_t rflags = 0x202;
    std::uint64_t fs_base = 0;
    std::uint64_t gs_base = 0;

    std::uint64_t& rax() noexcept { return general[0]; }
    std::uint64_t& rcx() noexcept { return general[1]; }
    std::uint64_t& rdx() noexcept { return general[2]; }
    std::uint64_t& rbx() noexcept { return general[3]; }
    std::uint64_t& rsp() noexcept { return general[4]; }
    std::uint64_t& rbp() noexcept { return general[5]; }
    std::uint64_t& rsi() noexcept { return general[6]; }
    std::uint64_t& rdi() noexcept { return general[7]; }
};

struct GuestCpuConfig final {
    std::uint64_t max_instructions = 1'000'000;
    std::uint64_t stack_top = 0x7fff'ff80'0000ull;
    std::uint64_t stack_size = 0x20'0000;
    std::uint64_t fs_base = 0;
    std::uint64_t gs_base = 0;
};

using SyscallHandler = std::function<bool(GuestRegisters&)>;

struct GuestCpuResult final {
    GuestRegisters registers;
    std::uint64_t instructions = 0;
    bool returned = false;
    std::string error;

    bool ok() const noexcept { return error.empty() && returned; }
};

// Demand-driven x86-64 interpreter for the firmware boot path. It is kept
// independent from UIKit/Metal and deliberately reports an unsupported
// opcode instead of guessing its semantics.
GuestCpuResult RunGuest(
    memory::GuestMemory& memory,
    std::uint64_t entry,
    const GuestCpuConfig& config = {},
    const SyscallHandler& syscall_handler = {});

} // namespace vshift::cpu
