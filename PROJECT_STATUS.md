# Project status

Date: 2026-08-13

## Current milestone

Milestone 0 complete; Milestone 1 PoC is CI-verified and awaiting physical
iPhone verification.

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
- GitHub Actions host tests and iOS simulator compilation passed on the pushed
  branch.

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

1. Produce a signed device artifact using the user's Apple signing setup.
2. Run the PoC on iPhone 15 and record result/logs.
3. Add IR and a guest register/flags model only after the device proof passes.
4. Add a guest register and flags model with focused unit tests.
5. Add a demand-driven basic-block cache.

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
