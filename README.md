# obs-ptz-gamepad

A native OBS Studio plugin (macOS, Apple Silicon) that lets you drive two VISCA-over-IP PTZ
cameras directly from an Xbox controller — right from inside OBS. Install it once, and it's active
every time OBS starts; there's no separate app or script to launch alongside your stream.

## What it's for

If you run a small studio or streaming setup with one or two network PTZ cameras (VISCA-over-IP,
port 52381) and want hands-on camera control without reaching for a mouse, a dedicated hardware
controller, or a paid PTZ control surface, this plugin turns a standard Xbox controller into that
control surface. It's aimed at solo streamers and small production setups switching between a
couple of camera angles live.

## Controls

- **A / B** — select camera 1 / camera 2
- **Left shoulder / right shoulder** — zoom out / zoom in (held)
- **Left stick** — pan/tilt at normal speed
- **Right stick** — pan/tilt at a slower, finer speed (ignored while the left stick is deflected)

Switching cameras automatically stops whatever the previously active camera was doing, so nothing
keeps panning or zooming in the background once you've moved on.

## Dock UI

A dock panel (visible under OBS's Docks menu) shows:

- Whether a controller is connected, and its name
- Which camera is currently active
- A live log of connect/disconnect and camera-switch events
- A **Settings** button to edit both cameras' IPs, the VISCA port, and all pan/tilt/zoom speeds —
  no rebuild required, changes take effect immediately and persist across restarts

## Requirements

- macOS on Apple Silicon
- OBS Studio
- An Xbox (or other `GameController.framework`-compatible extended gamepad) controller
- Two PTZ cameras reachable over the network, speaking VISCA-over-IP on port 52381

## Installing

Ad-hoc-signed prebuilt bundle goes to OBS's user plugin directory:

```
cp -R obs-ptz-gamepad.plugin ~/Library/Application\ Support/obs-studio/plugins/
```

Restart OBS — the plugin loads automatically and the dock appears.

## Building from source

Requires full Xcode.app (the macOS build uses CMake's `Xcode` generator — Command Line Tools alone
aren't enough), plus `cmake` and `ninja` (`brew install cmake ninja`).

```
cmake --preset macos-arm64
cmake --build --preset macos-arm64
```

The first configure downloads OBS sources, prebuilt obs-deps, and prebuilt Qt6 (see
`buildspec.json`) and builds libobs from source — this takes a few minutes and needs network
access. Subsequent configures/builds are incremental.

Output bundle: `build_macos_arm64/RelWithDebInfo/obs-ptz-gamepad.plugin`. Sign and install it with:

```
codesign --force --deep --sign - build_macos_arm64/RelWithDebInfo/obs-ptz-gamepad.plugin
cp -R build_macos_arm64/RelWithDebInfo/obs-ptz-gamepad.plugin \
      ~/Library/Application\ Support/obs-studio/plugins/
```

Ad-hoc signing is sufficient for local use; OBS loads third-party ad-hoc-signed plugins from the
user plugins directory without further Gatekeeper steps.

### AGL.framework build workaround (macOS SDK Xcode 26+)

The prebuilt Qt6 dependency unconditionally links `-framework AGL` on Apple platforms (see its
`FindWrapOpenGL.cmake`). Recent macOS SDKs no longer ship a linkable AGL binary (Apple left an
empty compatibility stub with a broken symlink). `CMakeLists.txt` works around this by building a
throwaway empty stub framework at configure time and combining it with `-dead_strip_dylibs`, so the
final binary carries zero runtime dependency on AGL. If Apple ever restores a real AGL binary to
the SDK, this workaround becomes a no-op (the `NOT WrapOpenGL_AGL` guard skips it once a real one
is found).

## Testing the control logic without OBS or a camera

`tests/` is a standalone CMake project with no libobs/Qt/Cocoa dependency, covering the VISCA byte
layer and the control state machine (deadzone persistence, stick priority, direction-byte mapping,
camera-switch behavior, shutdown):

```
cmake -S tests -B tests/build -G Ninja
cmake --build tests/build
./tests/build/test_visca_packets
./tests/build/test_ptz_controller
```

## Architecture

- `src/visca/` — VISCA-over-IP UDP protocol (`ViscaCamera`, `UdpViscaTransport`), no OBS
  dependency.
- `src/control/` — `PtzController`, the control state machine; no OBS/Cocoa dependency, directly
  unit-testable.
- `src/input/` — `GameControllerBridge` (Objective-C++), bridges `GameController.framework` to a
  plain-C++ `IControllerEventSink` interface.
- `src/ui/` — Qt dock widget and settings dialog.
- `src/settings/` — JSON persistence via `obs_module_config_path` + `obs_data_t`.
- `src/plugin-main.cpp` — wires everything together in `obs_module_load`/`obs_module_unload`.
