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
next stage adds an IR so the decoder does not need to know register allocation
or host instruction encoding.

## Planned execution model

- Decode one guest basic block at a time.
- Terminate blocks at direct/indirect control flow, calls, returns, faults, or
  unsupported instructions.
- Cache blocks by guest address plus a code-page generation counter.
- Link hot direct branches after correctness tests exist.
- Invalidate blocks when guest code pages are written.
- Keep guest flags explicit; do not rely on host NZCV state surviving arbitrary
  calls.
- Add SSE/AVX only when firmware or a test ELF demonstrates demand.

## iOS executable memory contract

`ExecutableMemory` owns allocation, cache flushing, and the transition from
writable to executable. The iOS application supplies signing/entitlement
configuration; core translation code does not include UIKit or iOS headers.

The current implementation uses a single page-aligned allocation per compiled
snippet. A production block cache will use an arena and a reclamation policy,
but only after the device proof is measured.
