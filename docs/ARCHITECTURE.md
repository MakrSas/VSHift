# VSHift architecture

Status: Milestone 0/1 plus firmware/loader foundation, 2026-08-13.

VSHift is a new modular emulator project. It does not merge RPCSX, KytyPS5,
SharpEmu, FEX, or Box64 into one tree. Those projects are research inputs; the
first implementation keeps the host/platform boundary explicit.

## Runtime layers

```text
PS5 SELF/ELF and system modules
            |
     firmware + VFS
            |
       PS5 HLE API
            |
  x86-64 decoder -> guest IR -> ARM64 backend
            |                         |
        guest memory          ExecutableMemory
            |
     GPU abstraction -> shader translation -> host renderer
            |
       iOS presentation / input / audio / haptics
```

The dependency direction is deliberately one-way:

- `core/cpu` must not include UIKit, Metal, Linux syscalls, or firmware data.
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
| `core/hle` | PS5-compatible services | memory, host platform adapters |
| `gpu/` | Guest commands and shader/resource translation | memory, host renderer |
| `app/ios` | iOS lifecycle and presentation | core, UIKit/Metal |

The IR module is intentionally still pending. The memory module now has a
sparse guest-address-space foundation used by synthetic ELF mapping tests.
