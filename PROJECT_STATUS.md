# Project status

Date: 2026-08-14

## Current milestone

The active target is now PS3. The previous PS4 work was preserved in the
`ps4` branch and the PS5 work remains in `ps5`.

Milestones 0 and 1 are complete: the JIT PoC returned `42` on the user's
iPhone 15. The current milestone is integrating RPCS3's real `rpcs3_emu`
target and its firmware installer behind a VSHift-owned mobile frontend.

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
- GitHub Actions is the source of truth; the latest run reached RPCS3
  dependency configuration and exposed platform-specific desktop USB/HID
  assumptions that are being removed from the headless integration profile.
- A GitHub Actions job now packages an unsigned arm64 device IPA for the
  iLoader + StikDebug test path.
- The upstream RPCS3 repository is pinned as `third_party/rpcs3` and its
  `rpcs3_emu` target is connected through `core/ps3`.
- The PS3 firmware installer uses RPCS3's PUP validation, package SELF
  decryption, and TAR extraction to create a user-owned `dev_flash`.
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

## Not working yet

- The RPCS3 headless build has not yet passed VSHift's GitHub Actions build.
- The firmware installer and `Emulator::BootGame("/dev_flash/vsh/module/vsh.self")`
  adapter are present, but the first successful cross-platform build is still
  required before calling them a working boot milestone.
- RSX presentation still needs a Metal/MoltenVK-backed callback that exposes
  the actual XMB frame; no synthetic XMB is allowed.
- Input, audio, haptics, persistence, and RSX presentation still need their
  platform-neutral bridges. Media import, ISO mounting, and the UTM-style
  control drawer have their first frontend/API layer but are not yet proven
  against a running guest.
- No Sony firmware or other copyrighted assets are committed or bundled.

## Current hypothesis

The shortest credible route to a real PS3 XMB is to reuse RPCS3's existing Cell,
LV2, firmware, VFS, and RSX implementations and replace only the desktop UI and
host adapters. An iPhone build is still a porting task: upstream officially
targets desktop platforms, so Actions must expose each missing mobile dependency
instead of hiding it behind a mock screen.

## Next 5 actions

1. Make the pinned `rpcs3_emu` build pass on the host and iOS toolchains.
2. Expose the RPCS3 firmware installer as an iOS document-picker operation.
3. Start `vsh.self` through the upstream `Emulator` lifecycle in headless mode.
4. Connect the real RSX frame path to Metal and verify a captured guest frame.
5. Add shared DualShock 3 input, audio, haptics, persistence, media import,
   ISO mounting, and the UTM-style control drawer.

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
- A successful source build is not the same as XMB boot. The real milestone is
  a guest frame emitted by RPCS3's RSX/VSH path on the iPhone.
- RPCS3's upstream CMake is desktop-oriented; the headless iOS adapter may
  need targeted portability patches while retaining upstream core code.
