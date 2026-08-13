# Branches

## `main`

The active development line for the PS4 VSH port. New emulator work should
target PS4 firmware first: firmware-root validation, SELF/ELF loading, HLE,
graphics, and the first real PS4 VSH frame.

The branch currently retains the shared ARM64/JIT and firmware-inspection
foundation while the PS4-specific loader path is being introduced.

## `ps5`

The paused PS5 line. It preserves the PS5 PUP inspection, extracted firmware
root preflight, and the iPhone JIT/JIT-less proof of concept. New PS5 work
should be developed there until the PS4 milestone is complete.

Firmware files, decrypted system files, keys, and copyrighted assets remain
local user-provided inputs and are not committed to either branch.
