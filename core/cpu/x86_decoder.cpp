#include "core/cpu/x86_decoder.h"

#include <cstring>

namespace vshift::cpu {

DecodeResult Decode(std::span<const std::uint8_t> bytes) {
    DecodeResult result;
    std::size_t offset = 0;

    while (offset < bytes.size()) {
        const auto opcode = bytes[offset];
        if (opcode == 0xB8) { // mov eax, imm32
            if (bytes.size() - offset < 5) {
                result.error = "truncated mov eax, imm32";
                return result;
            }
            std::uint32_t immediate = 0;
            std::memcpy(&immediate, bytes.data() + offset + 1, sizeof(immediate));
            result.instructions.push_back(
                {X86Opcode::MovEaxImm32, immediate, 5});
            offset += 5;
            continue;
        }

        if (opcode == 0x83 && offset + 2 < bytes.size() &&
            bytes[offset + 1] == 0xC0) { // add eax, imm8
            const auto immediate = static_cast<std::int8_t>(bytes[offset + 2]);
            result.instructions.push_back({
                X86Opcode::AddEaxImm8,
                static_cast<std::uint32_t>(static_cast<std::int32_t>(immediate)),
                3,
            });
            offset += 3;
            continue;
        }

        if (opcode == 0xC3) { // ret
            result.instructions.push_back({X86Opcode::Ret, 0, 1});
            offset += 1;
            if (offset != bytes.size()) {
                result.error = "bytes found after ret";
                return result;
            }
            return result;
        }

        result.error = "unsupported x86-64 instruction at byte " +
                       std::to_string(offset);
        return result;
    }

    result.error = "program has no ret";
    return result;
}

} // namespace vshift::cpu
