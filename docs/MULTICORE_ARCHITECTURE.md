# Multicore architecture

```text
VSHift App
    -> VSHift Core API
        -> PS3Core / PSPCore / PS4Core / PS5Core
            -> console-specific implementation
```

## Public contract

`core-api/console_core.h` defines the common lifecycle:

`Created -> Initializing -> Ready -> InstallingFirmware -> Booting -> Running`

with `Paused`, `Stopping`, `Stopped`, and `Error` transitions as appropriate.
It also defines firmware validation/installation, media, input, power, status,
capabilities, and event delivery. A core reports only capabilities it really
implements; the frontend does not branch on console names to discover them.

`platform/host_services.h` contains host-normalized controller and power state.
Touch overlays and physical controller drivers stay in the frontend/platform
layer. A core receives normalized controls and does not own touch UI.

## Targets

- `vshift_core_api`: header-only public contract.
- `vshift_platform`: header-only shared host types.
- `vshift_ps3_core`: PS3 public adapter.
- `vshift_core`: current incremental PS3 implementation layer.

The last target is intentionally retained while the PPU/LV2/firmware/RSX
implementation is moved behind `PS3Core` in small, testable slices. Future
`PSPCore`, `PS4Core`, and `PS5Core` targets can link the same API without
changing the app.

## Manifest

Each core owns a `core_manifest.json` with `schemaVersion`, identity,
firmware, media, input layout, capabilities, storage, and guest system UI.
The JSON manifest and the C++ descriptor use the same vocabulary. The manifest
is descriptive and does not grant capabilities that the implementation has not
advertised.

## Output direction

The intended output path is:

```text
guest RSX/audio/input handling -> core event/output API -> platform surface
```

Video, audio, haptics, storage, and disc APIs remain explicit extensions of
the shared contract. They must not leak PS3-specific classes into the app.
