/*
obs-ptz-gamepad
Copyright (C) 2026 Sascha <sascha@merg.de>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include <memory>
#include <vector>

#include <QMetaObject>
#include <QObject>
#include <QString>

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>

#include "control/PtzController.h"
#include "input/GameControllerBridge.h"
#include "settings/PtzSettings.h"
#include "ui/PtzDockWidget.h"
#include "ui/PtzSettingsDialog.h"
#include "visca/UdpViscaTransport.h"
#include "visca/ViscaCamera.h"

namespace {

constexpr const char *kDockId = "ptz-gamepad-dock";

PtzSettings g_settings;

std::shared_ptr<UdpViscaTransport> g_transport1;
std::shared_ptr<UdpViscaTransport> g_transport2;
std::unique_ptr<ViscaCamera> g_cam1;
std::unique_ptr<ViscaCamera> g_cam2;
std::unique_ptr<PtzController> g_controller;
std::unique_ptr<GameControllerBridge> g_bridge;

// Non-owning: obs_frontend_add_dock_by_id() hands the widget to OBS's main window, which takes
// ownership of it. This pointer is only for posting updates via invokeMethod; it must never be
// deleted from here.
PtzDockWidget *g_dock = nullptr;

void updateActiveCameraLabel() {
    if (!g_dock || !g_controller) {
        return;
    }
    int index = g_controller->activeCameraIndex();
    const std::string &ip = (index == 0) ? g_settings.cam1Ip : g_settings.cam2Ip;
    QString label = QString("Active camera: %1 (%2)").arg(index + 1).arg(QString::fromStdString(ip));
    QMetaObject::invokeMethod(g_dock, "onActiveCameraChanged", Qt::QueuedConnection, Q_ARG(QString, label));
}

void logToDock(const std::string &message) {
    if (!g_dock) {
        return;
    }
    QMetaObject::invokeMethod(g_dock, "onLogEvent", Qt::QueuedConnection,
                               Q_ARG(QString, QString::fromStdString(message)));
}

// Raw (pre-deadzone) values, indexed the same way as PtzController's internal lastAxis_: LeftX,
// LeftY, RightX, RightY. Tracked separately here purely for the dock's live diagnostic display -
// PtzController's own copy stays post-deadzone and private to its control logic.
float g_rawAxis[4] = {0.0f, 0.0f, 0.0f, 0.0f};

size_t axisSlot(PtzAxis axis) {
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

void updateStickValuesLabel() {
    if (!g_dock) {
        return;
    }
    QString text = QString("L: %1, %2   R: %3, %4")
                       .arg(g_rawAxis[0], 0, 'f', 2)
                       .arg(g_rawAxis[1], 0, 'f', 2)
                       .arg(g_rawAxis[2], 0, 'f', 2)
                       .arg(g_rawAxis[3], 0, 'f', 2);
    QMetaObject::invokeMethod(g_dock, "onStickValuesChanged", Qt::QueuedConnection, Q_ARG(QString, text));
}

// Called on the Qt main thread (PtzDockWidget::settingsRequested is a direct signal/slot
// connection triggered by a button click), so it's safe to touch g_transport1/g_transport2 and
// g_controller directly here without an invokeMethod hop.
void openSettingsDialog() {
    if (!g_dock) {
        return;
    }

    PtzSettingsDialog dialog(g_settings, g_dock);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    g_settings = dialog.settings();
    g_settings.save();

    if (g_transport1) {
        g_transport1->setEndpoint(g_settings.cam1Ip, g_settings.viscaPort);
    }
    if (g_transport2) {
        g_transport2->setEndpoint(g_settings.cam2Ip, g_settings.viscaPort);
    }
    if (g_controller) {
        g_controller->updateSpeeds(g_settings.speeds);
        g_controller->setDeadzone(g_settings.deadzone);
    }

    logToDock("Settings updated");
    updateActiveCameraLabel();
}

class ControllerAdapter : public IControllerEventSink {
public:
    void onControllerConnected(const std::string &name) override {
        obs_log(LOG_INFO, "PTZ: controller connected: %s", name.c_str());
        logToDock("Controller connected: " + name);
        if (g_dock) {
            QString status = QString("Controller: %1 (connected)").arg(QString::fromStdString(name));
            QMetaObject::invokeMethod(g_dock, "onControllerStatusChanged", Qt::QueuedConnection,
                                       Q_ARG(QString, status));
        }
    }

    void onControllerDisconnected() override {
        obs_log(LOG_INFO, "PTZ: controller disconnected");
        logToDock("Controller disconnected");
        if (g_dock) {
            QMetaObject::invokeMethod(g_dock, "onControllerStatusChanged", Qt::QueuedConnection,
                                       Q_ARG(QString, QString("Controller: not connected")));
        }
    }

    void onButtonChanged(PtzButton button, bool pressed) override {
        if (!g_controller) {
            return;
        }
        if (pressed) {
            g_controller->handleButtonDown(button);
        } else {
            g_controller->handleButtonUp(button);
        }
    }

    void onAxisChanged(PtzAxis axis, float rawValue) override {
        g_rawAxis[axisSlot(axis)] = rawValue;
        updateStickValuesLabel();
        if (g_controller) {
            g_controller->handleAxis(axis, rawValue);
        }
    }
};

ControllerAdapter g_adapter;

}  // namespace

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

bool obs_module_load(void) {
    obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);

    g_settings = PtzSettings::load();

    g_transport1 = std::make_shared<UdpViscaTransport>(g_settings.cam1Ip, g_settings.viscaPort);
    g_transport2 = std::make_shared<UdpViscaTransport>(g_settings.cam2Ip, g_settings.viscaPort);
    g_cam1 = std::make_unique<ViscaCamera>(g_transport1);
    g_cam2 = std::make_unique<ViscaCamera>(g_transport2);

    g_controller = std::make_unique<PtzController>(std::vector<IViscaSink *>{g_cam1.get(), g_cam2.get()},
                                                    g_settings.speeds, g_settings.deadzone);
    g_controller->setOnEvent([](const std::string &message) {
        obs_log(LOG_INFO, "PTZ: %s", message.c_str());
        logToDock(message);
        updateActiveCameraLabel();
    });

    // obs_frontend_add_dock_by_id() is safe to call here: OBS's InitOBSCallbacks() (which makes
    // the frontend API usable) runs before obs_load_all_modules2() during startup, verified
    // against the actual OBS 31.1.1 sources this plugin was built against - no need to defer to
    // OBS_FRONTEND_EVENT_FINISHED_LOADING.
    g_dock = new PtzDockWidget();
    QObject::connect(g_dock, &PtzDockWidget::settingsRequested, g_dock, &openSettingsDialog);
    obs_frontend_add_dock_by_id(kDockId, obs_module_text("DockTitle"), g_dock);
    updateActiveCameraLabel();

    g_bridge = std::make_unique<GameControllerBridge>();
    g_bridge->start(&g_adapter);

    return true;
}

void obs_module_unload(void) {
    if (g_bridge) {
        g_bridge->stop();
    }
    // Unconditional stop on both cameras, regardless of which was active - mirrors ptz_v4.py's
    // KeyboardInterrupt handler.
    if (g_controller) {
        g_controller->shutdown();
    }

    obs_frontend_remove_dock(kDockId);
    g_dock = nullptr;  // owned by OBS's main window, not deleted here

    g_bridge.reset();
    g_controller.reset();
    g_cam1.reset();
    g_cam2.reset();
    g_transport1.reset();
    g_transport2.reset();

    obs_log(LOG_INFO, "plugin unloaded");
}
