#include "core/cpu/ppu_runtime.h"

#include <iomanip>
#include <limits>
#include <sstream>

namespace vshift::cpu {

namespace {

std::int16_t Sign16(std::uint32_t value) noexcept {
    return static_cast<std::int16_t>(value & 0xffffu);
}

std::int32_t Sign32(std::uint32_t value) noexcept {
    return static_cast<std::int32_t>(value);
}

std::int64_t SignExtend(std::uint64_t value, unsigned bits) noexcept {
    const auto shift = 64u - bits;
    return static_cast<std::int64_t>(value << shift) >> shift;
}

std::uint64_t RotateLeft64(std::uint64_t value, unsigned amount) noexcept {
    amount &= 63u;
    if (amount == 0) return value;
    return (value << amount) | (value >> (64u - amount));
}

std::uint32_t RotateLeft32(std::uint32_t value, unsigned amount) noexcept {
    amount &= 31u;
    if (amount == 0) return value;
    return (value << amount) | (value >> (32u - amount));
}

std::uint32_t MaskFromMbMe(unsigned mb, unsigned me) noexcept {
    std::uint32_t mask = 0;
    for (unsigned bit = 0; bit < 32; ++bit) {
        const bool selected = mb <= me ? bit >= mb && bit <= me
                                       : bit >= mb || bit <= me;
        if (selected) mask |= 1u << (31u - bit);
    }
    return mask;
}

std::uint64_t MaskFromMbMe64(unsigned mb, unsigned me) noexcept {
    std::uint64_t mask = 0;
    for (unsigned bit = 0; bit < 64; ++bit) {
        const bool selected = mb <= me ? bit >= mb && bit <= me
                                       : bit >= mb || bit <= me;
        if (selected) mask |= UINT64_C(1) << (63u - bit);
    }
    return mask;
}

std::string Hex(std::uint64_t value) {
    std::ostringstream stream;
    stream << "0x" << std::hex << value;
    return stream.str();
}

} // namespace

bool PpuRuntime::ReadU32(std::uint64_t address,
                         std::uint32_t& value,
                         std::string& error) const {
    std::array<std::uint8_t, 4> bytes{};
    const auto result = memory_.Read(address, bytes);
    if (!result.ok()) {
        error = result.error;
        return false;
    }
    value = (static_cast<std::uint32_t>(bytes[0]) << 24) |
            (static_cast<std::uint32_t>(bytes[1]) << 16) |
            (static_cast<std::uint32_t>(bytes[2]) << 8) |
            static_cast<std::uint32_t>(bytes[3]);
    return true;
}

bool PpuRuntime::ReadU64(std::uint64_t address,
                         std::uint64_t& value,
                         std::string& error) const {
    std::array<std::uint8_t, 8> bytes{};
    const auto result = memory_.Read(address, bytes);
    if (!result.ok()) {
        error = result.error;
        return false;
    }
    value = 0;
    for (const auto byte : bytes) value = (value << 8) | byte;
    return true;
}

bool PpuRuntime::WriteU32(std::uint64_t address,
                          std::uint32_t value,
                          std::string& error) {
    const std::array<std::uint8_t, 4> bytes{
        static_cast<std::uint8_t>(value >> 24),
        static_cast<std::uint8_t>(value >> 16),
        static_cast<std::uint8_t>(value >> 8),
        static_cast<std::uint8_t>(value)};
    const auto result = memory_.Write(address, bytes);
    if (!result.ok()) error = result.error;
    return result.ok();
}

bool PpuRuntime::WriteU64(std::uint64_t address,
                          std::uint64_t value,
                          std::string& error) {
    std::array<std::uint8_t, 8> bytes{};
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        bytes[index] = static_cast<std::uint8_t>(
            value >> ((bytes.size() - index - 1) * 8));
    }
    const auto result = memory_.Write(address, bytes);
    if (!result.ok()) error = result.error;
    return result.ok();
}

void PpuRuntime::SetCr0FromLogical(std::uint64_t value) noexcept {
    const std::uint32_t field = value == 0 ? 0x2u : 0x4u;
    registers_.condition_register =
        (registers_.condition_register & 0x0fffffff) | (field << 28);
}

void PpuRuntime::SetCr0FromSigned(std::int64_t value) noexcept {
    const std::uint32_t field = value < 0 ? 0x8u : value > 0 ? 0x4u : 0x2u;
    registers_.condition_register =
        (registers_.condition_register & 0x0fffffff) | (field << 28);
}

