# obs-ptz-gamepad

Native OBS Studio plugin (macOS, Apple Silicon) that drives two VISCA-over-IP PTZ cameras from an
Xbox controller. Loads automatically when OBS starts — no separate script or app to launch.

Ported from an earlier standalone Python/pygame script (`ptz_v4.py`), which this plugin's
control logic (`src/control/PtzController.*`) replicates behaviorally, plus two deliberate
improvements:

- Switching the active camera now stops the outgoing camera (it used to keep moving unnoticed).
- Controller input goes through Apple's `GameController.framework` semantic button/stick
  properties instead of raw SDL indices, removing an LB/RB mapping ambiguity that existed in the
  Python version.

## Controls

- **A / B** — select camera 1 / camera 2
- **Left shoulder / right shoulder** — zoom out / zoom in (held)
- **Left stick** — pan/tilt at normal speed
- **Right stick** — pan/tilt at a slower ("fine") speed; ignored while the left stick is deflected

IPs, VISCA port, and all speed/deadzone values are editable from the plugin's dock
("Settings...") — see `src/settings/PtzSettings.*`. Defaults match the original Python script
(`192.168.20.41` / `192.168.20.42`, port `52381`).

## Building

Requires: full Xcode.app (not just the Command Line Tools — the macOS build uses CMake's `Xcode`
generator), `cmake`, `ninja` (`brew install cmake ninja`).

```
cmake --preset macos-arm64
cmake --build --preset macos-arm64
```

The first configure downloads OBS sources, prebuilt obs-deps, and prebuilt Qt6 (see
`buildspec.json`) and builds libobs from source — this takes a few minutes and needs network
access. Subsequent configures/builds are incremental.

Output bundle: `build_macos_arm64/RelWithDebInfo/obs-ptz-gamepad.plugin`.

### AGL.framework build workaround (macOS SDK Xcode 26+)

The prebuilt Qt6 dependency unconditionally links `-framework AGL` on Apple platforms (see its
`FindWrapOpenGL.cmake`). Recent macOS SDKs no longer ship a linkable AGL binary (Apple left an
empty compatibility stub with a broken symlink). `CMakeLists.txt` works around this by building a
throwaway empty stub framework at configure time and combining it with `-dead_strip_dylibs`, so
the final binary carries zero runtime dependency on AGL. If Apple ever restores a real AGL binary
to the SDK, this workaround becomes a no-op (the `NOT WrapOpenGL_AGL` guard skips it once a real
one is found).

## Installing

```
codesign --force --deep --sign - build_macos_arm64/RelWithDebInfo/obs-ptz-gamepad.plugin
cp -R build_macos_arm64/RelWithDebInfo/obs-ptz-gamepad.plugin \
      ~/Library/Application\ Support/obs-studio/plugins/
```

Ad-hoc signing is sufficient for local use; OBS loads third-party ad-hoc-signed plugins from the
user plugins directory without further Gatekeeper steps. Restart OBS to pick up a rebuilt plugin.

## Testing the control logic without OBS or a camera

`tests/` is a standalone CMake project with no libobs/Qt/Cocoa dependency, covering the VISCA byte
layer and the `PtzController` state machine (deadzone persistence, stick priority, direction-byte
mapping, camera-switch behavior, shutdown):

```
cmake -S tests -B tests/build -G Ninja
cmake --build tests/build
./tests/build/test_visca_packets
./tests/build/test_ptz_controller
```

## Architecture

- `src/visca/` — VISCA-over-IP UDP protocol (`ViscaCamera`, `UdpViscaTransport`), no OBS
  dependency.
- `src/control/` — `PtzController`, the ported state machine; no OBS/Cocoa dependency, directly
  unit-testable.
- `src/input/` — `GameControllerBridge` (Objective-C++), bridges `GameController.framework` to the
  plain-C++ `IControllerEventSink` interface.
- `src/ui/` — Qt dock widget and settings dialog.
- `src/settings/` — JSON persistence via `obs_module_config_path` + `obs_data_t`.
- `src/plugin-main.cpp` — wires everything together in `obs_module_load`/`obs_module_unload`.
