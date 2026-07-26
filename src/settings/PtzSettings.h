#pragma once

#include <cstdint>
#include <string>

#include "../control/PtzController.h"  // for PtzSpeeds

// Defaults match ptz_v4.py's hardcoded constants exactly, so a fresh install behaves identically
// to the Python script until the user edits something in the settings dialog.
struct PtzSettings {
    std::string cam1Ip = "192.168.20.41";
    std::string cam2Ip = "192.168.20.42";
    uint16_t viscaPort = 52381;
    PtzSpeeds speeds;
    float deadzone = 0.2f;

    // Reads obs_module_config_path("config.json"); returns defaults if the file doesn't exist yet
    // (first run) or is missing individual keys (forward-compatible with older config versions).
    static PtzSettings load();

    // Writes obs_module_config_path("config.json"), creating the containing directory if needed.
    void save() const;
};
