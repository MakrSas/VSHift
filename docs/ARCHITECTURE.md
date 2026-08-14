# VSHift architecture

Status: PS3 core integration started, 2026-08-14.

VSHift uses the upstream RPCS3 emulator core for PS3 instead of reimplementing
Cell emulation. The Qt/desktop frontend is not part of the mobile target.
VSHift owns the iOS lifecycle, presentation, input, audio, haptics, storage
permissions, and user-facing firmware workflow.

## Runtime layers

```text
PS3 PUP / installed dev_flash / VSH.self
            |
     firmware + VFS
            |
      RPCS3 rpcs3_emu
       |       |       |
    PPU/SPU  LV2/HLE  RSX
       |       |       |
       +-------+-------+
               |
       VSHift mobile adapters
        |       |       |       |
      video   input   audio  haptics
               |
       iOS / Metal frontend
```

The dependency direction is deliberately one-way:

- the upstream core must not include UIKit or VSHift UI state.
- shared input/audio/haptic interfaces must not depend on PS3-specific types.
- `ExecutableMemory` owns platform-specific executable-memory policy.
- guest addresses are handles into a memory subsystem; they are not assumed to
  equal host pointers.
- firmware is user-provided and remains outside the repository and IPA.
- iOS UI is a thin container around the emulator runtime.

## Milestone 1 boundary

The current PoC accepts this synthetic x86-64 program:

```text
mov eax, 40
add eax, 2
ret
```

It decodes three instructions, emits three AArch64 instructions, allocates
executable memory, flushes the instruction cache, and executes the function on
an ARM64 host. On non-ARM64 hosts it still validates decoding and emitted code
size but skips execution.

This is not an x86-64 emulator yet. The immediate values and instruction set
are intentionally constrained so that every new instruction can be added with
a focused test and a clear semantic definition.

## Planned module boundaries

| Module | Responsibility | First dependency |
| --- | --- | --- |
| `core/cpu/x86_decoder` | Decode guest instruction bytes | none |
| `core/cpu/ir` | Architecture-neutral operations | decoder |
| `core/cpu/arm64_backend` | Lower IR to AArch64 | IR, executable memory |
| `core/memory` | Guest address space and protection | platform virtual memory |
| `core/loader` | ELF/SELF headers and mappings | memory |
| `core/ps3` | RPCS3 ownership, firmware install, VSH lifecycle | RPCS3 `rpcs3_emu` |
| `core/frontend` | controller/audio/haptic contracts shared by PS3/PS4/PS5 | none |
| `app/ios` | iOS lifecycle and presentation | core, UIKit/Metal |

The IR module is intentionally still pending. The memory module now has a
sparse guest-address-space foundation used by synthetic ELF mapping tests.
