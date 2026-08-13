# VSHift

VSHift is an experimental research project for a modular PS5 System Software /
VSH execution environment on ARM64 iPhone hardware.

The repository does not contain Sony firmware, keys, decrypted system files, or
copyrighted assets. Firmware is planned as a user-provided runtime input.

## Current status

The iPhone 15 proof of concept returns `42` in both JIT and JIT-less modes.
The iOS probe also inspects a user-provided decrypted PS5 PUP, discovers its
SELF/ELF header map, and exports a metadata manifest. Real SELF payload
decryption, HLE, GPU, and VSH execution are still future milestones.

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
