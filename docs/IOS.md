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
3. Use StikDebug to activate JIT for the installed app, following the setup
   required by the current iOS version and signing method.
4. Launch `VSHiftJITProbe` and verify `ARM64 JIT result: 42`.

SideStore is a valid alternative for signing/installing and JIT activation,
but it is not required for the first proof.

## Required device proof

1. Build the device IPA in GitHub Actions.
2. Sign/install it with iLoader or SideStore.
3. Enable JIT with StikDebug or the JIT mechanism supported by the setup.
4. Launch the app and record `ARM64 JIT result: 42`.
5. Attach the device log to `PROJECT_STATUS.md`.

The repository includes the JIT entitlement in the iOS target as a declaration;
whether a signing service preserves it is an external deployment concern.

The iOS container is intentionally minimal. It is not the emulator UI and it
does not contain firmware data.
