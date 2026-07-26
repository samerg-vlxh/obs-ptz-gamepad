// Verifies PtzController's control-logic against the behavior spec extracted from ptz_v4.py's
// PTZApp: deadzone persistence, stick priority, direction byte mapping, camera-switch handling,
// and shutdown.

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "../src/control/PtzController.h"
#include "TestHarness.h"

namespace {

class FakeSink : public IViscaSink {
public:
    std::vector<std::string> calls;

    void zoomIn(uint8_t speed) override { calls.push_back("zoomIn(" + std::to_string(static_cast<int>(speed)) + ")"); }
    void zoomOut(uint8_t speed) override {
        calls.push_back("zoomOut(" + std::to_string(static_cast<int>(speed)) + ")");
    }
    void zoomStop() override { calls.push_back("zoomStop()"); }
    void panTilt(uint8_t panSpeed, uint8_t tiltSpeed, uint8_t panDir, uint8_t tiltDir) override {
        std::ostringstream oss;
        oss << "panTilt(" << static_cast<int>(panSpeed) << "," << static_cast<int>(tiltSpeed) << ","
            << static_cast<int>(panDir) << "," << static_cast<int>(tiltDir) << ")";
        calls.push_back(oss.str());
    }
    void panTiltStop() override { calls.push_back("panTiltStop()"); }
};

}  // namespace

void testButtonSwitchesActiveCamera() {
    FakeSink cam1, cam2;
    PtzController controller({&cam1, &cam2});

    EXPECT_EQ(controller.activeCameraIndex(), 0);
    controller.handleButtonDown(PtzButton::B);
    EXPECT_EQ(controller.activeCameraIndex(), 1);
    controller.handleButtonDown(PtzButton::A);
    EXPECT_EQ(controller.activeCameraIndex(), 0);
}

void testSwitchingCameraStopsOutgoingCamera() {
    FakeSink cam1, cam2;
    PtzController controller({&cam1, &cam2});

    controller.handleAxis(PtzAxis::LeftX, 1.0f);  // cam1 now "moving"
    cam1.calls.clear();
    cam2.calls.clear();

    controller.handleButtonDown(PtzButton::B);  // switch to camera 2

    EXPECT_EQ(cam1.calls.size(), 2u);
    EXPECT_EQ(cam1.calls[0], "zoomStop()");
    EXPECT_EQ(cam1.calls[1], "panTiltStop()");
    EXPECT_TRUE(cam2.calls.empty());  // switching alone doesn't command the newly-active camera
}

void testSwitchingToSameCameraIsNoOp() {
    FakeSink cam1, cam2;
    PtzController controller({&cam1, &cam2});

    controller.handleButtonDown(PtzButton::A);  // already active
    EXPECT_TRUE(cam1.calls.empty());
    EXPECT_TRUE(cam2.calls.empty());
}

void testShoulderButtonsZoomActiveCameraOnly() {
    FakeSink cam1, cam2;
    PtzController controller({&cam1, &cam2});

    controller.handleButtonDown(PtzButton::RightShoulder);
    controller.handleButtonUp(PtzButton::RightShoulder);
    controller.handleButtonDown(PtzButton::LeftShoulder);
    controller.handleButtonUp(PtzButton::LeftShoulder);

    EXPECT_EQ(cam1.calls.size(), 4u);
    EXPECT_EQ(cam1.calls[0], "zoomIn(7)");
    EXPECT_EQ(cam1.calls[1], "zoomStop()");
    EXPECT_EQ(cam1.calls[2], "zoomOut(7)");
    EXPECT_EQ(cam1.calls[3], "zoomStop()");
    EXPECT_TRUE(cam2.calls.empty());
}

void testDeadzoneSuppressesSmallValues() {
    FakeSink cam1;
    PtzController controller({&cam1});

    controller.handleAxis(PtzAxis::LeftX, 0.1f);  // below default 0.2 deadzone

    EXPECT_EQ(cam1.calls.size(), 1u);
    EXPECT_EQ(cam1.calls[0], "panTiltStop()");
}

