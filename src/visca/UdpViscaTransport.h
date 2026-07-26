#pragma once

#include <cstdint>
#include <mutex>
#include <string>

#include "ViscaTransport.h"

// POSIX UDP socket, one instance per camera. Fire-and-forget: mirrors ptz_v4.py's ViscaCamera,
// which never connect()s and never reads a reply.
class UdpViscaTransport : public IViscaTransport {
public:
    UdpViscaTransport(std::string ip, uint16_t port);
    ~UdpViscaTransport() override;

    UdpViscaTransport(const UdpViscaTransport&) = delete;
    UdpViscaTransport& operator=(const UdpViscaTransport&) = delete;

    void send(const std::vector<uint8_t>& packet) override;

    // Lets Phase 7 settings changes retarget an existing camera slot in place, so PtzController's
    // cameras_ vector (raw pointers into ViscaCamera instances) never has to be rebuilt just
    // because the user edited an IP/port in the settings dialog.
    void setEndpoint(std::string ip, uint16_t port);

private:
    int fd_ = -1;

    std::mutex endpointMutex_;
    std::string ip_;
    uint16_t port_;
};
