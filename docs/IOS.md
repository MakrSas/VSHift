# iOS build and device testing

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

The current probe also has an `Import PS5UPDATE.PUP` button. Select the
user-provided official PUP from Files; the app reads only the SLB2 header and
file table plus the public prefix of the first non-empty PUP fragment. It shows
the first component names and saves a metadata-only manifest in Application
Support. It does not copy, decrypt, or mount firmware yet.

The `Import decrypted PUP (.dec)` button accepts a user-provided decrypted
component such as `PS5UPDATE1.PUP.dec`. It reads the fixed PUP header and
bounded segment table, scans segment prefixes for PS5 SELF, parses the embedded
ELF header and `PT_LOAD` table, and records a conservative size-correlated
SELF-to-ELF map in the manifest. It does not perform payload decryption or
claim to boot the PS5 shell yet.

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
