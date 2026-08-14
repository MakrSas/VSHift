#pragma once

#include "core/memory/guest_memory.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace vshift::cpu {

struct PpuRegisters final {
    std::array<std::uint64_t, 32> gpr{};
    std::uint64_t pc = 0;
    std::uint64_t lr = 0;
    std::uint64_t ctr = 0;
    std::uint32_t condition_register = 0;
};

enum class PpuStopReason {
    StepLimit,
    Syscall,
    UnsupportedInstruction,
    MemoryFault,
    Halted,
};

struct PpuRunResult final {
    PpuStopReason reason = PpuStopReason::StepLimit;
    std::size_t instructions = 0;
    PpuRegisters registers;
    std::uint32_t instruction = 0;
    std::string error;
};

// Small big-endian PPU interpreter boundary. It is deliberately limited to
// the integer/control-flow subset used by the first VSH startup attempt; HLE
// LV2 syscalls and the RSX are separate runtime layers.
class PpuRuntime final {
public:
    explicit PpuRuntime(memory::GuestMemory& memory) noexcept
        : memory_(memory) {}

    PpuRegisters& registers() noexcept { return registers_; }
    const PpuRegisters& registers() const noexcept { return registers_; }

    PpuRunResult Run(std::size_t max_instructions);

private:
    enum class StepResult {
        Continue,
        Syscall,
        UnsupportedInstruction,
        MemoryFault,
        Halted,
    };

    StepResult Step(std::uint32_t& instruction, std::string& error);
    bool ReadU32(std::uint64_t address, std::uint32_t& value,
                 std::string& error) const;
    bool ReadU64(std::uint64_t address, std::uint64_t& value,
                 std::string& error) const;
    bool WriteU32(std::uint64_t address, std::uint32_t value,
                  std::string& error);
    bool WriteU64(std::uint64_t address, std::uint64_t value,
                  std::string& error);
    void SetCr0FromLogical(std::uint64_t value) noexcept;
    void SetCr0FromSigned(std::int64_t value) noexcept;
    bool BranchCondition(std::uint32_t bo, std::uint32_t bi) noexcept;

    memory::GuestMemory& memory_;
    PpuRegisters registers_;
};

} // namespace vshift::cpu
