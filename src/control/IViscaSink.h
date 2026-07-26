#pragma once

#include <cstdint>

// What PtzController needs from "a camera" - implemented by ViscaCamera in production, and by a
// FakeSink in tests so the control logic can be verified without a real socket.
class IViscaSink {
public:
    virtual ~IViscaSink() = default;

    virtual void zoomIn(uint8_t speed) = 0;
    virtual void zoomOut(uint8_t speed) = 0;
    virtual void zoomStop() = 0;
    virtual void panTilt(uint8_t panSpeed, uint8_t tiltSpeed, uint8_t panDir, uint8_t tiltDir) = 0;
    virtual void panTiltStop() = 0;
};
