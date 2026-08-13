# CPU dynamic binary translation

## Current proof

The first proof is intentionally narrow:

```text
bytes: B8 28 00 00 00 83 C0 02 C3
  -> x86 decoder
  -> instruction records
  -> AArch64 words: MOV W0,#40; ADD W0,W0,#2; RET
  -> executable memory
  -> result 42 on ARM64
```

The decoder, backend, and executable-memory policy are separate files. The
decoder now lowers into a shared IR. The ARM64 backend and the JIT-less
interpreter consume that same IR, so the interpreter can serve as the reference
semantics for the current instruction subset.

## Planned execution model

- Decode one guest basic block at a time.
- Lower blocks to a shared IR that can run either in the ARM64 JIT or in a
  JIT-less interpreter.
- Terminate blocks at direct/indirect control flow, calls, returns, faults, or
  unsupported instructions.
- Cache blocks by guest address plus a code-page generation counter.
- Link hot direct branches after correctness tests exist.
- Invalidate blocks when guest code pages are written.
- Keep guest flags explicit; do not rely on host NZCV state surviving arbitrary
  calls.
- Add SSE/AVX only when firmware or a test ELF demonstrates demand.

## JIT-less experimental mode

The interpreter is a deliberate second execution backend. It avoids executable
memory and is intended for deterministic instruction/loader/firmware tests,
debugging unsupported operations, and environments where StikDebug or another
JIT enabler is unavailable. Both backends must consume the same guest state and
IR semantics so the interpreter can serve as a reference implementation.

## iOS executable memory contract

`ExecutableMemory` owns allocation, cache flushing, and the transition from
writable to executable. The iOS application supplies signing/entitlement
configuration; core translation code does not include UIKit or iOS headers.

The current implementation uses a page-aligned allocation per compiled
snippet. On Apple platforms it can use a split-W^X pair: a writable mapping
for code generation and an executable mirror for dispatch. A production block
cache will use an arena and a reclamation policy, but only after the device
proof is measured.
