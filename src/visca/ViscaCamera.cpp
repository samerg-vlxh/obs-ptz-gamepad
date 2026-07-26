#include "ViscaCamera.h"

#include "ViscaConstants.h"

ViscaCamera::ViscaCamera(std::shared_ptr<IViscaTransport> transport) : transport_(std::move(transport)) {}

void ViscaCamera::sendBody(std::vector<uint8_t> body) {
    // 8-byte VISCA-over-IP header as four big-endian uint16_t words, matching ptz_v4.py's
    // struct.pack(">HHHH", 0x0100, 8+len(body), seq, 0) exactly - hand-rolled rather than using
    // htons() tricks, since arm64 is little-endian and byte order must not depend on host order.
    std::vector<uint8_t> packet;
    packet.reserve(8 + body.size());

    auto pushU16 = [&packet](uint16_t value) {
        packet.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        packet.push_back(static_cast<uint8_t>(value & 0xFF));
    };

    pushU16(0x0100);
    pushU16(static_cast<uint16_t>(8 + body.size()));
    pushU16(sequence_);
    pushU16(0x0000);

    packet.insert(packet.end(), body.begin(), body.end());
    transport_->send(packet);

    // Truncating cast wraps at 16 bits exactly like Python's `& 0xFFFF`.
    sequence_ = static_cast<uint16_t>(sequence_ + 1);
}

void ViscaCamera::zoomIn(uint8_t speed) {
    sendBody({0x81, 0x01, 0x04, 0x07, static_cast<uint8_t>(0x20 + speed), 0xFF});
}

void ViscaCamera::zoomOut(uint8_t speed) {
    sendBody({0x81, 0x01, 0x04, 0x07, static_cast<uint8_t>(0x30 + speed), 0xFF});
}

void ViscaCamera::zoomStop() {
    sendBody({0x81, 0x01, 0x04, 0x07, 0x00, 0xFF});
}

void ViscaCamera::panTilt(uint8_t panSpeed, uint8_t tiltSpeed, uint8_t panDir, uint8_t tiltDir) {
    sendBody({0x81, 0x01, 0x06, 0x01, panSpeed, tiltSpeed, panDir, tiltDir, 0xFF});
}

void ViscaCamera::panTiltStop() {
    // Always the *normal* speed constants and stop/stop direction, regardless of which stick or
    // fine/normal mode was last active - matches ptz_v4.py's pan_tilt_stop() exactly.
    panTilt(visca::kDefaultPanSpeed, visca::kDefaultTiltSpeed, visca::kDirStop, visca::kDirStop);
}
