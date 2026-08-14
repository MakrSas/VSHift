# Project status

Date: 2026-08-14

## Current milestone

Milestones 0 and 1 are complete: the JIT PoC returned `42` on the user's
iPhone 15. Milestone 2a now has on-device SLB2 inspection, decrypted-PUP
segment discovery, and metadata-only SELF/ELF mapping.

## Working

- Clean repository baseline with CMake build description.
- Three-instruction x86-64 decoder for the synthetic proof program.
- Explicit AArch64 emission for `mov`, `add`, and `ret`.
- Page-aligned executable-memory abstraction with instruction-cache flush.
- iOS application target that displays the JIT result.
- Documentation of upstream findings and legal/firmware boundaries.

## Latest verification

- Static source review completed with no trailing-whitespace errors.
- Portable CMake 4.4.2 was installed outside the repository and its archive was
  verified against the official SHA-256 file.
- Local CMake configuration is intentionally deferred; GitHub Actions is the
  source of truth for the first host and iOS builds.
- GitHub Actions host tests and iOS simulator compilation passed on the pushed
  branch.
- A GitHub Actions job now packages an unsigned arm64 device IPA for the
  iLoader + StikDebug test path.
- The Apple executable-memory backend now mirrors UTM's split-W^X fallback for
  iLoader installs and uses UTM's `BRK #0x69` StikDebug hook on iOS 26/TXM.
- Added the first real-firmware boundary: a read-only PS5 `SLB2` table parser,
  CLI inspector, and malformed-input tests. No firmware is bundled.
- The iOS probe now opens a user-selected PUP from Files and displays its first
  SLB2 component names without copying or decrypting firmware.
- The iOS probe reads the public prefix of the first non-empty nested PUP and
  saves a metadata-only manifest in Application Support.
- Added a metadata-only firmware catalog that resolves bounded component
  ranges without loading firmware bytes.
- Added a bounded ELF64/`PT_LOAD` header parser and synthetic malformed-input
  tests; real-firmware segment sourcing is not implemented yet.
- Added sparse guest memory and synthetic `PT_LOAD` mapping with zero-filled
  BSS behavior and permission-aware guest reads/writes.
- Added a shared IR plus JIT-less interpreter for the current `mov/add/ret`
  subset; a persistent runtime mode setting is intentionally still pending.
- Added a differential host test that checks the interpreter result against the
  ARM64 JIT when execution is available.
- Added a synthetic ELF boot session and iOS buttons for JIT/JIT-less boot
  reports; this is a firmware-independent pipeline, not real VSH.
- Added bounded PS5 SELF header parsing with embedded ELF discovery and
  conservative `PT_LOAD` ↔ SELF-entry size correlation.
- The iOS decrypted-PUP importer now scans all PUP segments, identifies the
  PS5 SELF candidate, records ELF architecture/entry/`PT_LOAD` data, and
  exports the complete segment map in the manifest.
- Added a firmware-root picker and PS5 Safe Mode preflight. It accepts both
  RPCSX's `mini-syscore.elf` layout and the real PS5 layout with
  `system/sys/SceSysCore.elf`, plus the system libraries and VSH entry.
- Added a local helper that expands the already decrypted PUP entries and
  preserves the user-provided exFAT system images for a separate filesystem
  extraction step.
- After the PS3 RPCS3 experiment was found to be too broad for the current
  iOS profile, `main` was rolled back to `e53654d` and verified green by
  GitHub Actions run `31805042046`. The run produced the unsigned device IPA
  artifact `vshift-ios-device-unsigned-ipa`.
- The attempted PS3 integration is preserved on `ps3-experimental` at
  `557f5c0`; it is intentionally not presented as a working PS3 emulator.

## Not working yet

- No PS5 SELF payload decryption or executable real-firmware segment source.
- No guest execution from the extracted PS5 root yet.
- No file-backed guest VFS or filesystem-image mounting inside the iOS app.
- No block cache, HLE, GPU, audio, or input.
- The local 12.02 test root has been extracted from user-provided firmware;
  no firmware bytes are included in the IPA or repository.

## Current hypothesis

A small native ARM64 backend plus explicit guest-memory and HLE interfaces is a
more realistic first step than porting a desktop emulator. The iPhone test must
validate the executable-memory/signing path before larger CPU work starts.

## Next 5 actions

1. Download the new device IPA and update the existing installation with
   iLoader.
2. Import the extracted firmware root on iOS and run Safe Mode preflight.
3. Implement a file-backed guest VFS and bounded SELF payload source.
4. Add more IR operations and a guest register/flags model while keeping the firmware parser
   independent from CPU execution.
5. Add a JIT-less IR interpreter for firmware and CPU experiments without
   executable memory.

## Important commands

```text
cmake -S . -B build -DVSHIFT_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build --output-on-failure -C Release
```

## Known blockers

- GitHub-hosted runners cannot install or exercise code on a physical iPhone.
- Local Windows compilation is out of scope for this pass; GitHub Actions
  provides the required host and Apple toolchains.
- iOS JIT execution depends on a valid signed entitlement and the user's
  sideload/JIT activation path.
- Actual PS5 VSH boot still requires file-backed guest VFS, SELF payload
  decryption/mapping, PS5 ABI/HLE, runtime linking, GPU, and input/audio work;
  no claim of VSH support is made yet.

## 2026-08-14 handoff state

The stable line is `main` at `e53654d`. The PS3 RPCS3 experiment was moved to
`ps3-experimental` after its iOS device build failed at the final link step.
The immediate failure was an undefined
`ppu_module_manager::cellAtracXdec` referenced by `cellAdec` after the
headless profile excluded `cellAtracXdec.cpp`. This is evidence that the
RPCS3 module graph cannot be reduced by removing media sources one at a time
without preserving all dependent module boundaries.

The next PS3 attempt must therefore be incremental: first select a minimal
RPCS3 source/profile that links unchanged, then add one independent boot
component at a time, with a separate Actions build after each component.
Do not add another large collection of iOS stubs or omit module-table entries
without tracing every static reference to them.
