#import "GameControllerBridge.h"

#import <Foundation/Foundation.h>
#import <GameController/GameController.h>

// Observes controller connect/disconnect and installs value-changed handlers on the extended
// gamepad profile. Only controllers exposing `extendedGamepad` are supported, matching
// ptz_v4.py's implicit assumption of a full Xbox-style pad.
@interface PtzControllerObserver : NSObject
@property(nonatomic, assign) IControllerEventSink* sink;
@property(nonatomic, strong) GCController* controller;

- (void)start;
- (void)stop;

@end

@implementation PtzControllerObserver

- (void)start {
    [[NSNotificationCenter defaultCenter] addObserver:self
                                              selector:@selector(handleControllerConnected:)
                                                  name:GCControllerDidConnectNotification
                                                object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                              selector:@selector(handleControllerDisconnected:)
                                                  name:GCControllerDidDisconnectNotification
                                                object:nil];

    // Pick up anything already connected before start() was called.
    for (GCController* existing in GCController.controllers) {
        [self attachHandlers:existing];
        if (self.controller) {
            break;
        }
    }
}

- (void)stop {
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    self.controller = nil;
    self.sink = nullptr;
}

- (void)handleControllerConnected:(NSNotification*)note {
    if (self.controller) {
        // v1 only tracks a single active controller, matching ptz_v4.py's unconditional
        // `Joystick(0)`. A second controller connecting doesn't take over.
        return;
    }
    [self attachHandlers:(GCController*)note.object];
}

- (void)handleControllerDisconnected:(NSNotification*)note {
    if (note.object == self.controller) {
        self.controller = nil;
        if (self.sink) {
            self.sink->onControllerDisconnected();
        }
    }
}

- (void)attachHandlers:(GCController*)controller {
    GCExtendedGamepad* gamepad = controller.extendedGamepad;
    if (!gamepad) {
        return;
    }

    self.controller = controller;

    IControllerEventSink* sink = self.sink;
    if (sink) {
        NSString* name = controller.vendorName ?: @"Unknown controller";
        sink->onControllerConnected(std::string(name.UTF8String));
    }

    gamepad.buttonA.valueChangedHandler = ^(GCControllerButtonInput*, float, BOOL pressed) {
      if (sink) sink->onButtonChanged(PtzButton::A, pressed);
    };
    gamepad.buttonB.valueChangedHandler = ^(GCControllerButtonInput*, float, BOOL pressed) {
      if (sink) sink->onButtonChanged(PtzButton::B, pressed);
    };
    gamepad.leftShoulder.valueChangedHandler = ^(GCControllerButtonInput*, float, BOOL pressed) {
      if (sink) sink->onButtonChanged(PtzButton::LeftShoulder, pressed);
    };
    gamepad.rightShoulder.valueChangedHandler = ^(GCControllerButtonInput*, float, BOOL pressed) {
      if (sink) sink->onButtonChanged(PtzButton::RightShoulder, pressed);
    };

    gamepad.leftThumbstick.valueChangedHandler = ^(GCControllerDirectionPad*, float x, float y) {
      if (!sink) return;
      sink->onAxisChanged(PtzAxis::LeftX, x);
      // Negate Y: GameController.framework reports +1.0 for "pushed up", pygame/SDL reported
      // -1.0 for the same physical direction. See the comment on IControllerEventSink::onAxisChanged.
      sink->onAxisChanged(PtzAxis::LeftY, -y);
    };
    gamepad.rightThumbstick.valueChangedHandler = ^(GCControllerDirectionPad*, float x, float y) {
      if (!sink) return;
      sink->onAxisChanged(PtzAxis::RightX, x);
      sink->onAxisChanged(PtzAxis::RightY, -y);
    };
}

@end

class GameControllerBridge::Impl {
public:
    PtzControllerObserver* observer = nil;
};

GameControllerBridge::GameControllerBridge() : impl_(std::make_unique<Impl>()) {}

GameControllerBridge::~GameControllerBridge() {
    stop();
}

void GameControllerBridge::start(IControllerEventSink* sink) {
    if (impl_->observer) {
        return;
    }
    impl_->observer = [[PtzControllerObserver alloc] init];
    impl_->observer.sink = sink;
    [impl_->observer start];
}

void GameControllerBridge::stop() {
    if (impl_->observer) {
        [impl_->observer stop];
        impl_->observer = nil;
    }
}
