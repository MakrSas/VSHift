# VSH boot plan

There is no VSH boot support in the current build. This document is the
implementation contract for the later milestones.

## Proven source behavior

RPCSX's current `rpcsx/main.cpp` shows a desktop boot orchestration in which a
user-provided extracted firmware directory is mounted at virtual `/`, system
mode selects `/mini-syscore.elf` when no explicit guest path is provided, and
the executable is passed through module loading, process setup, HLE service
initialization, and `guestExec`.

That is a useful sequence to model, not a portable implementation. The source
also depends on Linux process/signal/context behavior and desktop GPU/audio
services, so VSHift will validate each boundary independently.

## Required VSHift boot stages

1. Firmware manager exposes a read-only virtual filesystem.
2. SELF/ELF loader validates headers and maps declared segments into guest
   memory.
3. Runtime linker resolves only declared imports through a versioned HLE table.
4. Kernel/HLE creates the initial process, thread, TLS, clocks, and sync objects.
5. CPU DBT executes a synthetic ELF before any real system module is attempted.
6. Safe Mode is the first firmware-backed boot profile.
7. VSH is attempted only after Safe Mode has a repeatable frame and crash report.

## Success evidence

Each stage must leave an artifact or log: firmware manifest, mapped-segment
report, import table, guest-thread trace, JIT block trace, and rendered frame.
No claim of “real VSH” is made from a UI mock or a successful host-side unit
test.

The current foundation implements the metadata side of stage 1: an SLB2
component catalog resolves bounded file ranges without owning firmware bytes.
The iOS probe also validates the decrypted PUP segment table, finds the PS5 SELF
candidate, parses its embedded ELF header and `PT_LOAD` table, and exports a
conservative size-based payload map. Sparse guest-memory mapping currently
remains limited to the synthetic ELF fixture; real SELF payload decryption and
execution remain future work.
