#include "PtzSettings.h"

#include <obs-module.h>
#include <util/platform.h>

namespace {

std::string directoryOf(const std::string &path) {
    size_t pos = path.find_last_of('/');
    return pos == std::string::npos ? std::string() : path.substr(0, pos);
}

}  // namespace

PtzSettings PtzSettings::load() {
    PtzSettings settings;  // defaults

    char *path = obs_module_config_path("config.json");
    if (!path) {
        return settings;
    }

    obs_data_t *data = obs_data_create_from_json_file(path);
    bfree(path);
    if (!data) {
        return settings;
    }

    if (obs_data_has_user_value(data, "cam1_ip")) {
        settings.cam1Ip = obs_data_get_string(data, "cam1_ip");
    }
    if (obs_data_has_user_value(data, "cam2_ip")) {
        settings.cam2Ip = obs_data_get_string(data, "cam2_ip");
    }
    if (obs_data_has_user_value(data, "visca_port")) {
        settings.viscaPort = static_cast<uint16_t>(obs_data_get_int(data, "visca_port"));
    }
    if (obs_data_has_user_value(data, "zoom_speed")) {
        settings.speeds.zoom = static_cast<uint8_t>(obs_data_get_int(data, "zoom_speed"));
    }
    if (obs_data_has_user_value(data, "pan_speed")) {
        settings.speeds.pan = static_cast<uint8_t>(obs_data_get_int(data, "pan_speed"));
    }
    if (obs_data_has_user_value(data, "tilt_speed")) {
        settings.speeds.tilt = static_cast<uint8_t>(obs_data_get_int(data, "tilt_speed"));
    }
    if (obs_data_has_user_value(data, "fine_pan_speed")) {
        settings.speeds.finePan = static_cast<uint8_t>(obs_data_get_int(data, "fine_pan_speed"));
    }
    if (obs_data_has_user_value(data, "fine_tilt_speed")) {
        settings.speeds.fineTilt = static_cast<uint8_t>(obs_data_get_int(data, "fine_tilt_speed"));
    }
    if (obs_data_has_user_value(data, "deadzone")) {
        settings.deadzone = static_cast<float>(obs_data_get_double(data, "deadzone"));
    }

    obs_data_release(data);
    return settings;
}

void PtzSettings::save() const {
    char *path = obs_module_config_path("config.json");
    if (!path) {
        return;
    }

    os_mkdirs(directoryOf(path).c_str());

    obs_data_t *data = obs_data_create();
    obs_data_set_string(data, "cam1_ip", cam1Ip.c_str());
    obs_data_set_string(data, "cam2_ip", cam2Ip.c_str());
    obs_data_set_int(data, "visca_port", viscaPort);
    obs_data_set_int(data, "zoom_speed", speeds.zoom);
    obs_data_set_int(data, "pan_speed", speeds.pan);
    obs_data_set_int(data, "tilt_speed", speeds.tilt);
    obs_data_set_int(data, "fine_pan_speed", speeds.finePan);
    obs_data_set_int(data, "fine_tilt_speed", speeds.fineTilt);
    obs_data_set_double(data, "deadzone", deadzone);

    obs_data_save_json(data, path);

    obs_data_release(data);
    bfree(path);
}
