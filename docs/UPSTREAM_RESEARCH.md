# Upstream research

Research snapshot: 2026-08-13. Each repository was fetched with a shallow
checkout and inspected at the commit below. This document records source
locations and conclusions; no upstream source files have been copied into
VSHift.

| Project | Commit inspected | Relevant source locations | License | Reuse decision |
| --- | --- | --- | --- | --- |
| RPCSX | `e8ae1481ab7ba04d5c6bef89dd852aabba2c88ff` | `rpcsx/main.cpp`; `kernel/orbis/src`; `rpcsx/gpu`; `rpcs3/Loader`; `rpcs3/Emu/CPU/Backends/AArch64` | GPL-2.0, with per-directory exceptions/licenses | Study boot orchestration, HLE contracts, GPU and memory behavior. Do not import its Linux/x86 runtime into the first iOS core. |
| KytyPS5 | `7a40dad90f8aea58a2f5a3ca552b6fefa31ad160` | `src/loader/elf.cpp`; `src/loader/runtimeLinker.cpp`; `src/loader/jit.h`; `src/common/virtualMemory.cpp`; `src/graphics/guest_gpu`; `src/graphics/shader/recompiler` | GPL-2.0; repository also contains separately licensed files | Study ELF/SELF metadata, runtime linking, memory protection, and SPIR-V pipeline. The current JIT helpers are x86-host stubs, not an iOS ARM64 backend. |
| SharpEmu | `7caf430aa9bdb1de2bac57f5d4634016933465f5` | `src/SharpEmu.Core/Cpu`; `src/SharpEmu.Core/Loader`; `src/SharpEmu.Core/Memory`; `src/SharpEmu.Libs/Gpu/Metal`; `src/SharpEmu.ShaderCompiler.Metal` | GPL-2.0-or-later plus separately licensed files | Study the clean subsystem decomposition and direct Metal path. Do not copy GPL code into the small PoC. |
| FEX | `71afe476751deac24adabd1adb575fd2337b6e0a` | `FEXCore/Source/Interface/IR`; `FEXCore/Source/Interface/Core/JIT`; `FEXCore/Source/Interface/IR/Passes/RegisterAllocationPass.cpp`; Linux signal/syscall layers | MIT for the core repository snapshot | Study IR, register allocation, block compilation, and AArch64 emission. Linux syscall emulation and signal ownership must be replaced for iOS. |
| Box64 | `8ed77f3d9344c621217f5fe3c74e1f21bd9de0fb` | `src/dynarec`; `src/dynarec/arm64`; `src/dynarec/dynablock.c`; `src/elfs`; signal and memory-protection code | MIT | Study ARM64 emitter structure, block cache/invalidation, and dirty-code handling. Do not assume its Linux signal/memory model works on iOS. |
| MoltenVK | `bd2c60d93190b4d7c0b94faaeac63d64b2b7fe6d` | `MoltenVK/MoltenVK`; `MoltenVKShaderConverter`; iOS Xcode schemes | Apache-2.0 | Use as an optional host backend after a renderer interface exists. It is not yet a VSHift dependency. |

## Feature comparison

| Feature | RPCSX | Kyty | SharpEmu | Our choice | Reason |
| --- | --- | --- | --- | --- | --- |
| VSH boot | `rpcsx/main.cpp` mounts `--fw`, selects system mode, defaults to `/mini-syscore.elf`, and calls `loadModuleFile`/`guestExec` | Loader/runtime-linker oriented; VSH behavior is not the first portable target | Focuses on ELF/game infrastructure and HLE | Reproduce the boot contract behind interfaces, not the Linux process | Keeps firmware, VFS, loader, HLE, and execution independently testable |
| CPU execution | AArch64 backend exists in the broader RPCSX/RPCS3 tree, while the current main path also relies on Linux signal/context behavior | Runtime linker invokes x86-64 code and patches it for host execution | Direct native x64 execution with host-specific fault handling | Demand-driven x86 decoder → IR → ARM64 DBT | iPhone has no Rosetta and cannot execute guest x86-64 natively |
| Memory | Guest virtual layout and page-fault/cache handling are tightly coupled to Linux | Explicit virtual-memory and protection wrappers | Host-memory abstractions plus Windows/POSIX backends | Guest address → host mapping API with platform backend | iOS address-space and executable-memory constraints must not leak into guest code |
| GPU | Vulkan/SPIR-V and AMD/GCN-related libraries | Mature guest GPU, shader recompiler, and Vulkan renderer | Vulkan plus a direct Metal implementation | Renderer interface; MoltenVK first, Metal only when profiling justifies it | Fastest route to a validated first frame without prematurely duplicating a full backend |
| Firmware | Host directory mounted as `/` by `--fw` | Loader expects extracted files and system modules | No firmware shipped | User-imported, versioned, local cache | Avoids distributing Sony firmware and makes legal boundary explicit |

## RPCSX boot path supported by source inspection

The current `rpcsx/main.cpp` path is:

1. Parse `--fw`, `--system`, and `--safemode`.
2. Mount the user-provided firmware directory at virtual `/`.
3. Install signal handling, construct Orbis globals, start the watchdog, create
   the GPU device, and initialize VFS/thread state.
4. If no guest path is supplied, use `/mini-syscore.elf` and force system mode.
5. Create the initial process/thread, load the executable with
   `rx::linker::loadModuleFile`, and build the execution environment.
6. Load `libSceLibcInternal.sprx` and the system/user `libkernel*.sprx` when an
   interpreter is not supplied.
7. Initialize guest devices/FDs and, for non-system mode, create IPMI/HLE
   services and daemons.
8. Enter the guest through `guestExec`.

The source proves the loader and service orchestration above. It does not prove
that this whole Linux-oriented process can be lifted unchanged to iOS. The
firmware format, decryption boundary, proprietary ABI details, and VSH service
contracts remain explicit research tasks.

## iOS blockers found

- AArch64 is the host ISA, so guest x86-64 code needs a DBT; iOS provides no
  Rosetta API to third-party apps.
- JIT pages must be allocated using the platform-approved `MAP_JIT` flow and
  the app must be signed with the required JIT entitlement. Instruction cache
  invalidation is mandatory after writing code.
- RPCSX and Kyty use Linux/POSIX mechanisms such as `prctl`, Linux signal
  contexts, direct `mmap`/`mprotect` assumptions, and Linux GPU/audio/input
  stacks. These need adapters or replacement implementations.
- A hosted GitHub Actions runner can compile an unsigned iOS/simulator bundle,
  but it cannot install on the user's iPhone. Device execution needs a signed
  IPA and a provisioning/JIT setup supplied by the user.
- MoltenVK is Apache-2.0 but brings a large dependency graph; it should remain
  optional until a synthetic renderer test requires it.
