#include "core/cpu/arm64_jit.h"

#include "core/cpu/executable_memory.h"

#include <cstring>

namespace vshift::cpu {

namespace {

// The PoC intentionally emits only the instructions needed by the first
// smoke program. The encoding is fixed-width and kept explicit so the next
// decoder/IR work can be tested without depending on an assembler.
constexpr std::uint32_t AddW0Imm(std::uint32_t immediate) {
    return 0x11000000u | ((immediate & 0xFFFu) << 10);
}

constexpr std::uint32_t MovW0Imm(std::uint32_t immediate) {
    return 0x52800000u | ((immediate & 0xFFFFu) << 5);
}

constexpr std::uint32_t Ret() {
    return 0xD65F03C0u;
}

} // namespace

Arm64Jit::Result Arm64Jit::Compile(
    std::span<const X86Instruction> instructions) {
    std::vector<std::uint32_t> code;
    code.reserve(instructions.size());

    for (const auto& instruction : instructions) {
        switch (instruction.opcode) {
        case X86Opcode::MovEaxImm32:
            if (instruction.immediate > 0xFFFFu) {
                return {nullptr, "mov eax immediate is outside PoC range"};
            }
            code.push_back(MovW0Imm(instruction.immediate));
            break;
        case X86Opcode::AddEaxImm8:
            if (static_cast<std::int32_t>(instruction.immediate) < 0 ||
                instruction.immediate > 0xFFFu) {
                return {nullptr, "add eax immediate is outside PoC range"};
            }
            code.push_back(AddW0Imm(instruction.immediate));
            break;
        case X86Opcode::Ret:
            code.push_back(Ret());
            break;
        }
    }

    if (code.empty() || code.back() != Ret()) {
        return {nullptr, "ARM64 program must end with ret"};
    }

    auto memory = ExecutableMemory::Allocate(code.size() * sizeof(std::uint32_t));
    if (!memory) {
        return {nullptr, "failed to allocate executable memory"};
    }
    std::memcpy(memory->writable_data(), code.data(), code.size() * sizeof(code[0]));
    memory->FlushInstructionCache(0, code.size() * sizeof(code[0]));
    if (!memory->MakeExecutable()) {
        return {nullptr, "failed to make JIT memory executable"};
    }

    return {std::unique_ptr<Arm64Jit>(
                new Arm64Jit(std::move(memory), std::move(code))),
            {}};
}

bool Arm64Jit::Execute(std::uint32_t& result) const noexcept {
#if defined(__aarch64__) || defined(_M_ARM64)
    using Function = std::uint32_t (*)();
    const auto function = reinterpret_cast<Function>(memory_->executable_data());
    result = function();
    return true;
#else
    (void)result;
    return false;
#endif
}

} // namespace vshift::cpu
