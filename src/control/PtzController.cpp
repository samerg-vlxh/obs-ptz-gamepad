#include "PtzController.h"

#include <cassert>
#include <cmath>

namespace {

size_t axisIndex(PtzAxis axis) {
    switch (axis) {
        case PtzAxis::LeftX:
            return 0;
        case PtzAxis::LeftY:
            return 1;
        case PtzAxis::RightX:
            return 2;
        case PtzAxis::RightY:
            return 3;
    }
    return 0;
}

}  // namespace

PtzController::PtzController(std::vector<IViscaSink*> cameras, PtzSpeeds speeds, float deadzone)
    : cameras_(std::move(cameras)), speeds_(speeds), deadzone_(deadzone) {
    assert(!cameras_.empty());
}

void PtzController::setOnEvent(std::function<void(const std::string&)> callback) {
    onEvent_ = std::move(callback);
}

void PtzController::emitEvent(const std::string& message) {
    if (onEvent_) {
        onEvent_(message);
    }
}

IViscaSink* PtzController::activeCam() {
    std::lock_guard<std::mutex> lock(mutex_);
    return cameras_[activeIndex_];
}

int PtzController::activeCameraIndex() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(activeIndex_);
}

PtzSpeeds PtzController::speedsSnapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return speeds_;
}

float PtzController::deadzoneSnapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return deadzone_;
}

void PtzController::updateSpeeds(const PtzSpeeds& speeds) {
    std::lock_guard<std::mutex> lock(mutex_);
    speeds_ = speeds;
}

void PtzController::setDeadzone(float deadzone) {
    std::lock_guard<std::mutex> lock(mutex_);
    deadzone_ = deadzone;
}

void PtzController::setActive(size_t index) {
    if (index >= cameras_.size()) {
        return;
    }

    IViscaSink* outgoing = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (activeIndex_ == index) {
            return;
        }
        outgoing = cameras_[activeIndex_];
        activeIndex_ = index;
    }

    // Fix vs. ptz_v4.py: the original left the previously-active camera moving indefinitely if it
    // was mid-pan/zoom when you switched away. Stop it explicitly instead.
    outgoing->zoomStop();
    outgoing->panTiltStop();

    emitEvent("Active camera -> " + std::to_string(index + 1));
}

void PtzController::handleButtonDown(PtzButton button) {
    switch (button) {
        case PtzButton::A:
            setActive(0);
            break;
        case PtzButton::B:
            setActive(1);
            break;
        case PtzButton::LeftShoulder:
            activeCam()->zoomOut(speedsSnapshot().zoom);
            break;
        case PtzButton::RightShoulder:
            activeCam()->zoomIn(speedsSnapshot().zoom);
            break;
    }
}

void PtzController::handleButtonUp(PtzButton button) {
    switch (button) {
        case PtzButton::LeftShoulder:
        case PtzButton::RightShoulder:
            activeCam()->zoomStop();
            break;
        case PtzButton::A:
        case PtzButton::B:
            break;
    }
}

uint8_t PtzController::dirFromPan(float value) {
    if (value < 0) return 0x01;
    if (value > 0) return 0x02;
    return 0x03;
}

uint8_t PtzController::dirFromTilt(float value) {
    // Numerically identical to dirFromPan, but kept as a separate function on purpose - see the
    // comment in ViscaConstants.h on why this reversal must not be merged away.
    if (value < 0) return 0x01;
    if (value > 0) return 0x02;
    return 0x03;
}

void PtzController::handleAxis(PtzAxis axis, float rawValue) {
    float value = std::fabs(rawValue) < deadzoneSnapshot() ? 0.0f : rawValue;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        lastAxis_[axisIndex(axis)] = value;
    }
    recomputePanTilt();
}

void PtzController::recomputePanTilt() {
    float leftX, leftY, rightX, rightY;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        leftX = lastAxis_[0];
        leftY = lastAxis_[1];
        rightX = lastAxis_[2];
        rightY = lastAxis_[3];
    }

    IViscaSink* cam = activeCam();
    PtzSpeeds speeds = speedsSnapshot();

    // Left stick (normal speed) takes exclusive priority over the right stick (fine speed) - if
    // left has any deflection at all, right is ignored entirely for this command, even if right
    // is also deflected. Only when left is fully zero does right get used. Matches
    // ptz_v4.py's handle_axis exactly.
    if (leftX != 0.0f || leftY != 0.0f) {
        cam->panTilt(speeds.pan, speeds.tilt, dirFromPan(leftX), dirFromTilt(leftY));
    } else if (rightX != 0.0f || rightY != 0.0f) {
        cam->panTilt(speeds.finePan, speeds.fineTilt, dirFromPan(rightX), dirFromTilt(rightY));
    } else {
        cam->panTiltStop();
    }
}

void PtzController::shutdown() {
    for (IViscaSink* cam : cameras_) {
        cam->zoomStop();
        cam->panTiltStop();
    }
}
