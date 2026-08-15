# iOS build and device testing

## GitHub Actions jobs

The `CI` workflow contains independent simulator and device jobs. Both jobs
recursively check out the RPCS3 submodule and apply the existing iOS libusb
patch at build time. The device job packages an unsigned arm64 IPA and both
jobs upload checkout/Xcode diagnostics even when a build step fails.

Artifacts:

- `vshift-ios-simulator` — unsigned arm64 simulator `.app`.
- `vshift-ios-device-unsigned-ipa` — unsigned arm64 IPA for local signing.
- `vshift-ios-simulator-diagnostics` and `vshift-ios-device-diagnostics` —
  submodule revisions and Xcode version.

Actions cannot verify physical-device installation, JIT activation,
Metal/MoltenVK presentation, or PS3 VSH execution. Those checks remain
unverified until the IPA is signed, installed, and launched on an iPhone.

GitHub Actions builds an unsigned iOS simulator bundle and an unsigned
arm64 device IPA. The hosted macOS runner cannot access a developer's physical
iPhone, so signing, installation, and JIT activation happen on the user's
machine.

## iLoader + StikDebug path

1. Open the latest successful GitHub Actions run and download the artifact
   `vshift-ios-device-unsigned-ipa`.
2. Import `VSHiftJITProbe-unsigned.ipa` into iLoader and let iLoader sign and
   install it on the iPhone 15.
3. In StikDebug, assign the bundled `legacy.js` script to `VSHiftJITProbe`.
   VSHift uses the same `BRK #0x69` JIT hook as UTM; StikDebug's `universal.js`
   intentionally rejects that legacy breakpoint.
4. Activate JIT and launch `VSHiftJITProbe` from StikDebug. On iOS 26+, keep
   `Always Run Scripts` enabled if the app is not automatically recognized.
5. Tap `Run synthetic boot (JIT)` once and verify `BOOT OK` with result `42`.
   The current probe does not allocate JIT memory during app startup, so the
   single breakpoint handled by `legacy.js` is reserved for this test.

On `main`, the probe starts from a user-provided extracted PS4 root selected
through Files. The PS5 SLB2/PUP inspection controls are intentionally not part
of the active PS4 screen.

The low-level decrypted-PUP metadata importer is retained as inherited work;
the active `main` UI does not expose it because it is PS5-specific. That
inspection path remains available on the paused `ps5` branch. The active PS4
path starts from the extracted firmware root described below.

On `main`, the `Import extracted PS4 firmware root` button is the active VSH
path. It expects `system/sys/SceSysCore.elf`, the required libraries under
`system/common/lib/`, and reports `system/vsh/SceShellCore.elf`, `system_ex/`,
and `preinst/` when present. The probe checks this layout and writes a
preflight result to the manifest; it does not yet execute the guest.

The paused `ps5` branch retains the PS5 root preflight and decrypted-PUP
inspection path.

After a successful import, tap `Export manifest to Files` and choose a folder
in the iOS Files app. The exported `firmware-manifest.json` contains only
container and public-header metadata.

The probe also has two synthetic boot buttons. `Run synthetic boot (JIT)` and
`Run synthetic boot (JIT-less)` load a bundled firmware-independent ELF fixture,
map its `PT_LOAD`, and execute the guest `mov/add/ret` entry. A successful
result is `BOOT OK` with result `42`; this validates the loader/runtime path but
is not a claim that real PS5 VSH has booted.

SideStore is a valid alternative for signing/installing and JIT activation,
but it is not required for the first proof.

## Required device proof

1. Build the device IPA in GitHub Actions.
2. Sign/install it with iLoader or SideStore.
3. Enable JIT with StikDebug or the JIT mechanism supported by the setup.
4. Launch the app and record `ARM64 JIT result: 42`.
5. Attach the device log to `PROJECT_STATUS.md`.

The iOS allocator first tries `MAP_JIT`. If the signed app cannot use the
restricted `dynamic-codesigning` entitlement, it falls back to UTM's Darwin
split-W^X mapping: one writable mapping and one executable mirror created with
`vm_remap`. On iOS 26/TXM, the executable mapping is prepared through the
StikDebug breakpoint hook before the writable mapping is restored.

The repository includes the JIT entitlement in the iOS target as a declaration;
whether a signing service preserves it is an external deployment concern.

The iOS container is intentionally minimal. It is not the emulator UI and it
does not contain firmware data.
