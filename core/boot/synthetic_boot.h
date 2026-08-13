#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace vshift::boot {

enum class ExecutionMode : std::uint8_t {
    Jit,
    JitLess,
};

struct SyntheticBootReport final {
    ExecutionMode mode = ExecutionMode::JitLess;
    std::uint64_t entry = 0;
    std::size_t mapped_segments = 0;
    std::uint32_t result = 0;
    std::string error;

    bool ok() const noexcept { return error.empty(); }
};

// Creates a firmware-independent ELF64 fixture whose mapped guest entry
// contains the same x86-64 mov/add/ret proof used by the initial JIT test.
std::vector<std::uint8_t> BuildSyntheticElfFixture();

// Exercises the complete bounded path: ELF parse, PT_LOAD mapping, guest
// entry read, x86 decode, shared IR, and either JIT or JIT-less execution.
SyntheticBootReport RunSyntheticElfBoot(
    std::span<const std::uint8_t> elf_image,
    ExecutionMode mode);

} // namespace vshift::boot
