# Branches

## `main`

The active development line for the PS3 VSH port. It integrates the pinned
RPCS3 `rpcs3_emu` core and owns the iOS frontend for the real PS3 VSH/XMB.
New emulator work should target the PS3 firmware-install → `dev_flash` → VSH
boot path first.

The branch retains the shared ARM64/JIT and firmware-inspection foundation,
but PS3 is now the active product target.

## `ps4`

The preserved PS4 line. It contains the previous PS4 firmware-root, SELF/ELF,
HLE, and guest-frame work and is the place for PS4-specific development.

## `ps5`

The paused PS5 line. It preserves the PS5 PUP inspection, extracted firmware
root preflight, and the iPhone JIT/JIT-less proof of concept. New PS5 work
should be developed there until the PS3 milestone is complete.

Firmware files, decrypted system files, keys, and copyrighted assets remain
local user-provided inputs and are not committed to either branch.
