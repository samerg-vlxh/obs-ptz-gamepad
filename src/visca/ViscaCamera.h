#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include "../control/IViscaSink.h"
#include "ViscaTransport.h"

// Byte-for-byte port of ptz_v4.py's ViscaCamera: owns one transport and one independent 16-bit
// sequence counter, starting at 1 and wrapping naturally on uint16_t overflow (matches Python's
// `(self.sequence + 1) & 0xFFFF`).
//
// Thread-safe: PtzController's delayed stop-reaffirmation (see PtzController::reaffirmStopAsync)
// calls back into the same ViscaCamera instance from a short-lived background thread while the
// controller's main event thread may concurrently be handling new input, so sendBody() must not
// race on sequence_ or interleave two packets' bytes through the transport.
class ViscaCamera : public IViscaSink {
public:
    explicit ViscaCamera(std::shared_ptr<IViscaTransport> transport);

    void zoomIn(uint8_t speed) override;
    void zoomOut(uint8_t speed) override;
    void zoomStop() override;
    void panTilt(uint8_t panSpeed, uint8_t tiltSpeed, uint8_t panDir, uint8_t tiltDir) override;
    void panTiltStop() override;

    // Raw primitive, public on purpose: future VISCA extensions (presets, focus, white balance,
    // ...) can be added as new convenience methods without touching the header-packing logic.
    void sendBody(std::vector<uint8_t> body);

private:
    std::shared_ptr<IViscaTransport> transport_;
    std::mutex sendMutex_;
    uint16_t sequence_ = 1;
};
