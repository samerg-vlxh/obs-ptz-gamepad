#include "UdpViscaTransport.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>

UdpViscaTransport::UdpViscaTransport(std::string ip, uint16_t port) : ip_(std::move(ip)), port_(port) {
    fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_ < 0) {
        std::fprintf(stderr, "UdpViscaTransport: socket() failed for %s:%u\n", ip_.c_str(), port_);
    }
}

UdpViscaTransport::~UdpViscaTransport() {
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

void UdpViscaTransport::send(const std::vector<uint8_t>& packet) {
    if (fd_ < 0) {
        return;
    }

    std::string ip;
    uint16_t port;
    {
        std::lock_guard<std::mutex> lock(endpointMutex_);
        ip = ip_;
        port = port_;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (::inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1) {
        std::fprintf(stderr, "UdpViscaTransport: invalid IP address '%s'\n", ip.c_str());
        return;
    }

    // No connect(): each send targets ip_/port_ directly, matching ptz_v4.py's fire-and-forget
    // sendto() usage. Failures are logged and swallowed, never thrown - a dropped VISCA command
    // is not fatal to the controller.
    ssize_t sent =
        ::sendto(fd_, packet.data(), packet.size(), 0, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (sent < 0) {
        std::fprintf(stderr, "UdpViscaTransport: send failed: %s\n", std::strerror(errno));
    }
}

void UdpViscaTransport::setEndpoint(std::string ip, uint16_t port) {
    std::lock_guard<std::mutex> lock(endpointMutex_);
    ip_ = std::move(ip);
    port_ = port;
}
