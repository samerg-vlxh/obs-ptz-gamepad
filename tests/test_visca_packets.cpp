// Verifies ViscaCamera's byte output against the worked examples in the project plan, computed
// by hand from ptz_v4.py's struct.pack(">HHHH", ...) header and VISCA command bodies.

#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>

#include "../src/visca/ViscaCamera.h"
#include "../src/visca/ViscaConstants.h"
#include "TestHarness.h"

namespace {

class FakeTransport : public IViscaTransport {
public:
    std::vector<std::vector<uint8_t>> packets;

    void send(const std::vector<uint8_t>& packet) override { packets.push_back(packet); }
};

std::string hex(const std::vector<uint8_t>& bytes) {
    std::ostringstream oss;
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (i) oss << ' ';
        oss << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(bytes[i]);
    }
    return oss.str();
}

}  // namespace

void testZoomInFirstPacket() {
    auto transport = std::make_shared<FakeTransport>();
    ViscaCamera cam(transport);

    cam.zoomIn(visca::kDefaultZoomSpeed);

    EXPECT_EQ(transport->packets.size(), 1u);
    EXPECT_EQ(hex(transport->packets[0]), "01 00 00 0E 00 01 00 00 81 01 04 07 27 FF");
}

void testPanTiltSecondPacketSequenceIncrements() {
    auto transport = std::make_shared<FakeTransport>();
    ViscaCamera cam(transport);

    cam.zoomIn(visca::kDefaultZoomSpeed);  // seq=1
    cam.panTilt(visca::kDefaultPanSpeed, visca::kDefaultTiltSpeed, visca::kDirPanLeft, visca::kDirStop);  // seq=2

    EXPECT_EQ(transport->packets.size(), 2u);
    EXPECT_EQ(hex(transport->packets[1]), "01 00 00 11 00 02 00 00 81 01 06 01 10 10 01 03 FF");
}

void testPanTiltStopAlwaysUsesNormalSpeed() {
    auto transport = std::make_shared<FakeTransport>();
    ViscaCamera cam(transport);

    cam.panTiltStop();

    EXPECT_EQ(transport->packets.size(), 1u);
    EXPECT_EQ(hex(transport->packets[0]), "01 00 00 11 00 01 00 00 81 01 06 01 10 10 03 03 FF");
}

void testZoomOutAndZoomStopBodies() {
    auto transport = std::make_shared<FakeTransport>();
    ViscaCamera cam(transport);

    cam.zoomOut(visca::kDefaultZoomSpeed);
    cam.zoomStop();

    EXPECT_EQ(hex(transport->packets[0]), "01 00 00 0E 00 01 00 00 81 01 04 07 37 FF");
    EXPECT_EQ(hex(transport->packets[1]), "01 00 00 0E 00 02 00 00 81 01 04 07 00 FF");
}

void testSequenceWrapsAt16Bit() {
    auto transport = std::make_shared<FakeTransport>();
    ViscaCamera cam(transport);

    // 65535 sends use sequence numbers 1..65535; the internal counter then wraps to 0 (matches
    // Python's `(self.sequence + 1) & 0xFFFF`, which never revisits 1 - it wraps to 0).
    for (int i = 0; i < 65535; ++i) {
        cam.zoomStop();
    }
    cam.zoomStop();

    const auto& last = transport->packets.back();
    EXPECT_EQ(static_cast<int>(last[4]), 0);
    EXPECT_EQ(static_cast<int>(last[5]), 0);
}

void testIndependentSequenceCountersPerCamera() {
    auto transportA = std::make_shared<FakeTransport>();
    auto transportB = std::make_shared<FakeTransport>();
    ViscaCamera camA(transportA);
    ViscaCamera camB(transportB);

    camA.zoomStop();
    camA.zoomStop();
    camB.zoomStop();

    EXPECT_EQ(static_cast<int>(transportA->packets[1][5]), 2);
    EXPECT_EQ(static_cast<int>(transportB->packets[0][5]), 1);
}

int main() {
    testZoomInFirstPacket();
    testPanTiltSecondPacketSequenceIncrements();
    testPanTiltStopAlwaysUsesNormalSpeed();
    testZoomOutAndZoomStopBodies();
    testSequenceWrapsAt16Bit();
    testIndependentSequenceCountersPerCamera();

    std::cout << (test::totalCount() - test::failureCount()) << "/" << test::totalCount() << " assertions passed\n";
    return test::failureCount() == 0 ? 0 : 1;
}
