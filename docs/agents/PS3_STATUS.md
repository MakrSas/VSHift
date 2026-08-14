# PS3 agent status

## Current status

Working on the real PS3 VSH boot path on branch `ps3`.

## Implemented

- Incremental PS3 PUP 4.93 parsing and decryption of the regular `dev_flash`
  packages used by the firmware.
- Real `vsh.self` SELF parsing/mapping into persistent guest memory.
- Persistent PPU/LV2 execution state instead of a one-shot synthetic boot.
- Incremental PRX image loading and module-start handling for the first VSH
  module path reached by the current boot slice.
- Public `PS3Core` adapter, common lifecycle/capability contract, platform
  input/power types, and a PS3 core manifest.
- Full-screen iOS display controller exists; actual RSX framebuffer delivery
  is still pending.

## Exposed Core APIs

`PS3Core` exposes initialization, firmware validation/install, boot, pause,
resume, reset, shutdown, media placeholders, normalized controller/power
state, status, capabilities, and event callbacks through
`core-api/console_core.h`.

## Known blockers

- Continue implementing the PPU instruction and LV2 coverage exposed by the
  real VSH execution trace.
- PRX relocation/import/export linking is incomplete.
- RSX command processing, framebuffer, audio, and guest controller routing are
  not implemented yet, so this is not an XMB milestone.
- The iOS target requires an Apple toolchain; the Windows host verifies the
  shared C++ targets and tests only.

## Needs from Frontend

Consume `PS3Core` lifecycle/status rather than including `core/*` internals.
The frontend should keep fullscreen presentation, touch layout, file picking,
and host surface ownership.

## Needs from Core API

The next shared additions should be a versioned video-frame sink, audio-frame
sink, and normalized haptics event. They should remain host-neutral.

## Last verified commit

Architecture work is in progress after checkpoint `5759a8b`.

## CI

Before this architecture slice, the Windows build and CTest suite passed
23/23 tests. Re-run both after the new public target is integrated.

## Device testing notes

No IPA was produced in the Windows environment. Firmware remains user-supplied
and is not committed to the repository.
