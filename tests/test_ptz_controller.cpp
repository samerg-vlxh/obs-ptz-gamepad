// Verifies PtzController's control-logic against the behavior spec extracted from ptz_v4.py's
// PTZApp: deadzone persistence, stick priority, direction byte mapping, camera-switch handling,
// and shutdown.

#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include "../src/control/PtzController.h"
#include "TestHarness.h"

namespace {

// PtzController can have more than one stop-reaffirmation future in flight at once targeting the
// same camera (e.g. two closely-spaced axis events each transitioning into "stopped" schedule
// their own reaffirm - see PtzController::reaffirmStopAsync), and those background threads race
// against both each other AND the test's own main-thread assertions. The real ViscaCamera
// serializes concurrent sends internally (see its sendMutex_); this fake needs the same guarantee
// on both writes AND reads - an unprotected `.size()`/`.back()` read racing a mutex-protected
// write is still a data race (confirmed via ThreadSanitizer), the mutex on the write side alone
// isn't sufficient. Hence `calls_` is private with every access, read or write, going through the
// same lock.
class FakeSink : public IViscaSink {
public:
    void zoomIn(uint8_t speed) override {
        push("zoomIn(" + std::to_string(static_cast<int>(speed)) + ")");
    }
    void zoomOut(uint8_t speed) override {
        push("zoomOut(" + std::to_string(static_cast<int>(speed)) + ")");
    }
    void zoomStop() override { push("zoomStop()"); }
    void panTilt(uint8_t panSpeed, uint8_t tiltSpeed, uint8_t panDir, uint8_t tiltDir) override {
        std::ostringstream oss;
        oss << "panTilt(" << static_cast<int>(panSpeed) << "," << static_cast<int>(tiltSpeed) << ","
            << static_cast<int>(panDir) << "," << static_cast<int>(tiltDir) << ")";
        push(oss.str());
    }
    void panTiltStop() override { push("panTiltStop()"); }

    size_t callCount() {
        std::lock_guard<std::mutex> lock(mutex_);
        return calls_.size();
    }
    std::string callAt(size_t i) {
        std::lock_guard<std::mutex> lock(mutex_);
        return calls_.at(i);
    }
    std::string lastCall() {
        std::lock_guard<std::mutex> lock(mutex_);
        return calls_.back();
    }
    bool callsEmpty() {
        std::lock_guard<std::mutex> lock(mutex_);
        return calls_.empty();
    }
    void clearCalls() {
        std::lock_guard<std::mutex> lock(mutex_);
        calls_.clear();
    }

private:
    void push(std::string call) {
        std::lock_guard<std::mutex> lock(mutex_);
        calls_.push_back(std::move(call));
    }

    std::vector<std::string> calls_;
    std::mutex mutex_;
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
    cam1.clearCalls();
    cam2.clearCalls();

    controller.handleButtonDown(PtzButton::B);  // switch to camera 2

