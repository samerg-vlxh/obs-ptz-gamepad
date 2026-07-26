#pragma once

#include <memory>
#include <string>

#include "../control/PtzTypes.h"

// Plain-C++ facing side of the GameController.framework bridge. Kept free of any ObjC/Cocoa types
// so it can be included from PtzController/plugin-main.cpp without pulling Objective-C into the
// rest of the codebase - only GameControllerBridge.mm knows about GCController.
class IControllerEventSink {
public:
    virtual ~IControllerEventSink() = default;

    virtual void onControllerConnected(const std::string& name) = 0;
    virtual void onControllerDisconnected() = 0;
    virtual void onButtonChanged(PtzButton button, bool pressed) = 0;
    // rawValue is in the SAME sign convention ptz_v4.py used with pygame (X: -1=left/+1=right,
    // Y: -1=up/+1=down) - the .mm implementation negates GameController.framework's Y axis
    // (which reports +1=up) before calling this, specifically so PtzController's direction-byte
    // logic (ported verbatim from the Python deadzone/inversion code) doesn't need to know which
    // input backend produced the value.
    virtual void onAxisChanged(PtzAxis axis, float rawValue) = 0;
};

class GameControllerBridge {
public:
    GameControllerBridge();
    ~GameControllerBridge();

    GameControllerBridge(const GameControllerBridge&) = delete;
    GameControllerBridge& operator=(const GameControllerBridge&) = delete;

    // Does not take ownership of `sink`; caller must keep it alive until stop() (or destruction).
    void start(IControllerEventSink* sink);
    void stop();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
