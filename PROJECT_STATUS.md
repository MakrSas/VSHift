# Project status

Date: 2026-08-13

## Current milestone

Milestone 0 in progress; Milestone 1 PoC implemented and awaiting build plus
physical iPhone verification.

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

## Not working yet

- No PS5 ELF/SELF loader.
- No firmware importer or PUP parser.
- No guest memory subsystem, IR, block cache, HLE, GPU, audio, or input.
- No signed IPA or device run has been recorded.

## Current hypothesis

A small native ARM64 backend plus explicit guest-memory and HLE interfaces is a
more realistic first step than porting a desktop emulator. The iPhone test must
validate the executable-memory/signing path before larger CPU work starts.

## Next 5 actions

1. Run host unit test and inspect emitted bytes.
2. Verify the iOS simulator build in GitHub Actions.
3. Produce a signed device artifact using the user's Apple signing setup.
4. Run the PoC on iPhone 15 and record result/logs.
5. Add IR and a guest register/flags model only after the device proof passes.

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
- Actual PS5 VSH boot requires user-provided firmware and substantial missing
  PS5 ABI/HLE/GPU work; no claim of VSH support is made yet.
