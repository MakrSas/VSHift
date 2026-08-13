# VSHift

VSHift is an experimental research project for a modular PS5 System Software /
VSH execution environment on ARM64 iPhone hardware.

The repository does not contain Sony firmware, keys, decrypted system files, or
copyrighted assets. Firmware is planned as a user-provided runtime input.

## Current status

Milestone 1 proof of concept: decode a tiny x86-64 program, emit ARM64 code,
and execute it on an ARM64 host. The expected result is `42`.

See [PROJECT_STATUS.md](PROJECT_STATUS.md), [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md),
and [docs/UPSTREAM_RESEARCH.md](docs/UPSTREAM_RESEARCH.md).

## Build

The canonical builds run in GitHub Actions. Locally, with CMake 3.24 or newer:

```sh
cmake -S . -B build -DVSHIFT_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build --output-on-failure -C Release
```

GitHub Actions also compiles an unsigned ARM64 iOS simulator bundle. A real
iPhone 15 run requires a signed IPA and a sideload/JIT setup that preserves the
JIT entitlement.
