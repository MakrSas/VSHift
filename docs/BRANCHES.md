# Branches

## `main`

The active development line for the PS4 VSH port. New emulator work should
target PS4 firmware first: firmware-root validation, SELF/ELF loading, HLE,
graphics, and the first real PS4 VSH frame.

The branch currently retains the shared ARM64/JIT and firmware-inspection
foundation while the PS4-specific loader path is being introduced.

Current verified commit: `e53654d`.

## `ps5`

The paused PS5 line. It preserves the PS5 PUP inspection, extracted firmware
root preflight, and the iPhone JIT/JIT-less proof of concept. New PS5 work
should be developed there until the PS4 milestone is complete.

Firmware files, decrypted system files, keys, and copyrighted assets remain
local user-provided inputs and are not committed to either branch.

## `ps3-experimental`

This branch preserves the attempted RPCS3/PS3Native integration for research.
It is not a verified build and must not be merged into `main` as-is. The
latest attempted commit is `557f5c0`; Actions run `31803685986` failed at the
iOS link step because `cellAdec` still references the omitted
`ppu_module_manager::cellAtracXdec` module.

Future PS3 work belongs here and must be integrated in small, independently
buildable slices. The first slice should establish an unchanged RPCS3 core
link boundary; only then should VSH boot, firmware mounting, video, audio, and
input be added.
