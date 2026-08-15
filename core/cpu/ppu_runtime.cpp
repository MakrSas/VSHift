#include "core/cpu/ppu_runtime.h"

#include <bit>
#include <cmath>
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
        error = result.error + " at " + Hex(address);
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
        error = result.error + " at " + Hex(address);
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
    if (!result.ok()) error = result.error + " at " + Hex(address);
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
    if (!result.ok()) error = result.error + " at " + Hex(address);
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
    case 0x04: { // VMX/AltiVec VX-form instructions
        const auto xo = instruction & 0x7ffu;
        switch (xo) {
        case 0x4c4: { // vxor
            for (std::size_t lane = 0; lane < registers_.vr[rt].size(); ++lane) {
                registers_.vr[rt][lane] = registers_.vr[ra][lane] ^
                                           registers_.vr[rb][lane];
            }
            break;
        }
        case 0x20c: { // vspltb
            const auto source = rb;
            const auto element = ra & 0xfu;
            const auto value = registers_.vr[source][element];
            registers_.vr[rt].fill(value);
            break;
        }
        default:
            error = "unsupported PPU vector opcode " + Hex(xo) + " at " + Hex(pc);
            return StepResult::UnsupportedInstruction;
        }
        break;
    }
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
    case 0x08: // subfic
        registers_.gpr[rt] = static_cast<std::int64_t>(immediate) -
                             (ra == 0 ? 0 : registers_.gpr[ra]);
        break;
    case 0x07: // mulli
        registers_.gpr[rt] = (ra == 0 ? 0 : registers_.gpr[ra]) *
                             static_cast<std::int64_t>(immediate);
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
            if ((instruction & (1u << 21)) == 0) {
                // cmpwi compares the sign-extended low word.  lwz zero
                // extends its result, so comparing the full 64-bit register
                // would turn the usual 0xffffffff sentinel into +4294967295.
                const auto value = static_cast<std::int32_t>(left);
                field = value < right ? 0x8u : value > right ? 0x4u : 0x2u;
            } else {
                const auto value = static_cast<std::int64_t>(left);
                field = value < right ? 0x8u : value > right ? 0x4u : 0x2u;
            }
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
        if (xo == 0x000) { // mcrf
            const auto destination_field = (instruction >> 23) & 7u;
            const auto source_field = (instruction >> 18) & 7u;
            const auto destination_shift = (7u - destination_field) * 4u;
            const auto source_shift = (7u - source_field) * 4u;
            const auto source = (registers_.condition_register >> source_shift) & 0xfu;
            registers_.condition_register =
                (registers_.condition_register & ~(0xfu << destination_shift)) |
                (source << destination_shift);
        } else if (xo == 0x010) {
            if (BranchCondition(bo, bi)) branch(registers_.lr & ~3ull, (instruction & 1u) != 0);
        } else if (xo == 0x210) {
            if (BranchCondition(bo, bi)) branch(registers_.ctr & ~3ull, (instruction & 1u) != 0);
        } else if (xo == 0x096) { // isync
            // Instruction synchronization has no observable effect in this
            // interpreter because code and data share the same guest map.
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
    case 0x17: { // rlwnm
        const auto shift = static_cast<unsigned>(registers_.gpr[rb] & 0x1f);
        const auto mb = (instruction >> 6) & 0x1f;
        const auto me = (instruction >> 1) & 0x1f;
        const auto value = RotateLeft32(
            static_cast<std::uint32_t>(registers_.gpr[rt]), shift) &
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
        const auto effective = address(immediate);
        const auto result = memory_.Read(effective, value);
        if (!result.ok()) { error = result.error + " at " + Hex(effective); return StepResult::MemoryFault; }
        registers_.gpr[rt] = value[0];
        break;
    }
    case 0x23: { // lbzu
        const auto effective = address(immediate);
        std::array<std::uint8_t, 1> value{};
        if (ra == 0 || !memory_.Read(effective, value).ok()) {
            error = "guest byte load-update failed at " + Hex(effective);
            return StepResult::MemoryFault;
        }
        registers_.gpr[rt] = value[0];
        registers_.gpr[ra] = effective;
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
        const auto effective = address(immediate);
        const auto result = memory_.Write(effective, value);
        if (!result.ok()) { error = result.error + " at " + Hex(effective); return StepResult::MemoryFault; }
        break;
    }
    case 0x27: { // stbu
        const std::array<std::uint8_t, 1> value{
            static_cast<std::uint8_t>(registers_.gpr[rt])};
        const auto effective = address(immediate);
        if (ra == 0 || !memory_.Write(effective, value).ok()) {
            error = "guest byte store-update failed at " + Hex(effective);
            return StepResult::MemoryFault;
        }
        registers_.gpr[ra] = effective;
        break;
    }
    case 0x28: { // lhz
        std::array<std::uint8_t, 2> value{};
        const auto effective = address(immediate);
        const auto result = memory_.Read(effective, value);
        if (!result.ok()) { error = result.error + " at " + Hex(effective); return StepResult::MemoryFault; }
        registers_.gpr[rt] = (static_cast<std::uint64_t>(value[0]) << 8) | value[1];
        break;
    }
    case 0x29: { // lhzu
        const auto effective = address(immediate);
        std::array<std::uint8_t, 2> value{};
        if (ra == 0 || !memory_.Read(effective, value).ok()) {
            error = "guest halfword load-update failed at " + Hex(effective);
            return StepResult::MemoryFault;
        }
        registers_.gpr[rt] = (static_cast<std::uint64_t>(value[0]) << 8) | value[1];
        registers_.gpr[ra] = effective;
        break;
    }
    case 0x2a: { // lha
        std::array<std::uint8_t, 2> value{};
        const auto effective = address(immediate);
        const auto result = memory_.Read(effective, value);
        if (!result.ok()) { error = result.error + " at " + Hex(effective); return StepResult::MemoryFault; }
        const auto half = static_cast<std::int16_t>((value[0] << 8) | value[1]);
        registers_.gpr[rt] = static_cast<std::int64_t>(half);
        break;
    }
    case 0x2b: { // lhau
        const auto effective = address(immediate);
        std::array<std::uint8_t, 2> value{};
        if (ra == 0 || !memory_.Read(effective, value).ok()) {
            error = "guest signed-halfword load-update failed at " + Hex(effective);
            return StepResult::MemoryFault;
        }
        const auto half = static_cast<std::int16_t>((value[0] << 8) | value[1]);
        registers_.gpr[rt] = static_cast<std::int64_t>(half);
        registers_.gpr[ra] = effective;
        break;
    }
    case 0x2c: { // sth
        const std::array<std::uint8_t, 2> value{
            static_cast<std::uint8_t>(registers_.gpr[rt] >> 8),
            static_cast<std::uint8_t>(registers_.gpr[rt])};
        const auto effective = address(immediate);
        const auto result = memory_.Write(effective, value);
        if (!result.ok()) { error = result.error + " at " + Hex(effective); return StepResult::MemoryFault; }
        break;
    }
    case 0x2d: { // sthu
        const std::array<std::uint8_t, 2> value{
            static_cast<std::uint8_t>(registers_.gpr[rt] >> 8),
            static_cast<std::uint8_t>(registers_.gpr[rt])};
        const auto effective = address(immediate);
        if (ra == 0 || !memory_.Write(effective, value).ok()) {
            error = "guest halfword store-update failed at " + Hex(effective);
            return StepResult::MemoryFault;
        }
        registers_.gpr[ra] = effective;
        break;
    }
    case 0x30: { // lfs
        std::uint32_t value = 0;
        if (!ReadU32(address(immediate), value, error)) return StepResult::MemoryFault;
        registers_.fpr[rt] = value;
        break;
    }
    case 0x31: { // lfsu
        const auto effective = address(immediate);
        std::uint32_t value = 0;
        if (ra == 0 || !ReadU32(effective, value, error)) return StepResult::MemoryFault;
        registers_.fpr[rt] = value;
        registers_.gpr[ra] = effective;
        break;
    }
    case 0x32: { // lfd
        std::uint64_t value = 0;
        if (!ReadU64(address(immediate), value, error)) return StepResult::MemoryFault;
        registers_.fpr[rt] = value;
        break;
    }
    case 0x33: { // lfdu
        const auto effective = address(immediate);
        std::uint64_t value = 0;
        if (ra == 0 || !ReadU64(effective, value, error)) return StepResult::MemoryFault;
        registers_.fpr[rt] = value;
        registers_.gpr[ra] = effective;
        break;
    }
    case 0x34: { // stfs
        if (!WriteU32(address(immediate), static_cast<std::uint32_t>(registers_.fpr[rt]), error)) {
            return StepResult::MemoryFault;
        }
        break;
    }
    case 0x35: { // stfsu
        const auto effective = address(immediate);
        if (ra == 0 || !WriteU32(effective, static_cast<std::uint32_t>(registers_.fpr[rt]), error)) {
            return StepResult::MemoryFault;
        }
        registers_.gpr[ra] = effective;
        break;
    }
    case 0x36: { // stfd
        if (!WriteU64(address(immediate), registers_.fpr[rt], error)) return StepResult::MemoryFault;
        break;
    }
    case 0x37: { // stfdu
        const auto effective = address(immediate);
        if (ra == 0 || !WriteU64(effective, registers_.fpr[rt], error)) return StepResult::MemoryFault;
        registers_.gpr[ra] = effective;
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
            // rldimi inserts the rotated source into the destination while
            // preserving the bits outside the MB..63 mask.
            const auto mask = MaskFromMbMe64(mb, 63);
            registers_.gpr[ra] = (registers_.gpr[ra] & ~mask) | (value & mask);
        }
        break;
    }
    case 0x1f: {
        const auto xo = (instruction >> 1) & 0x3ffu;
        switch (xo) {
        case 0x068: // neg
            registers_.gpr[rt] = static_cast<std::uint64_t>(-
                static_cast<std::int64_t>(registers_.gpr[ra]));
            if ((instruction & 1u) != 0) {
                SetCr0FromSigned(static_cast<std::int64_t>(registers_.gpr[rt]));
            }
            break;
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
        case 0x090: { // mtcrf
            const auto field_mask = (instruction >> 12) & 0xffu;
            const auto source = static_cast<std::uint32_t>(registers_.gpr[rt]);
            for (unsigned field = 0; field < 8; ++field) {
                if ((field_mask & (1u << (7u - field))) == 0) continue;
                const auto shift = (7u - field) * 4u;
                registers_.condition_register =
                    (registers_.condition_register & ~(0xfu << shift)) |
                    (source & (0xfu << shift));
            }
            break;
        }
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
        case 0x39a: { // extsh (XO 922)
            registers_.gpr[ra] = static_cast<std::int64_t>(
                static_cast<std::int16_t>(registers_.gpr[rt]));
            if ((instruction & 1u) != 0) {
                SetCr0FromSigned(static_cast<std::int64_t>(registers_.gpr[ra]));
            }
            break;
        }
        case 0x3ba: { // extsb (XO 954)
            registers_.gpr[ra] = static_cast<std::int64_t>(
                static_cast<std::int8_t>(registers_.gpr[rt]));
            if ((instruction & 1u) != 0) {
                SetCr0FromSigned(static_cast<std::int64_t>(registers_.gpr[ra]));
            }
            break;
        }
        case 0x3da: { // extsw (XO 986)
            registers_.gpr[ra] = static_cast<std::int64_t>(
                static_cast<std::int32_t>(registers_.gpr[rt]));
            if ((instruction & 1u) != 0) {
                SetCr0FromSigned(static_cast<std::int64_t>(registers_.gpr[ra]));
            }
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
        case 0x218: { // srw
            const auto shift = registers_.gpr[rb] & 0x3fu;
            registers_.gpr[ra] = shift >= 32 ? 0 :
                static_cast<std::uint32_t>(registers_.gpr[rt]) >> shift;
            if ((instruction & 1u) != 0) SetCr0FromLogical(registers_.gpr[ra]);
            break;
        }
        case 0x21b: { // srd
            const auto shift = registers_.gpr[rb] & 0x7fu;
            registers_.gpr[ra] = shift >= 64 ? 0 : registers_.gpr[rt] >> shift;
            if ((instruction & 1u) != 0) SetCr0FromLogical(registers_.gpr[ra]);
            break;
        }
        case 0x338: { // srawi
            const auto shift = (instruction >> 11) & 0x1fu;
            const auto value = static_cast<std::int32_t>(registers_.gpr[rt]);
            registers_.gpr[ra] = static_cast<std::int64_t>(value >> shift);
            if ((instruction & 1u) != 0) SetCr0FromSigned(
                static_cast<std::int64_t>(registers_.gpr[ra]));
            break;
        }
        case 0x33a: // sradi, SH[5] = 0
        case 0x33b: { // sradi, SH[5] = 1
            const auto shift = ((instruction >> 11) & 0x1fu) |
                               ((xo & 1u) << 5);
            const auto value = static_cast<std::int64_t>(registers_.gpr[rt]);
            registers_.gpr[ra] = shift == 0 ? value : value >> shift;
            constexpr std::uint32_t kXerCarry = 1u << 29;
            const auto shifted_out = shift == 0 ? UINT64_C(0)
                : registers_.gpr[rt] & ((UINT64_C(1) << shift) - 1);
            if (value < 0 && shifted_out != 0) registers_.xer |= kXerCarry;
            else registers_.xer &= ~kXerCarry;
            if ((instruction & 1u) != 0) SetCr0FromSigned(
                static_cast<std::int64_t>(registers_.gpr[ra]));
            break;
        }
        case 0x036: // dcbst
        case 0x056: // dcbf
        case 0x0f6: // dcbtst
        case 0x116: // dcbt
        case 0x196: // icbi
        case 0x3f6: // dcbz
            break;
        case 0x01c: // and
            registers_.gpr[ra] = registers_.gpr[rt] & registers_.gpr[rb];
            if ((instruction & 1u) != 0) SetCr0FromLogical(registers_.gpr[ra]);
            break;
        case 0x07c: // nor
            registers_.gpr[ra] = ~(registers_.gpr[rt] | registers_.gpr[rb]);
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
            registers_.gpr[rt] = registers_.gpr[ra] + registers_.gpr[rb];
            if ((instruction & 1u) != 0) SetCr0FromSigned(static_cast<std::int64_t>(registers_.gpr[rt]));
            break;
        case 0x0ca: { // addze
            constexpr std::uint32_t kXerCarry = 1u << 29;
            const auto carry = (registers_.xer & kXerCarry) != 0 ? 1u : 0u;
            registers_.gpr[rt] = registers_.gpr[ra] + carry;
            if ((instruction & 1u) != 0) {
                SetCr0FromSigned(static_cast<std::int64_t>(registers_.gpr[rt]));
            }
            break;
        }
        case 0x028: // subf
            registers_.gpr[rt] = registers_.gpr[rb] - registers_.gpr[ra];
            if ((instruction & 1u) != 0) SetCr0FromSigned(static_cast<std::int64_t>(registers_.gpr[rt]));
            break;
        case 0x1c9: // divdu
            // The non-overflow form is not a trapping instruction. RPCS3
            // produces zero for a zero divisor, which lets firmware probe
            // counters without terminating its PPU thread.
            registers_.gpr[rt] = registers_.gpr[rb] == 0
                ? 0 : registers_.gpr[ra] / registers_.gpr[rb];
            if ((instruction & 1u) != 0) SetCr0FromLogical(registers_.gpr[rt]);
            break;
        case 0x009: { // mulhdu
            const auto product = static_cast<unsigned __int128>(
                registers_.gpr[ra]) * registers_.gpr[rb];
            registers_.gpr[rt] = static_cast<std::uint64_t>(product >> 64);
            if ((instruction & 1u) != 0) SetCr0FromLogical(registers_.gpr[rt]);
            break;
        }
        case 0x049: { // mulhd
            const auto product = static_cast<__int128>(
                static_cast<std::int64_t>(registers_.gpr[ra])) *
                static_cast<std::int64_t>(registers_.gpr[rb]);
            registers_.gpr[rt] = static_cast<std::uint64_t>(product >> 64);
            if ((instruction & 1u) != 0) {
                SetCr0FromSigned(static_cast<std::int64_t>(registers_.gpr[rt]));
            }
            break;
        }
        case 0x04b: { // mulhw
            const auto left = static_cast<std::int32_t>(registers_.gpr[ra]);
            const auto right = static_cast<std::int32_t>(registers_.gpr[rb]);
            const auto product = static_cast<std::int64_t>(left) * right;
            const auto high = static_cast<std::int32_t>(
                static_cast<std::uint64_t>(product) >> 32);
            registers_.gpr[rt] = static_cast<std::uint64_t>(
                static_cast<std::int64_t>(high));
            if ((instruction & 1u) != 0) {
                SetCr0FromSigned(static_cast<std::int64_t>(high));
            }
            break;
        }
        case 0x00b: { // mulhwu
            const auto product = static_cast<std::uint64_t>(
                static_cast<std::uint32_t>(registers_.gpr[ra])) *
                static_cast<std::uint32_t>(registers_.gpr[rb]);
            registers_.gpr[rt] = product >> 32;
            if ((instruction & 1u) != 0) {
                SetCr0FromLogical(registers_.gpr[rt]);
            }
            break;
        }
        case 0x007: { // lvebx
            // VMX element loads place the byte at the lane selected by the
            // low address bits and preserve the other lanes.  PPU memory is
            // big-endian, so the guest byte order is already the vector lane
            // order used by this compact register model.
            const auto effective = (ra == 0 ? 0ull : registers_.gpr[ra]) +
                                   registers_.gpr[rb];
            std::array<std::uint8_t, 1> value{};
            const auto result = memory_.Read(effective, value);
            if (!result.ok()) {
                error = result.error + " at " + Hex(effective);
                return StepResult::MemoryFault;
            }
            registers_.vr[rt][effective & 0xfu] = value[0];
            break;
        }
        case 0x1e9: { // divd
            const auto divisor = static_cast<std::int64_t>(registers_.gpr[rb]);
            if (divisor == 0) {
                error = "PPU signed divide by zero at " + Hex(pc);
                return StepResult::Halted;
            }
            const auto dividend = static_cast<std::int64_t>(registers_.gpr[ra]);
            // PowerPC leaves the overflow result implementation-defined. The
            // architectural implementations used by VSH retain the signed
            // minimum value for INT64_MIN / -1; avoid C++ signed-overflow UB.
            const auto quotient = dividend == INT64_MIN && divisor == -1
                ? INT64_MIN
                : dividend / divisor;
            registers_.gpr[rt] = static_cast<std::uint64_t>(quotient);
            if ((instruction & 1u) != 0) SetCr0FromSigned(quotient);
            break;
        }
        case 0x0e9: // mulld
            registers_.gpr[rt] = registers_.gpr[ra] * registers_.gpr[rb];
            if ((instruction & 1u) != 0) SetCr0FromLogical(registers_.gpr[rt]);
            break;
        case 0x0eb: { // mullw
            const auto left = static_cast<std::int32_t>(registers_.gpr[ra]);
            const auto right = static_cast<std::int32_t>(registers_.gpr[rb]);
            registers_.gpr[rt] = static_cast<std::int64_t>(
                static_cast<std::int32_t>(left * right));
            if ((instruction & 1u) != 0) SetCr0FromSigned(static_cast<std::int64_t>(registers_.gpr[rt]));
            break;
        }
        case 0x1cb: // divwu
            registers_.gpr[rt] = static_cast<std::uint32_t>(registers_.gpr[rb]) == 0
                ? 0 : static_cast<std::uint32_t>(registers_.gpr[ra]) /
                      static_cast<std::uint32_t>(registers_.gpr[rb]);
            if ((instruction & 1u) != 0) SetCr0FromLogical(registers_.gpr[rt]);
            break;
        case 0x1eb: { // divw
            const auto divisor = static_cast<std::int32_t>(registers_.gpr[rb]);
            if (divisor == 0) {
                error = "PPU signed word divide by zero at " + Hex(pc);
                return StepResult::Halted;
            }
            const auto dividend = static_cast<std::int32_t>(registers_.gpr[ra]);
            const auto quotient = dividend == INT32_MIN && divisor == -1
                ? INT32_MIN
                : dividend / divisor;
            registers_.gpr[rt] = static_cast<std::uint64_t>(
                static_cast<std::int64_t>(quotient));
            if ((instruction & 1u) != 0) {
                SetCr0FromSigned(static_cast<std::int64_t>(quotient));
            }
            break;
        }
        case 0x014: { // lwarx (reservation is not contended in the first runtime)
            const auto effective = (ra == 0 ? 0ull : registers_.gpr[ra]) + registers_.gpr[rb];
            std::uint32_t value = 0;
            if (!ReadU32(effective, value, error)) return StepResult::MemoryFault;
            registers_.gpr[rt] = value;
            break;
        }
        case 0x015: { // ldx
            const auto effective = (ra == 0 ? 0ull : registers_.gpr[ra]) +
                                   registers_.gpr[rb];
            std::uint64_t value = 0;
            if (!ReadU64(effective, value, error)) return StepResult::MemoryFault;
            registers_.gpr[rt] = value;
            break;
        }
        case 0x017: { // lwzx
            const auto effective = (ra == 0 ? 0ull : registers_.gpr[ra]) + registers_.gpr[rb];
            std::uint32_t value = 0;
            if (!ReadU32(effective, value, error)) return StepResult::MemoryFault;
            registers_.gpr[rt] = value;
            break;
        }
        case 0x054: { // ldarx (reservation is not contended in the first runtime)
            const auto effective = (ra == 0 ? 0ull : registers_.gpr[ra]) + registers_.gpr[rb];
            std::uint64_t value = 0;
            if (!ReadU64(effective, value, error)) return StepResult::MemoryFault;
            registers_.gpr[rt] = value;
            break;
        }
        case 0x096: { // stwcx. (the single PPU runtime always owns the reservation)
            const auto effective = (ra == 0 ? 0ull : registers_.gpr[ra]) + registers_.gpr[rb];
            if (!WriteU32(effective, static_cast<std::uint32_t>(registers_.gpr[rt]), error)) {
                return StepResult::MemoryFault;
            }
            // A successful conditional store reports CR0.EQ. Setting the
            // logical result to one would report GT and make the usual
            // following bne retry a store that already succeeded.
            SetCr0FromLogical(0);
            break;
        }
        case 0x0d6: { // stdcx.
            const auto effective = (ra == 0 ? 0ull : registers_.gpr[ra]) + registers_.gpr[rb];
            if (!WriteU64(effective, registers_.gpr[rt], error)) return StepResult::MemoryFault;
            // A successful conditional store sets CR0.EQ; bne then falls
            // through to the post-store path.
            SetCr0FromLogical(0);
            break;
        }
        case 0x095: { // stdx
            const auto effective = (ra == 0 ? 0ull : registers_.gpr[ra]) + registers_.gpr[rb];
            if (!WriteU64(effective, registers_.gpr[rt], error)) return StepResult::MemoryFault;
            break;
        }
        case 0x057: { // lbzx
            const auto effective = (ra == 0 ? 0ull : registers_.gpr[ra]) + registers_.gpr[rb];
            std::array<std::uint8_t, 1> value{};
            const auto result = memory_.Read(effective, value);
            if (!result.ok()) { error = result.error + " at " + Hex(effective); return StepResult::MemoryFault; }
            registers_.gpr[rt] = value[0];
            break;
        }
        case 0x097: { // stwx
            const auto effective = (ra == 0 ? 0ull : registers_.gpr[ra]) + registers_.gpr[rb];
            if (!WriteU32(effective, static_cast<std::uint32_t>(registers_.gpr[rt]), error)) {
                return StepResult::MemoryFault;
            }
            break;
        }
        case 0x117: { // lhzx
            const auto effective = (ra == 0 ? 0ull : registers_.gpr[ra]) + registers_.gpr[rb];
            std::array<std::uint8_t, 2> value{};
            const auto result = memory_.Read(effective, value);
            if (!result.ok()) { error = result.error + " at " + Hex(effective); return StepResult::MemoryFault; }
            registers_.gpr[rt] = (static_cast<std::uint64_t>(value[0]) << 8) | value[1];
            break;
        }
        case 0x157: { // lhax
            const auto effective = (ra == 0 ? 0ull : registers_.gpr[ra]) + registers_.gpr[rb];
            std::array<std::uint8_t, 2> value{};
            const auto result = memory_.Read(effective, value);
            if (!result.ok()) { error = result.error + " at " + Hex(effective); return StepResult::MemoryFault; }
            const auto half = static_cast<std::int16_t>((value[0] << 8) | value[1]);
            registers_.gpr[rt] = static_cast<std::int64_t>(half);
            break;
        }
        case 0x197: { // sthx
            const auto effective = (ra == 0 ? 0ull : registers_.gpr[ra]) + registers_.gpr[rb];
            const std::array<std::uint8_t, 2> value{
                static_cast<std::uint8_t>(registers_.gpr[rt] >> 8),
                static_cast<std::uint8_t>(registers_.gpr[rt])};
            const auto result = memory_.Write(effective, value);
            if (!result.ok()) { error = result.error + " at " + Hex(effective); return StepResult::MemoryFault; }
            break;
        }
        case 0x0d7: { // stbx
            const auto effective = (ra == 0 ? 0ull : registers_.gpr[ra]) + registers_.gpr[rb];
            const std::array<std::uint8_t, 1> value{
                static_cast<std::uint8_t>(registers_.gpr[rt])};
            const auto result = memory_.Write(effective, value);
            if (!result.ok()) { error = result.error + " at " + Hex(effective); return StepResult::MemoryFault; }
            break;
        }
        case 0x0e7: { // stvx
            const auto effective = (ra == 0 ? 0ull : registers_.gpr[ra]) +
                                   registers_.gpr[rb];
            const auto aligned = effective & ~0xfu;
            const auto result = memory_.Write(
                aligned, std::span<const std::uint8_t>(registers_.vr[rt]));
            if (!result.ok()) {
                error = result.error + " at " + Hex(aligned);
                return StepResult::MemoryFault;
            }
            break;
        }
        case 0x173: { // mftb/mftbl/mftbu
            const auto spr = ((instruction >> 16) & 0x1fu) |
                             (((instruction >> 11) & 0x1fu) << 5);
            if (spr == 268) registers_.gpr[rt] = timebase_ & UINT64_C(0xffffffff);
            else if (spr == 269) registers_.gpr[rt] = timebase_ >> 32;
            else registers_.gpr[rt] = 0;
            break;
        }
        case 0x150: // isel
            registers_.gpr[rt] = ((registers_.condition_register >> (31u - (instruction >> 6 & 0x1f))) & 1u)
                ? registers_.gpr[ra] : registers_.gpr[rb];
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
        case 0x256: // sync
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
    case 0x3b: {
        const auto xo = (instruction >> 1) & 0x3ffu;
        switch (xo) {
        case 0x012: { // fdivs
            const auto left = static_cast<float>(
                std::bit_cast<double>(registers_.fpr[ra]));
            const auto right = static_cast<float>(
                std::bit_cast<double>(registers_.fpr[rb]));
            const auto result = left / right;
            registers_.fpr[rt] = std::bit_cast<std::uint64_t>(
                static_cast<double>(result));
            break;
        }
        default:
            error = "unsupported PPU single-precision opcode " + Hex(xo) +
                    " at " + Hex(pc);
            return StepResult::UnsupportedInstruction;
        }
        break;
    }
    case 0x3f: {
        const auto xo = (instruction >> 1) & 0x3ffu;
        switch (xo) {
        case 0x048: // fmr
            registers_.fpr[rt] = registers_.fpr[rb];
            break;
        case 0x34e: { // fcfid
            const auto integer = static_cast<std::int64_t>(registers_.fpr[rb]);
            registers_.fpr[rt] = std::bit_cast<std::uint64_t>(
                static_cast<double>(integer));
            break;
        }
        case 0x00c: { // frsp
            const auto source = std::bit_cast<double>(registers_.fpr[rb]);
            const auto rounded = static_cast<float>(source);
            registers_.fpr[rt] = std::bit_cast<std::uint64_t>(
                static_cast<double>(rounded));
            break;
        }
        default:
            error = "unsupported PPU floating-point opcode " + Hex(xo) +
                    " at " + Hex(pc);
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

PpuRunResult PpuRuntime::Run(std::size_t max_instructions,
                             const PpuSyscallHandler& syscall_handler,
                             const PpuInstructionHook& instruction_hook) {
    PpuRunResult result;
    trace_.clear();
    for (; result.instructions < max_instructions; ++result.instructions) {
        if (instruction_hook) instruction_hook(registers_);
        timebase_ += 100;
        const auto step_pc = registers_.pc;
        std::uint32_t instruction = 0;
        std::string error;
        const auto step = Step(instruction, error);
        trace_.push_back({step_pc, instruction, registers_.gpr[0],
                          registers_.gpr[2], registers_.gpr[3],
                          registers_.gpr[4], registers_.gpr[5],
                          registers_.gpr[9], registers_.ctr});
        if (trace_.size() > 128) trace_.erase(trace_.begin());
        result.instruction = instruction;
        if (step == StepResult::Continue) continue;
        if (step == StepResult::Syscall && syscall_handler) {
            if (syscall_handler(registers_, error)) {
                continue;
            }
            if (error.empty()) {
                error = "unhandled PS3 LV2 syscall 0x";
                std::ostringstream stream;
                stream << std::hex << registers_.gpr[11];
                error += stream.str();
            }
        }
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
