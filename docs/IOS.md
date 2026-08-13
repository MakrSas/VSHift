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
5. Verify `ARM64 JIT result: 42`.

The current probe also has an `Import PS5UPDATE.PUP` button. Select the
user-provided official PUP from Files; the app reads only the SLB2 header and
file table and shows the first component names. It does not copy or decrypt
firmware yet.

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
