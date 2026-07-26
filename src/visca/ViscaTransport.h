#pragma once

#include <cstdint>
#include <vector>

// Abstraction over "send these bytes to the camera", so ViscaCamera can be unit-tested with a
// fake transport instead of a real UDP socket.
class IViscaTransport {
public:
    virtual ~IViscaTransport() = default;
    virtual void send(const std::vector<uint8_t>& packet) = 0;
};
