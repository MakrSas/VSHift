#include "core/cpu/ir.h"

namespace vshift::cpu {

IrLoweringResult LowerToIr(
    std::span<const X86Instruction> instructions) {
    IrLoweringResult result;
    result.instructions.reserve(instructions.size());
    for (const auto& instruction : instructions) {
        switch (instruction.opcode) {
        case X86Opcode::MovEaxImm32:
            result.instructions.push_back(
                {IrOpcode::SetEaxImm32, instruction.immediate});
            break;
        case X86Opcode::AddEaxImm8:
            result.instructions.push_back(
                {IrOpcode::AddEaxImm32, instruction.immediate});
            break;
        case X86Opcode::Ret:
            if (!result.instructions.empty() &&
                result.instructions.back().opcode == IrOpcode::Return) {
                result.error = "IR contains instructions after return";
                result.instructions.clear();
                return result;
            }
            result.instructions.push_back({IrOpcode::Return, 0});
            break;
        }
    }

    if (result.instructions.empty() ||
        result.instructions.back().opcode != IrOpcode::Return) {
        result.error = "IR program must end with return";
        result.instructions.clear();
    }
    return result;
}

InterpreterResult Interpret(
    std::span<const IrInstruction> instructions) {
    InterpreterResult result;
    if (instructions.empty() ||
        instructions.back().opcode != IrOpcode::Return) {
        result.error = "interpreter program must end with return";
        return result;
    }

    for (const auto& instruction : instructions) {
        switch (instruction.opcode) {
        case IrOpcode::SetEaxImm32:
            result.state.eax = instruction.immediate;
            break;
        case IrOpcode::AddEaxImm32:
            result.state.eax += instruction.immediate;
            break;
        case IrOpcode::Return:
            result.state.returned = true;
            break;
        }
        if (result.state.returned) {
            break;
        }
    }

    if (!result.state.returned) {
        result.error = "interpreter reached the end without return";
    }
    return result;
}

} // namespace vshift::cpu
