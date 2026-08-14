# PS3 experimental handoff

Date: 2026-08-14

## Branch state

- Stable development line: `main` at `e53654d`.
- PS3 experiment: `ps3-experimental` at `557f5c0`.
- PS5 line: `ps5`.
- The PS3 branch is a preserved experiment, not a working PS3 emulator.

## Verified baseline

The rollback target `e53654d` passed GitHub Actions run
`31805042046`. That run produced:

`vshift-ios-device-unsigned-ipa`

The canonical validation path remains GitHub Actions. Local Windows CMake and
Xcode builds were intentionally not used.

## What was attempted

The PS3 branch embedded an RPCS3 core and added an iOS headless profile. The
work included iOS lifecycle/input bridges, media stubs, RSX utility stubs,
firmware/boot boundary checks, and cache changes. The intent was to reach a
real RPCS3 VSH boot path, not draw a fake XMB.

## Why it was rolled back

The profile excluded desktop media sources and then needed replacement module
definitions. The final device link failed with:

```text
Undefined symbols for architecture arm64:
  "ppu_module_manager::cellAtracXdec",
  referenced from: get_core_ops(int) in cellAdec.o
```

Earlier iterations also exposed missing `cellDmuxPamf`, `cellVdec`, and
`avconf` boundaries, followed by Apple-libc++ incompatibilities inside
`cellDmuxPamf.cpp`. These errors show that removing RPCS3 modules from the
iOS target without preserving the complete static module dependency graph is
not a safe integration strategy.

## Required next strategy

1. Keep `main` untouched and work only on `ps3-experimental`.
2. Start from the smallest RPCS3 target that links without broad source
   exclusions or speculative stubs.
3. Add exactly one platform-independent boundary at a time.
4. Run the device Actions build after every boundary.
5. Only after a green link, proceed to firmware installation, `dev_flash`,
   VSH entry, and finally RSX/Metal frame output.

Do not claim that a green IPA boots the real PS3 XMB. A successful link only
proves that the selected native core can be packaged. Real XMB output still
requires a working PS3 guest boot, RSX renderer, audio path, and input path.

Firmware, keys, decrypted system files, and copyrighted assets remain
user-provided runtime inputs and are not committed to this repository.
