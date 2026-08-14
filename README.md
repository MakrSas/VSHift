# VSHift

VSHift is an experimental research project for a modular PS3 System Software /
VSH execution environment on ARM64 iPhone hardware. The active PS3 line is
`main`; the preserved PS4 line is `ps4`, and the paused PS5 implementation is
preserved in `ps5`.

The repository does not contain Sony firmware, keys, decrypted system files, or
copyrighted assets. Firmware is planned as a user-provided runtime input.

## Current status

The shared iPhone 15 proof of concept returns `42` in both JIT and JIT-less
modes. The active milestone on `main` is the real RPCS3 PS3 firmware install
and VSH/XMB path. The earlier PS4 and PS5 work remains on its own branch.

See [docs/BRANCHES.md](docs/BRANCHES.md) for the branch policy.

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
