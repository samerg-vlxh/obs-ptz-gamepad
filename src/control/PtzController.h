#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include "IViscaSink.h"
#include "PtzTypes.h"

// VISCA speed bytes; defaults match ptz_v4.py's module-level constants. Held here (not baked into
// ViscaCamera) so Phase 7 settings persistence can override them without touching the VISCA layer.
struct PtzSpeeds {
    uint8_t zoom = 0x07;
    uint8_t pan = 0x10;
    uint8_t tilt = 0x10;
    uint8_t finePan = 0x04;
    uint8_t fineTilt = 0x04;
};

// Core control-logic state machine - a 1:1 behavioral port of ptz_v4.py's PTZApp, plus one
// deliberate fix: switching the active camera now stops the outgoing camera (see setActive()).
// Has no OBS/Cocoa dependency so it's directly unit-testable.
class PtzController {
public:
    // `cameras` must be non-empty; index 0 is initially active (mirrors ptz_v4.py's active=1 for
    // camera 1). v1 only ever routes PtzButton::A/B to indices 0/1, but the controller itself
    // works against an arbitrary-length vector so a future camera-list UI doesn't require a
    // rewrite here.
    explicit PtzController(std::vector<IViscaSink*> cameras, PtzSpeeds speeds = {},
                            float deadzone = 0.2f);

    void handleButtonDown(PtzButton button);
    void handleButtonUp(PtzButton button);
    void handleAxis(PtzAxis axis, float rawValue);

    // Unconditionally stops every camera, regardless of which is active - mirrors ptz_v4.py's
    // KeyboardInterrupt handler. Call from obs_module_unload().
    void shutdown();

    int activeCameraIndex() const;  // 0-based; UI/logging should display +1

    void setOnEvent(std::function<void(const std::string&)> callback);

    // Settings-dialog live updates (Phase 7). Camera IP/port changes go through
    // UdpViscaTransport::setEndpoint() instead - cameras_ holds raw IViscaSink* pointers that
    // never need to change identity just because an endpoint moved.
    void updateSpeeds(const PtzSpeeds& speeds);
    void setDeadzone(float deadzone);

private:
    void setActive(size_t index);
    void recomputePanTilt();
    IViscaSink* activeCam();
    PtzSpeeds speedsSnapshot() const;
    float deadzoneSnapshot() const;

    static uint8_t dirFromPan(float value);
    static uint8_t dirFromTilt(float value);

    // Named emitEvent, not emit: Qt's <qobjectdefs.h> #defines `emit` to nothing once Qt headers
    // are included anywhere in a translation unit (e.g. via plugin-main.cpp pulling in QString),
    // which silently mangles a method actually named `emit`.
    void emitEvent(const std::string& message);

    std::vector<IViscaSink*> cameras_;
    PtzSpeeds speeds_;
    float deadzone_;

    mutable std::mutex mutex_;
    size_t activeIndex_ = 0;
    // LeftX, LeftY, RightX, RightY - each slot persists at its last reported value until *that*
    // axis fires again, matching ptz_v4.py's per-axis (not per-event) deadzone/state handling.
    std::array<float, 4> lastAxis_{0.0f, 0.0f, 0.0f, 0.0f};

    std::function<void(const std::string&)> onEvent_;
};
