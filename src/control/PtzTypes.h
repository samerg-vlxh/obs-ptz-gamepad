#pragma once

// Semantic controller events - deliberately not raw SDL/HID indices. ptz_v4.py used raw indices
// (BTN_A=0, BTN_B=1, BTN_LB=10, BTN_RB=9) whose shoulder-button ordering doesn't match canonical
// SDL Xbox mapping tables; GameControllerBridge translates GCExtendedGamepad's named properties
// into these enums so that ambiguity can't resurface here.
enum class PtzButton { A, B, LeftShoulder, RightShoulder };
enum class PtzAxis { LeftX, LeftY, RightX, RightY };