void testAxisValuesPersistUntilOwnNextEvent() {
    FakeSink cam1;
    PtzController controller({&cam1});

    controller.handleAxis(PtzAxis::LeftX, 1.0f);  // pan right
    cam1.calls.clear();
    controller.handleAxis(PtzAxis::LeftY, 0.0f);  // LeftX must still be remembered as 1.0

    EXPECT_EQ(cam1.calls.size(), 1u);
    EXPECT_EQ(cam1.calls[0], "panTilt(16,16,2,3)");
}

void testDirectionByteMapping() {
    FakeSink cam1;
    PtzController controller({&cam1});

    controller.handleAxis(PtzAxis::LeftX, -1.0f);
    EXPECT_EQ(cam1.calls.back(), "panTilt(16,16,1,3)");

    controller.handleAxis(PtzAxis::LeftX, 1.0f);
    EXPECT_EQ(cam1.calls.back(), "panTilt(16,16,2,3)");

    controller.handleAxis(PtzAxis::LeftX, 0.0f);
    controller.handleAxis(PtzAxis::LeftY, -1.0f);
    EXPECT_EQ(cam1.calls.back(), "panTilt(16,16,3,1)");

    controller.handleAxis(PtzAxis::LeftY, 1.0f);
    EXPECT_EQ(cam1.calls.back(), "panTilt(16,16,3,2)");
}

void testLeftStickTakesPriorityOverRightStick() {
    FakeSink cam1;
    PtzController controller({&cam1});

    controller.handleAxis(PtzAxis::RightX, 1.0f);  // fine pan right - fine speeds are 0x04=4
    EXPECT_EQ(cam1.calls.back(), "panTilt(4,4,2,3)");

    controller.handleAxis(PtzAxis::LeftX, -1.0f);  // normal stick engages -> takes over exclusively
    EXPECT_EQ(cam1.calls.back(), "panTilt(16,16,1,3)");
}

void testBothSticksZeroSendsStop() {
    FakeSink cam1;
    PtzController controller({&cam1});

    controller.handleAxis(PtzAxis::LeftX, 1.0f);
    controller.handleAxis(PtzAxis::LeftX, 0.0f);
    controller.handleAxis(PtzAxis::LeftY, 0.0f);

    EXPECT_EQ(cam1.calls.back(), "panTiltStop()");
}

void testShutdownStopsAllCamerasRegardlessOfActive() {
    FakeSink cam1, cam2;
    PtzController controller({&cam1, &cam2});

    controller.handleButtonDown(PtzButton::B);  // camera 2 active
    cam1.calls.clear();
    cam2.calls.clear();

    controller.shutdown();

    EXPECT_EQ(cam1.calls.size(), 2u);
    EXPECT_EQ(cam1.calls[0], "zoomStop()");
    EXPECT_EQ(cam1.calls[1], "panTiltStop()");
    EXPECT_EQ(cam2.calls.size(), 2u);
    EXPECT_EQ(cam2.calls[0], "zoomStop()");
    EXPECT_EQ(cam2.calls[1], "panTiltStop()");
}

int main() {
    testButtonSwitchesActiveCamera();
    testSwitchingCameraStopsOutgoingCamera();
    testSwitchingToSameCameraIsNoOp();
    testShoulderButtonsZoomActiveCameraOnly();
    testDeadzoneSuppressesSmallValues();
    testAxisValuesPersistUntilOwnNextEvent();
    testDirectionByteMapping();
    testLeftStickTakesPriorityOverRightStick();
    testBothSticksZeroSendsStop();
    testShutdownStopsAllCamerasRegardlessOfActive();

    std::cout << (test::totalCount() - test::failureCount()) << "/" << test::totalCount() << " assertions passed\n";
    return test::failureCount() == 0 ? 0 : 1;
}
