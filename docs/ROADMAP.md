# Roadmap

The project follows small, verifiable milestones. A later milestone does not
claim that the previous one is production-ready.

## Milestone 0 — Repository

- [x] CMake build description
- [x] portable host smoke test
- [x] iOS application target
- [x] GitHub Actions host and iOS-simulator builds
- [x] architecture, research, licensing, and status documentation
- [ ] signed device build and first iPhone run

## Milestone 1 — ARM64 JIT proof of concept

- [x] decode `mov eax, imm32`
- [x] decode `add eax, imm8`
- [x] decode `ret`
- [x] emit AArch64 `mov`, `add`, and `ret`
- [x] executable-memory abstraction with cache invalidation
- [ ] run on the user's iPhone 15 and record the result

## Milestone 2 — Mini x86-64 userspace

Add an explicit guest register file, flags, stack model, branches, calls,
memory operands, and a demand-driven instruction coverage test suite. Do not
start with all of x86-64 or AVX.

## Milestone 2a — Firmware container inspection

- [x] Parse the PS5 `SLB2` outer container header and file table
- [x] Validate table bounds and component ranges without loading the whole PUP
- [x] Add a host inspector and synthetic malformed-input tests
- [ ] Add an iOS document picker and versioned manifest storage

## Milestone 3 — PS5 ELF/SELF loader

Implement safe header parsing, PT_LOAD mapping, entry-point discovery, symbol
metadata, and a firmware-independent synthetic ELF fixture. SELF decryption and
key handling are out of scope for the repository unless the user supplies
lawfully obtained material at runtime.

## Milestone 4 — PS5 HLE foundation

Add guest threads, synchronization, clocks, file I/O, virtual paths, and a
small syscall/import registry. Host scheduling must not create one host thread
per guest thread by default.

## Milestone 5 — First graphics

Start with a synthetic guest triangle and a renderer interface. Prefer the
existing SPIR-V/MoltenVK path for the first frame; reserve a direct Metal shader
path for measured bottlenecks.

## Milestone 6 — Firmware manager

Import a user-selected official `PS5UPDATE.PUP`, validate and version it, and
extract only the components required by a declared boot profile. No firmware,
keys, decrypted Sony files, or proprietary libraries are stored in this repo or
the IPA.

## Milestones 7–11 — Safe Mode, VSH, input, audio, sustained performance

The order is Safe Mode before VSH, then touch/DualSense input, UI audio, and
profiling. A 60 FPS target is measured rather than promised; the frame budget
is 16.67 ms and thermal/sustained performance matters.
