#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace vshift::cpu {

enum class X86Opcode : std::uint8_t {
    MovEaxImm32,
    AddEaxImm8,
    Ret,
};

struct X86Instruction final {
    X86Opcode opcode;
    std::uint32_t immediate = 0;
    std::uint8_t length = 0;
};

struct DecodeResult final {
    std::vector<X86Instruction> instructions;
    std::string error;

    bool ok() const noexcept { return error.empty(); }
};

DecodeResult Decode(std::span<const std::uint8_t> bytes);

} // namespace vshift::cpu
