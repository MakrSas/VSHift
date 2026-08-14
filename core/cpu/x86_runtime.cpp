#include "core/cpu/x86_runtime.h"

#include <array>
#include <cstdio>
#include <cstring>

namespace vshift::cpu {

namespace {

constexpr std::uint64_t kFlagCarry = 1ull << 0;
constexpr std::uint64_t kFlagZero = 1ull << 6;
constexpr std::uint64_t kFlagSign = 1ull << 7;
constexpr std::uint64_t kFlagOverflow = 1ull << 11;

struct Cursor final {
    memory::GuestMemory& memory;
    std::uint64_t address;

    bool Read8(std::uint8_t& value) {
        const auto result = memory.Read(address, std::span(&value, 1));
        if (!result.ok()) {
            return false;
        }
        ++address;
        return true;
    }

    template <typename T>
    bool Read(T& value) {
        std::array<std::uint8_t, sizeof(T)> bytes{};
        const auto result = memory.Read(address, bytes);
        if (!result.ok()) {
            return false;
        }
        std::memcpy(&value, bytes.data(), sizeof(value));
        address += sizeof(T);
        return true;
    }
};

struct Operand final {
    bool is_register = false;
    std::uint8_t reg = 0;
    std::uint64_t address = 0;
};

bool AddSigned(std::uint64_t base, std::int64_t displacement,
               std::uint64_t& result) {
    if (displacement >= 0) {
        result = base + static_cast<std::uint64_t>(displacement);
        return result >= base;
    }
    const auto magnitude = static_cast<std::uint64_t>(-(displacement + 1)) + 1;
    if (base < magnitude) {
        return false;
    }
    result = base - magnitude;
    return true;
}

bool DecodeOperand(Cursor& cursor,
                   GuestRegisters& registers,
                   std::uint8_t rex,
                   std::uint8_t modrm,
                   Operand& operand) {
    const auto mod = static_cast<std::uint8_t>(modrm >> 6);
    const auto rm = static_cast<std::uint8_t>(modrm & 7);
    if (mod == 3) {
        operand.is_register = true;
        operand.reg = static_cast<std::uint8_t>(rm | ((rex & 1) != 0 ? 8 : 0));
        return true;
    }

    std::uint64_t base = 0;
    std::int64_t displacement = 0;
    if (rm == 4) {
        std::uint8_t sib = 0;
        if (!cursor.Read8(sib)) {
            return false;
        }
        const auto scale = static_cast<std::uint8_t>(sib >> 6);
        const auto index = static_cast<std::uint8_t>((sib >> 3) & 7);
        const auto sibBase = static_cast<std::uint8_t>(sib & 7);
        if (index != 4) {
            base += registers.general[index | ((rex & 2) != 0 ? 8 : 0)] << scale;
        }
        if (mod == 0 && sibBase == 5) {
            std::int32_t absolute = 0;
            if (!cursor.Read(absolute)) {
                return false;
            }
            displacement = absolute;
        } else {
            base += registers.general[sibBase | ((rex & 1) != 0 ? 8 : 0)];
            if (mod == 1) {
                std::int8_t value = 0;
                if (!cursor.Read(value)) {
                    return false;
                }
                displacement = value;
            } else if (mod == 2) {
                std::int32_t value = 0;
                if (!cursor.Read(value)) {
                    return false;
                }
                displacement = value;
            }
        }
    } else if (mod == 0 && rm == 5) {
        std::int32_t relative = 0;
        if (!cursor.Read(relative)) {
            return false;
        }
        if (!AddSigned(cursor.address, relative, base)) {
            return false;
        }
        displacement = 0;
    } else {
        base = registers.general[rm | ((rex & 1) != 0 ? 8 : 0)];
        if (mod == 1) {
            std::int8_t value = 0;
            if (!cursor.Read(value)) {
                return false;
            }
            displacement = value;
        } else if (mod == 2) {
            std::int32_t value = 0;
            if (!cursor.Read(value)) {
                return false;
            }
            displacement = value;
        }
    }

    operand.is_register = false;
    return AddSigned(base, displacement, operand.address);
}

template <typename T>
bool ReadOperand(memory::GuestMemory& memory, const Operand& operand, T& value,
                 const GuestRegisters& registers) {
    if (operand.is_register) {
        std::memcpy(&value, &registers.general[operand.reg], sizeof(T));
        return true;
    }
    std::array<std::uint8_t, sizeof(T)> bytes{};
    const auto result = memory.Read(operand.address, bytes);
    if (!result.ok()) {
        return false;
    }
    std::memcpy(&value, bytes.data(), sizeof(T));
    return true;
}

template <typename T>
bool WriteOperand(memory::GuestMemory& memory, const Operand& operand, T value,
                  GuestRegisters& registers) {
    if (operand.is_register) {
        std::memcpy(&registers.general[operand.reg], &value, sizeof(T));
        return true;
    }
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
    return memory.Write(operand.address, std::span(bytes, sizeof(T))).ok();
}

void SetLogicFlags(GuestRegisters& registers, std::uint64_t value,
                   unsigned bits) {
    const auto mask = bits == 32 ? 0xffff'ffffull : 0xffff'ffff'ffff'ffffull;
    registers.rflags &= ~(kFlagCarry | kFlagOverflow | kFlagZero | kFlagSign);
    if ((value & mask) == 0) {
        registers.rflags |= kFlagZero;
    }
    if ((value >> (bits - 1)) & 1) {
        registers.rflags |= kFlagSign;
    }
}

void SetSubFlags(GuestRegisters& registers, std::uint64_t left,
                 std::uint64_t right, std::uint64_t result, unsigned bits) {
    const auto mask = bits == 32 ? 0xffff'ffffull : 0xffff'ffff'ffff'ffffull;
    const auto sign = 1ull << (bits - 1);
    registers.rflags &= ~(kFlagCarry | kFlagOverflow | kFlagZero | kFlagSign);
    if ((left & mask) < (right & mask)) {
        registers.rflags |= kFlagCarry;
    }
    if ((result & mask) == 0) {
        registers.rflags |= kFlagZero;
    }
    if ((result & sign) != 0) {
        registers.rflags |= kFlagSign;
    }
    const bool leftNegative = (left & sign) != 0;
    const bool rightNegative = (right & sign) != 0;
    const bool resultNegative = (result & sign) != 0;
    if (leftNegative != rightNegative && resultNegative != leftNegative) {
        registers.rflags |= kFlagOverflow;
    }
}

bool Condition(std::uint8_t condition, const GuestRegisters& registers) {
    const bool zero = (registers.rflags & kFlagZero) != 0;
    const bool carry = (registers.rflags & kFlagCarry) != 0;
    const bool sign = (registers.rflags & kFlagSign) != 0;
    const bool overflow = (registers.rflags & kFlagOverflow) != 0;
    switch (condition & 0x0f) {
    case 0x0: return overflow;
    case 0x1: return !overflow;
    case 0x2: return carry;
    case 0x3: return !carry;
    case 0x4: return zero;
    case 0x5: return !zero;
    case 0x6: return carry || zero;
    case 0x7: return !carry && !zero;
    case 0x8: return sign;
    case 0x9: return !sign;
    case 0xa: return false; // parity is intentionally not guessed
    case 0xb: return true;
    case 0xc: return sign != overflow;
    case 0xd: return sign == overflow;
    case 0xe: return zero || sign != overflow;
    case 0xf: return !zero && sign == overflow;
    }
    return false;
}

bool ReadCodeByte(memory::GuestMemory& memory, std::uint64_t address,
                  std::uint8_t& byte) {
    return memory.Read(address, std::span(&byte, 1)).ok();
}

std::string OpcodeError(std::uint64_t rip, std::uint8_t opcode,
                        std::string_view detail = {}) {
    std::string error = "unsupported x86-64 opcode at 0x";
    char buffer[32]{};
    std::snprintf(buffer, sizeof(buffer), "%llx", rip);
    error += buffer;
    error += " (0x";
    std::snprintf(buffer, sizeof(buffer), "%02x", opcode);
    error += buffer;
    error += ')';
    if (!detail.empty()) {
        error += ": ";
        error += detail;
    }
    return error;
}

} // namespace

GuestCpuResult RunGuest(memory::GuestMemory& memory, std::uint64_t entry,
                        const GuestCpuConfig& config,
                        const SyscallHandler& syscall_handler) {
    GuestCpuResult result;
    result.registers.rip = entry;
    result.registers.rsp = config.stack_top - sizeof(std::uint64_t);
    const auto stackMapping = memory.Map({
        config.stack_top - config.stack_size,
        config.stack_size,
        memory::kPermissionRead | memory::kPermissionWrite,
    });
    if (!stackMapping.ok() &&
        !stackMapping.error.starts_with("guest mapping overlaps")) {
        result.error = stackMapping.error;
        return result;
    }
    const std::uint64_t sentinel = 0;
    if (!memory.Write(result.registers.rsp,
                      std::span(reinterpret_cast<const std::uint8_t*>(&sentinel),
                                sizeof(sentinel))).ok()) {
        result.error = "guest stack could not be initialized";
        return result;
    }

    while (result.instructions < config.max_instructions) {
        const auto instructionRip = result.registers.rip;
        Cursor cursor{memory, instructionRip};
        std::uint8_t rex = 0;
        std::uint8_t opcode = 0;
        if (!cursor.Read8(opcode)) {
            result.error = "guest instruction fetch failed";
            return result;
        }
        if ((opcode & 0xf0) == 0x40) {
            rex = opcode;
            if (!cursor.Read8(opcode)) {
                result.error = "truncated REX-prefixed instruction";
                return result;
            }
        }

        bool advance = true;
        switch (opcode) {
        case 0x90:
            break;
        case 0xc3: {
            std::uint64_t target = 0;
            if (!memory.Read(result.registers.rsp,
                             std::span(reinterpret_cast<std::uint8_t*>(&target),
                                       sizeof(target))).ok()) {
                result.error = "guest ret stack read failed";
                return result;
            }
            result.registers.rsp += sizeof(target);
            if (target == 0) {
                result.returned = true;
                ++result.instructions;
                return result;
            }
            result.registers.rip = target;
            advance = false;
            break;
        }
        case 0xc2: {
            std::uint16_t adjust = 0;
            if (!cursor.Read(adjust)) {
                result.error = "truncated ret";
                return result;
            }
            std::uint64_t target = 0;
            if (!memory.Read(result.registers.rsp,
                             std::span(reinterpret_cast<std::uint8_t*>(&target),
                                       sizeof(target))).ok()) {
                result.error = "guest ret stack read failed";
                return result;
            }
            result.registers.rsp += sizeof(target) + adjust;
            if (target == 0) {
                result.returned = true;
                ++result.instructions;
                return result;
            }
            result.registers.rip = target;
            advance = false;
            break;
        }
        case 0x50 ... 0x57: {
            const auto reg = static_cast<std::uint8_t>((opcode - 0x50) |
                                                        ((rex & 1) ? 8 : 0));
            result.registers.rsp -= 8;
            if (!memory.Write(result.registers.rsp,
                              std::span(reinterpret_cast<const std::uint8_t*>(
                                            &result.registers.general[reg]),
                                        8)).ok()) {
                result.error = "guest push failed";
                return result;
            }
            break;
        }
        case 0x58 ... 0x5f: {
            const auto reg = static_cast<std::uint8_t>((opcode - 0x58) |
                                                        ((rex & 1) ? 8 : 0));
            if (!memory.Read(result.registers.rsp,
                             std::span(reinterpret_cast<std::uint8_t*>(
                                           &result.registers.general[reg]),
                                       8)).ok()) {
                result.error = "guest pop failed";
                return result;
            }
            result.registers.rsp += 8;
            break;
        }
        case 0xb8 ... 0xbf: {
            const auto reg = static_cast<std::uint8_t>((opcode - 0xb8) |
                                                        ((rex & 1) ? 8 : 0));
            if ((rex & 8) != 0) {
                std::uint64_t value = 0;
                if (!cursor.Read(value)) {
                    result.error = "truncated mov imm64";
                    return result;
                }
                result.registers.general[reg] = value;
            } else {
                std::uint32_t value = 0;
                if (!cursor.Read(value)) {
                    result.error = "truncated mov imm32";
                    return result;
                }
                result.registers.general[reg] = value;
            }
            break;
        }
        case 0xe8: {
            std::int32_t displacement = 0;
            if (!cursor.Read(displacement)) {
                result.error = "truncated call";
                return result;
            }
            const auto returnAddress = cursor.address;
            result.registers.rsp -= 8;
            if (!memory.Write(result.registers.rsp,
                              std::span(reinterpret_cast<const std::uint8_t*>(
                                            &returnAddress),
                                        8)).ok()) {
                result.error = "guest call stack write failed";
                return result;
            }
            if (!AddSigned(returnAddress, displacement, result.registers.rip)) {
                result.error = "guest call target overflow";
                return result;
            }
            advance = false;
            break;
        }
        case 0xe9:
        case 0xeb: {
            std::int64_t displacement = 0;
            if (opcode == 0xe9) {
                std::int32_t value = 0;
                if (!cursor.Read(value)) {
                    result.error = "truncated near jump";
                    return result;
                }
                displacement = value;
            } else {
                std::int8_t value = 0;
                if (!cursor.Read(value)) {
                    result.error = "truncated short jump";
                    return result;
                }
                displacement = value;
            }
            if (!AddSigned(cursor.address, displacement, result.registers.rip)) {
                result.error = "guest jump target overflow";
                return result;
            }
            advance = false;
            break;
        }
        case 0x83: {
            std::uint8_t modrm = 0;
            std::int8_t immediate = 0;
            if (!cursor.Read8(modrm) || !cursor.Read(immediate)) {
                result.error = "truncated 0x83 instruction";
                return result;
            }
            const auto operation = static_cast<std::uint8_t>((modrm >> 3) & 7);
            if (operation != 0 && operation != 5) {
                result.error = OpcodeError(instructionRip, opcode,
                                            "only add/sub are implemented");
                return result;
            }
            Operand operand;
            if (!DecodeOperand(cursor, result.registers, rex, modrm, operand)) {
                result.error = "invalid 0x83 operand";
                return result;
            }
            const auto immediateValue = static_cast<std::int64_t>(immediate);
            std::uint64_t value = 0;
            if (!ReadOperand(memory, operand, value, result.registers)) {
                result.error = "guest 0x83 read failed";
                return result;
            }
            const auto resultValue = operation == 0
                                         ? value + immediateValue
                                         : value - immediateValue;
            if (!WriteOperand(memory, operand, resultValue,
                              result.registers)) {
                result.error = "guest 0x83 write failed";
                return result;
            }
            if (operation == 5) {
                SetSubFlags(result.registers, value,
                            static_cast<std::uint64_t>(immediateValue),
                            resultValue, 64);
            } else {
                SetLogicFlags(result.registers, resultValue, 64);
            }
            break;
        }
        case 0x74:
        case 0x75: {
            std::int8_t displacement = 0;
            if (!cursor.Read(displacement)) {
                result.error = "truncated conditional jump";
                return result;
            }
            if (Condition(static_cast<std::uint8_t>(opcode == 0x74 ? 4 : 5),
                          result.registers)) {
                if (!AddSigned(cursor.address, displacement,
                               result.registers.rip)) {
                    result.error = "conditional jump target overflow";
                    return result;
                }
                advance = false;
            }
            break;
        }
        case 0x31:
        case 0x33:
        case 0x39:
        case 0x3b:
        case 0x85:
        case 0x89:
        case 0x8b:
        case 0x8d: {
            std::uint8_t modrm = 0;
            if (!cursor.Read8(modrm)) {
                result.error = "truncated ModRM instruction";
                return result;
            }
            const auto mod = static_cast<std::uint8_t>(modrm >> 6);
            const auto reg = static_cast<std::uint8_t>(
                ((modrm >> 3) & 7) | ((rex & 4) ? 8 : 0));
            Operand operand;
            if (!DecodeOperand(cursor, result.registers, rex, modrm, operand)) {
                result.error = "invalid ModRM operand";
                return result;
            }
            if (opcode == 0x8d) {
                if (operand.is_register) {
                    result.error = OpcodeError(instructionRip, opcode,
                                               "lea requires memory operand");
                    return result;
                }
                result.registers.general[reg] = operand.address;
                break;
            }
            if (opcode == 0x89 || opcode == 0x8b) {
                if ((rex & 8) != 0) {
                    std::uint64_t value = result.registers.general[reg];
                    if (opcode == 0x89) {
                        if (!WriteOperand(memory, operand, value,
                                          result.registers)) {
                            result.error = "guest mov write failed";
                            return result;
                        }
                    } else if (!ReadOperand(memory, operand, value,
                                             result.registers)) {
                        result.error = "guest mov read failed";
                        return result;
                    } else {
                        result.registers.general[reg] = value;
                    }
                } else {
                    std::uint32_t value = static_cast<std::uint32_t>(
                        result.registers.general[reg]);
                    if (opcode == 0x89) {
                        if (!WriteOperand(memory, operand, value,
                                          result.registers)) {
                            result.error = "guest mov write failed";
                            return result;
                        }
                    } else if (!ReadOperand(memory, operand, value,
                                             result.registers)) {
                        result.error = "guest mov read failed";
                        return result;
                    } else {
                        result.registers.general[reg] = value;
                    }
                }
                break;
            }
            std::uint64_t left = 0;
            std::uint64_t right = result.registers.general[reg];
            if (!ReadOperand(memory, operand, left, result.registers)) {
                result.error = "guest arithmetic read failed";
                return result;
            }
            if (opcode == 0x31 || opcode == 0x33) {
                const auto value = left ^ right;
                if (opcode == 0x31) {
                    if (!WriteOperand(memory, operand, value,
                                      result.registers)) {
                        result.error = "guest xor write failed";
                        return result;
                    }
                } else {
                    result.registers.general[reg] = value;
                }
                SetLogicFlags(result.registers, value, 64);
            } else if (opcode == 0x85) {
                SetLogicFlags(result.registers, left & right, 64);
            } else {
                const auto value = opcode == 0x39 ? left - right : right - left;
                SetSubFlags(result.registers,
                            opcode == 0x39 ? left : right,
                            opcode == 0x39 ? right : left, value, 64);
            }
            (void)mod;
            break;
        }
        case 0x48: // unreachable after REX handling; documents invalid path
            result.error = OpcodeError(instructionRip, opcode);
            return result;
        case 0x0f: {
            std::uint8_t second = 0;
            if (!cursor.Read8(second)) {
                result.error = "truncated 0F instruction";
                return result;
            }
            if (second == 0x05) {
                if (!syscall_handler || !syscall_handler(result.registers)) {
                    result.error = OpcodeError(instructionRip, opcode,
                                               "syscall handler stopped guest");
                    return result;
                }
                break;
            }
            if (second == 0x84 || second == 0x85) {
                std::int32_t displacement = 0;
                if (!cursor.Read(displacement)) {
                    result.error = "truncated near conditional jump";
                    return result;
                }
                const auto condition = static_cast<std::uint8_t>(
                    second == 0x84 ? 4 : 5);
                if (Condition(condition, result.registers)) {
                    if (!AddSigned(cursor.address, displacement,
                                   result.registers.rip)) {
                        result.error = "near conditional jump target overflow";
                        return result;
                    }
                    advance = false;
                }
                break;
            }
            result.error = OpcodeError(instructionRip, second);
            return result;
        }
        default:
            result.error = OpcodeError(instructionRip, opcode);
            return result;
        }

        if (advance) {
            result.registers.rip = cursor.address;
        }
        ++result.instructions;
    }

    result.error = "guest instruction budget exhausted";
    return result;
}

} // namespace vshift::cpu
