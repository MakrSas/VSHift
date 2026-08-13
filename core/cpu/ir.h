#pragma once

#include "core/cpu/x86_decoder.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace vshift::cpu {

enum class IrOpcode : std::uint8_t {
    SetEaxImm32,
    AddEaxImm32,
    Return,
};

struct IrInstruction final {
    IrOpcode opcode;
    std::uint32_t immediate = 0;
};

struct IrLoweringResult final {
    std::vector<IrInstruction> instructions;
    std::string error;

    bool ok() const noexcept { return error.empty(); }
};

IrLoweringResult LowerToIr(std::span<const X86Instruction> instructions);

struct GuestState final {
    std::uint32_t eax = 0;
    bool returned = false;
};

struct InterpreterResult final {
    GuestState state;
    std::string error;

    bool ok() const noexcept { return error.empty() && state.returned; }
};

// Reference semantics for the current IR subset. Arithmetic is deliberately
// 32-bit and wraps like the guest EAX register.
InterpreterResult Interpret(std::span<const IrInstruction> instructions);

} // namespace vshift::cpu