    EXPECT_EQ(cam1.callCount(), 2u);
    EXPECT_EQ(cam1.callAt(0), "zoomStop()");
    EXPECT_EQ(cam1.callAt(1), "panTiltStop()");
    EXPECT_TRUE(cam2.callsEmpty());  // switching alone doesn't command the newly-active camera
}

void testSwitchingToSameCameraIsNoOp() {
    FakeSink cam1, cam2;
    PtzController controller({&cam1, &cam2});

    controller.handleButtonDown(PtzButton::A);  // already active
    EXPECT_TRUE(cam1.callsEmpty());
    EXPECT_TRUE(cam2.callsEmpty());
}

void testShoulderButtonsZoomActiveCameraOnly() {
    FakeSink cam1, cam2;
    PtzController controller({&cam1, &cam2});

    controller.handleButtonDown(PtzButton::RightShoulder);
    controller.handleButtonUp(PtzButton::RightShoulder);
    controller.handleButtonDown(PtzButton::LeftShoulder);
    controller.handleButtonUp(PtzButton::LeftShoulder);

    EXPECT_EQ(cam1.callCount(), 4u);
    EXPECT_EQ(cam1.callAt(0), "zoomIn(7)");
    EXPECT_EQ(cam1.callAt(1), "zoomStop()");
    EXPECT_EQ(cam1.callAt(2), "zoomOut(7)");
    EXPECT_EQ(cam1.callAt(3), "zoomStop()");
    EXPECT_TRUE(cam2.callsEmpty());
}

void testDeadzoneSuppressesSmallValues() {
    FakeSink cam1;
    PtzController controller({&cam1});

    controller.handleAxis(PtzAxis::LeftX, 0.1f);  // below default 0.2 deadzone

    EXPECT_EQ(cam1.callCount(), 1u);
    EXPECT_EQ(cam1.callAt(0), "panTiltStop()");
}

void testAxisValuesPersistUntilOwnNextEvent() {
    FakeSink cam1;
    PtzController controller({&cam1});

    controller.handleAxis(PtzAxis::LeftX, 1.0f);  // pan right
    cam1.clearCalls();
    controller.handleAxis(PtzAxis::LeftY, 0.0f);  // LeftX must still be remembered as 1.0

    EXPECT_EQ(cam1.callCount(), 1u);
    EXPECT_EQ(cam1.callAt(0), "panTilt(16,16,2,3)");
}

void testDirectionByteMapping() {
    FakeSink cam1;
    PtzController controller({&cam1});

    controller.handleAxis(PtzAxis::LeftX, -1.0f);
    EXPECT_EQ(cam1.lastCall(), "panTilt(16,16,1,3)");

    controller.handleAxis(PtzAxis::LeftX, 1.0f);
    EXPECT_EQ(cam1.lastCall(), "panTilt(16,16,2,3)");

    controller.handleAxis(PtzAxis::LeftX, 0.0f);
    controller.handleAxis(PtzAxis::LeftY, -1.0f);
    EXPECT_EQ(cam1.lastCall(), "panTilt(16,16,3,1)");

    controller.handleAxis(PtzAxis::LeftY, 1.0f);
    EXPECT_EQ(cam1.lastCall(), "panTilt(16,16,3,2)");
}

void testLeftStickTakesPriorityOverRightStick() {
    FakeSink cam1;
    PtzController controller({&cam1});

    controller.handleAxis(PtzAxis::RightX, 1.0f);  // fine pan right - fine speeds are 0x04=4
    EXPECT_EQ(cam1.lastCall(), "panTilt(4,4,2,3)");

    controller.handleAxis(PtzAxis::LeftX, -1.0f);  // normal stick engages -> takes over exclusively
    EXPECT_EQ(cam1.lastCall(), "panTilt(16,16,1,3)");
}

void testBothSticksZeroSendsStop() {
    FakeSink cam1;
    PtzController controller({&cam1});

    controller.handleAxis(PtzAxis::LeftX, 1.0f);
    controller.handleAxis(PtzAxis::LeftX, 0.0f);
    controller.handleAxis(PtzAxis::LeftY, 0.0f);

    EXPECT_EQ(cam1.lastCall(), "panTiltStop()");
}

void testShutdownStopsAllCamerasRegardlessOfActive() {
    FakeSink cam1, cam2;
    PtzController controller({&cam1, &cam2});

    controller.handleButtonDown(PtzButton::B);  // camera 2 active
    cam1.clearCalls();
    cam2.clearCalls();

    controller.shutdown();

    EXPECT_EQ(cam1.callCount(), 2u);
    EXPECT_EQ(cam1.callAt(0), "zoomStop()");
    EXPECT_EQ(cam1.callAt(1), "panTiltStop()");
    EXPECT_EQ(cam2.callCount(), 2u);
    EXPECT_EQ(cam2.callAt(0), "zoomStop()");
    EXPECT_EQ(cam2.callAt(1), "panTiltStop()");
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