bool PpuRuntime::BranchCondition(std::uint32_t bo,
                                 std::uint32_t bi) noexcept {
    bool ctr_ok = true;
    if ((bo & 0x4u) == 0) {
        registers_.ctr = registers_.ctr - 1;
        ctr_ok = ((registers_.ctr != 0) != ((bo & 0x2u) != 0));
    }
    bool condition_ok = true;
    if ((bo & 0x10u) == 0) {
        const auto bit = (registers_.condition_register >> (31u - bi)) & 1u;
        condition_ok = bit == ((bo & 0x8u) != 0 ? 1u : 0u);
    }
    return ctr_ok && condition_ok;
}

PpuRuntime::StepResult PpuRuntime::Step(std::uint32_t& instruction,
                                        std::string& error) {
    const auto pc = registers_.pc;
    if ((pc & 3u) != 0) {
        error = "PPU PC is not instruction aligned at " + Hex(pc);
        return StepResult::MemoryFault;
    }
    if (!ReadU32(pc, instruction, error)) return StepResult::MemoryFault;

    const auto primary = instruction >> 26;
    const auto rt = (instruction >> 21) & 0x1f;
    const auto ra = (instruction >> 16) & 0x1f;
    const auto rb = (instruction >> 11) & 0x1f;
    const auto bo = (instruction >> 21) & 0x1f;
    const auto bi = (instruction >> 16) & 0x1f;
    const auto immediate = Sign16(instruction);
    const auto next_pc = pc + 4;
    auto address = [&](std::int64_t displacement) {
        return (ra == 0 ? 0 : registers_.gpr[ra]) + displacement;
    };
    auto branch = [&](std::uint64_t target, bool link) {
        if (link) registers_.lr = next_pc;
        registers_.pc = target;
    };
    registers_.pc = next_pc;

    switch (primary) {
    case 0x00: // illegal/reserved primary opcode
        error = "unsupported PPU primary opcode 0 at " + Hex(pc);
        return StepResult::UnsupportedInstruction;
    case 0x0c: { // addic
        registers_.gpr[rt] = address(immediate);
        if ((instruction & 1u) != 0) SetCr0FromSigned(static_cast<std::int64_t>(registers_.gpr[rt]));
        break;
    }
    case 0x0e: // addi
        registers_.gpr[rt] = address(immediate);
        break;
    case 0x0f: // addis
        registers_.gpr[rt] = (ra == 0 ? 0 : registers_.gpr[ra]) +
                             (static_cast<std::int64_t>(immediate) << 16);
        break;
    case 0x0a: // cmpli
    case 0x0b: { // cmpi
        const auto bf = (instruction >> 23) & 7u;
        const auto left = registers_.gpr[ra];
        std::uint32_t field = 0x2;
        if (primary == 0x0a) {
            const auto right = static_cast<std::uint32_t>(instruction & 0xffffu);
            const auto value = static_cast<std::uint32_t>(left);
            field = value < right ? 0x8u : value > right ? 0x4u : 0x2u;
        } else {
            const auto right = static_cast<std::int16_t>(instruction & 0xffffu);
            const auto value = static_cast<std::int64_t>(left);
            field = value < right ? 0x8u : value > right ? 0x4u : 0x2u;
        }
        const auto shift = (7u - bf) * 4u;
        registers_.condition_register =
            (registers_.condition_register & ~(0xfu << shift)) | (field << shift);
        break;
    }
    case 0x10: { // bc
        const auto displacement = SignExtend((instruction >> 2) & 0x3fffu, 14) << 2;
        if (BranchCondition(bo, bi)) {
            const auto target = (instruction & 2u) != 0
                ? static_cast<std::uint64_t>(displacement)
                : static_cast<std::uint64_t>(static_cast<std::int64_t>(pc) + displacement);
            branch(target, (instruction & 1u) != 0);
        }
        break;
    }
    case 0x11: // sc
        return StepResult::Syscall;
    case 0x12: { // b
        const auto displacement = SignExtend((instruction >> 2) & 0x00ffffffu, 24) << 2;
        const auto target = (instruction & 2u) != 0
            ? static_cast<std::uint64_t>(displacement)
            : static_cast<std::uint64_t>(static_cast<std::int64_t>(pc) + displacement);
        branch(target, (instruction & 1u) != 0);
        break;
    }
    case 0x13: { // bclr / bcctr
        const auto xo = (instruction >> 1) & 0x3ffu;
        if (xo == 0x010) {
            if (BranchCondition(bo, bi)) branch(registers_.lr & ~3ull, (instruction & 1u) != 0);
        } else if (xo == 0x210) {
            if (BranchCondition(bo, bi)) branch(registers_.ctr & ~3ull, (instruction & 1u) != 0);
        } else {
            error = "unsupported PPU branch-control opcode " + Hex(xo) + " at " + Hex(pc);
            return StepResult::UnsupportedInstruction;
        }
        break;
    }
    case 0x18: // ori
        registers_.gpr[ra] = registers_.gpr[rt] | (instruction & 0xffffu);
        break;
    case 0x19: // oris
        registers_.gpr[ra] = registers_.gpr[rt] |
                             (static_cast<std::uint64_t>(instruction & 0xffffu) << 16);
        break;
    case 0x1a: // xori
        registers_.gpr[ra] = registers_.gpr[rt] ^ (instruction & 0xffffu);
        break;
    case 0x1b: // xoris
        registers_.gpr[ra] = registers_.gpr[rt] ^
                             (static_cast<std::uint64_t>(instruction & 0xffffu) << 16);
        break;
    case 0x1c: // andi.
        registers_.gpr[ra] = registers_.gpr[rt] & (instruction & 0xffffu);
        SetCr0FromLogical(registers_.gpr[ra]);
        break;
    case 0x1d: // andis.
        registers_.gpr[ra] = registers_.gpr[rt] &
                             (static_cast<std::uint64_t>(instruction & 0xffffu) << 16);
        SetCr0FromLogical(registers_.gpr[ra]);
        break;
    case 0x15: { // rlwinm
        const auto sh = (instruction >> 11) & 0x1f;
        const auto mb = (instruction >> 6) & 0x1f;
        const auto me = (instruction >> 1) & 0x1f;
        const auto value = RotateLeft32(static_cast<std::uint32_t>(registers_.gpr[rt]), sh) &
                           MaskFromMbMe(mb, me);
        registers_.gpr[ra] = value;
        if ((instruction & 1u) != 0) SetCr0FromLogical(value);
        break;
    }
    case 0x20: { // lwz
        std::uint32_t value = 0;
        if (!ReadU32(address(immediate), value, error)) return StepResult::MemoryFault;
        registers_.gpr[rt] = value;
        break;
    }
    case 0x21: { // lwzu
        std::uint32_t value = 0;
        const auto effective = address(immediate);
        if (ra == 0 || !ReadU32(effective, value, error)) return StepResult::MemoryFault;
        registers_.gpr[ra] = effective;
        registers_.gpr[rt] = value;
        break;
    }
    case 0x22: { // lbz
        std::array<std::uint8_t, 1> value{};
        const auto result = memory_.Read(address(immediate), value);
        if (!result.ok()) { error = result.error; return StepResult::MemoryFault; }
        registers_.gpr[rt] = value[0];
        break;
    }
    case 0x24: { // stw
        if (!WriteU32(address(immediate), static_cast<std::uint32_t>(registers_.gpr[rt]), error)) {
            return StepResult::MemoryFault;
        }
        break;
    }
    case 0x25: { // stwu
        const auto effective = address(immediate);
        if (ra == 0 || !WriteU32(effective, static_cast<std::uint32_t>(registers_.gpr[rt]), error)) {
            return StepResult::MemoryFault;
        }
        registers_.gpr[ra] = effective;
        break;
    }
    case 0x26: { // stb
        const std::array<std::uint8_t, 1> value{
            static_cast<std::uint8_t>(registers_.gpr[rt])};
        const auto result = memory_.Write(address(immediate), value);
        if (!result.ok()) { error = result.error; return StepResult::MemoryFault; }
        break;
    }
    case 0x28: { // lhz
        std::array<std::uint8_t, 2> value{};
        const auto result = memory_.Read(address(immediate), value);
        if (!result.ok()) { error = result.error; return StepResult::MemoryFault; }
        registers_.gpr[rt] = (static_cast<std::uint64_t>(value[0]) << 8) | value[1];
        break;
    }
    case 0x2a: { // lha
        std::array<std::uint8_t, 2> value{};
        const auto result = memory_.Read(address(immediate), value);
        if (!result.ok()) { error = result.error; return StepResult::MemoryFault; }
        const auto half = static_cast<std::int16_t>((value[0] << 8) | value[1]);
        registers_.gpr[rt] = static_cast<std::int64_t>(half);
        break;
    }
    case 0x2c: { // sth
        const std::array<std::uint8_t, 2> value{
            static_cast<std::uint8_t>(registers_.gpr[rt] >> 8),
            static_cast<std::uint8_t>(registers_.gpr[rt])};
        const auto result = memory_.Write(address(immediate), value);
        if (!result.ok()) { error = result.error; return StepResult::MemoryFault; }
        break;
    }
    case 0x3a: { // ld/ldu/lwa (DS form)
        const auto form = instruction & 3u;
        const auto displacement = SignExtend((instruction >> 2) & 0x3fffu, 14) << 2;
        const auto effective = address(displacement);
        if (form == 2) {
            std::uint64_t value = 0;
            if (!ReadU64(effective, value, error)) return StepResult::MemoryFault;
            registers_.gpr[rt] = static_cast<std::int64_t>(static_cast<std::int32_t>(value));
        } else {
            std::uint64_t value = 0;
            if (!ReadU64(effective, value, error)) return StepResult::MemoryFault;
            registers_.gpr[rt] = value;
        }
        if (form == 1) {
            if (ra == 0) return StepResult::UnsupportedInstruction;
            registers_.gpr[ra] = effective;
        }
        break;
    }
    case 0x3e: { // std/stdu
        const auto form = instruction & 3u;
        const auto displacement = SignExtend((instruction >> 2) & 0x3fffu, 14) << 2;
        const auto effective = address(displacement);
        if (!WriteU64(effective, registers_.gpr[rt], error)) return StepResult::MemoryFault;
        if (form == 1) {
            if (ra == 0) return StepResult::UnsupportedInstruction;
            registers_.gpr[ra] = effective;
        }
        break;
    }
    case 0x1e: { // rldicl/rldicr/rldimi family
        const auto sh = ((instruction >> 11) & 0x1fu) |
                        (((instruction >> 1) & 1u) << 5);
        const auto mb = ((instruction >> 6) & 0x1fu) |
                        (((instruction >> 5) & 1u) << 5);
        const auto value = RotateLeft64(registers_.gpr[rt], sh);
        if ((instruction & 0x4u) == 0) { // rldicl
            registers_.gpr[ra] = value & (~UINT64_C(0) >> mb);
        } else if ((instruction & 0x8u) == 0) { // rldicr
            registers_.gpr[ra] = value & (~UINT64_C(0) << (mb ^ 63u));
        } else {
            error = "unsupported PPU rotate-double form at " + Hex(pc);
            return StepResult::UnsupportedInstruction;
        }
        break;
    }
    case 0x1f: {
        const auto xo = (instruction >> 1) & 0x3ffu;
        switch (xo) {
        case 0x000: // cmpw/cmpd
        case 0x20: { // cmplw
            const auto bf = (instruction >> 23) & 7u;
            const auto left = registers_.gpr[ra];
            const auto right = registers_.gpr[rb];
            std::uint32_t field = 0x2;
            if (xo == 0x000) {
                const auto l = static_cast<std::int64_t>(static_cast<std::int32_t>(left));
                const auto r = static_cast<std::int64_t>(static_cast<std::int32_t>(right));
                field = l < r ? 0x8u : l > r ? 0x4u : 0x2u;
            } else {
                const auto l = static_cast<std::uint32_t>(left);
                const auto r = static_cast<std::uint32_t>(right);
                field = l < r ? 0x8u : l > r ? 0x4u : 0x2u;
            }
            const auto shift = (7u - bf) * 4u;
            registers_.condition_register =
                (registers_.condition_register & ~(0xfu << shift)) | (field << shift);
            break;
        }
        case 0x013: // mfocrf
            registers_.gpr[rt] = registers_.condition_register;
            break;
        case 0x01a: { // cntlzw
            const auto value = static_cast<std::uint32_t>(registers_.gpr[rt]);
            unsigned count = 32;
            for (unsigned bit = 0; bit < 32; ++bit) {
                if ((value & (1u << (31u - bit))) != 0) {
                    count = bit;
                    break;
                }
            }
            registers_.gpr[ra] = count;
            if ((instruction & 1u) != 0) SetCr0FromLogical(count);
            break;
        }
        case 0x03a: { // cntlzd
            const auto value = registers_.gpr[rt];
            unsigned count = 64;
            for (unsigned bit = 0; bit < 64; ++bit) {
                if ((value & (UINT64_C(1) << (63u - bit))) != 0) {
                    count = bit;
                    break;
                }
            }
            registers_.gpr[ra] = count;
            if ((instruction & 1u) != 0) SetCr0FromLogical(count);
            break;
        }
        case 0x018: { // slw
            const auto shift = registers_.gpr[rb] & 0x3fu;
            registers_.gpr[ra] = shift >= 32 ? 0 :
                static_cast<std::uint32_t>(registers_.gpr[rt]) << shift;
            if ((instruction & 1u) != 0) SetCr0FromLogical(registers_.gpr[ra]);
            break;
        }
        case 0x01b: { // sld
            const auto shift = registers_.gpr[rb] & 0x7fu;
            registers_.gpr[ra] = shift >= 64 ? 0 : registers_.gpr[rt] << shift;
            if ((instruction & 1u) != 0) SetCr0FromLogical(registers_.gpr[ra]);
            break;
        }
        case 0x01c: // and
            registers_.gpr[ra] = registers_.gpr[rt] & registers_.gpr[rb];
            if ((instruction & 1u) != 0) SetCr0FromLogical(registers_.gpr[ra]);
            break;
        case 0x13c: // xor
            registers_.gpr[ra] = registers_.gpr[rt] ^ registers_.gpr[rb];
            if ((instruction & 1u) != 0) SetCr0FromLogical(registers_.gpr[ra]);
            break;
        case 0x1bc: // or / mr
            registers_.gpr[ra] = registers_.gpr[rt] | registers_.gpr[rb];
            if ((instruction & 1u) != 0) SetCr0FromLogical(registers_.gpr[ra]);
            break;
        case 0x10a: // add
            registers_.gpr[ra] = registers_.gpr[rt] + registers_.gpr[rb];
            if ((instruction & 1u) != 0) SetCr0FromSigned(static_cast<std::int64_t>(registers_.gpr[ra]));
            break;
        case 0x028: // subf
            registers_.gpr[ra] = registers_.gpr[rb] - registers_.gpr[rt];
            if ((instruction & 1u) != 0) SetCr0FromSigned(static_cast<std::int64_t>(registers_.gpr[ra]));
            break;
        case 0x150: // isel
            registers_.gpr[ra] = ((registers_.condition_register >> (31u - (instruction >> 6 & 0x1f))) & 1u)
                ? registers_.gpr[rt] : registers_.gpr[rb];
            break;
        case 0x153: { // mfspr
            const auto spr = ((instruction >> 16) & 0x1fu) |
                             (((instruction >> 11) & 0x1fu) << 5);
            if (spr == 8) registers_.gpr[rt] = registers_.lr;
            else if (spr == 9) registers_.gpr[rt] = registers_.ctr;
            else registers_.gpr[rt] = 0;
            break;
        }
        case 0x1d3: { // mtspr
            const auto spr = ((instruction >> 16) & 0x1fu) |
                             (((instruction >> 11) & 0x1fu) << 5);
            if (spr == 8) registers_.lr = registers_.gpr[rt];
            else if (spr == 9) registers_.ctr = registers_.gpr[rt];
            break;
        }
        case 0x004: // tw
            return StepResult::Halted;
        case 0x016: // sync/isync variants are harmless at this stage
        case 0x356:
        case 0x3b6:
            break;
        default:
            if (xo == 0x210) { // bcctr shares the XO in some encodings
                if (BranchCondition(bo, bi)) branch(registers_.ctr & ~3ull, (instruction & 1u) != 0);
                break;
            }
            error = "unsupported PPU extended opcode " + Hex(xo) + " at " + Hex(pc);
            return StepResult::UnsupportedInstruction;
        }
        break;
    }
    default:
        error = "unsupported PPU primary opcode " + Hex(primary) + " at " + Hex(pc);
        return StepResult::UnsupportedInstruction;
    }
    return StepResult::Continue;
}

PpuRunResult PpuRuntime::Run(std::size_t max_instructions) {
    PpuRunResult result;
    for (; result.instructions < max_instructions; ++result.instructions) {
        std::uint32_t instruction = 0;
        std::string error;
        const auto step = Step(instruction, error);
        result.instruction = instruction;
        if (step == StepResult::Continue) continue;
        result.instructions++;
        result.registers = registers_;
        result.error = std::move(error);
        result.reason = step == StepResult::Syscall ? PpuStopReason::Syscall
            : step == StepResult::UnsupportedInstruction ? PpuStopReason::UnsupportedInstruction
            : step == StepResult::MemoryFault ? PpuStopReason::MemoryFault
            : PpuStopReason::Halted;
        return result;
    }
    result.reason = PpuStopReason::StepLimit;
    result.registers = registers_;
    return result;
}

} // namespace vshift::cpu
