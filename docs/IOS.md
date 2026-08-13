# iOS build and device testing

GitHub Actions builds the project and an unsigned iOS simulator bundle. The
hosted macOS runner cannot access a developer's physical iPhone, so the first
device test is a separate signed-artifact step.

## Required device proof

1. Build/sign the IPA with a valid Apple development certificate and profile.
2. Install it on the iPhone 15.
3. Enable the JIT mechanism supported by the user's sideloading setup.
4. Launch the app and record `ARM64 JIT result: 42`.
5. Attach the device log to `PROJECT_STATUS.md`.

The repository includes the JIT entitlement in the iOS target as a declaration;
whether a signing service preserves it is an external deployment concern.

The iOS container is intentionally minimal. It is not the emulator UI and it
does not contain firmware data.
