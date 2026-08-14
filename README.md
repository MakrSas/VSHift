# VSHift

VSHift is an experimental research project for modular PlayStation System
Software / VSH execution environments on ARM64 iPhone hardware. The active
verified line is the PS4-oriented `main`; the paused PS5 implementation is
preserved in `ps5`, and the unverified PS3/RPCS3 experiment is preserved in
`ps3-experimental`.

The repository does not contain Sony firmware, keys, decrypted system files, or
copyrighted assets. Firmware is planned as a user-provided runtime input.

## Current status

The shared iPhone 15 proof of concept returns `42` in both JIT and JIT-less
modes. The next active milestone on `main` is the PS4 firmware-root and VSH
path. The PS5 PUP/SELF inspection work remains available on `ps5`.

See [docs/BRANCHES.md](docs/BRANCHES.md) for the branch policy.

See [PROJECT_STATUS.md](PROJECT_STATUS.md), [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md),
and [docs/UPSTREAM_RESEARCH.md](docs/UPSTREAM_RESEARCH.md).

For the current PS3 experiment, read
[docs/PS3_EXPERIMENTAL.md](docs/PS3_EXPERIMENTAL.md) before changing the
RPCS3 integration.

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
