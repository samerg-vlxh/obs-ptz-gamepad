#pragma once

#include <cstdint>

// Defaults ported 1:1 from ptz_v4.py's module-level constants.
namespace visca {

inline constexpr uint16_t kDefaultPort = 52381;

inline constexpr uint8_t kDirStop = 0x03;
inline constexpr uint8_t kDirPanLeft = 0x01;
inline constexpr uint8_t kDirPanRight = 0x02;
// Tilt byte values are numerically identical to pan's (negative->0x01, positive->0x02), but are
// kept as a separate helper (see PtzController::dirFromTilt) because ptz_v4.py intentionally
// reverses which physical stick direction maps to which VISCA byte here, to match the camera's
// mounted orientation. Collapsing pan/tilt into one shared function would make that reversal easy
// to lose in a future edit.
inline constexpr uint8_t kDirTiltDown = 0x01;
inline constexpr uint8_t kDirTiltUp = 0x02;

inline constexpr uint8_t kDefaultZoomSpeed = 0x07;
inline constexpr uint8_t kDefaultPanSpeed = 0x10;
inline constexpr uint8_t kDefaultTiltSpeed = 0x10;
inline constexpr uint8_t kDefaultFinePanSpeed = 0x04;
inline constexpr uint8_t kDefaultFineTiltSpeed = 0x04;

inline constexpr float kDefaultDeadzone = 0.2f;

}  // namespace visca
