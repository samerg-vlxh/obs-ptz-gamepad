#include "PtzSettingsDialog.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {
// VISCA speed bytes are single bytes in the protocol (see ViscaCamera::zoomIn/panTilt), so the
// dialog caps entry at 255 to match what can actually be sent on the wire.
constexpr int kMaxSpeedByte = 255;
}  // namespace

PtzSettingsDialog::PtzSettingsDialog(const PtzSettings &current, QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("PTZ Gamepad Settings"));

    cam1IpEdit_ = new QLineEdit(QString::fromStdString(current.cam1Ip), this);
    cam2IpEdit_ = new QLineEdit(QString::fromStdString(current.cam2Ip), this);

    portSpin_ = new QSpinBox(this);
    portSpin_->setRange(1, 65535);
    portSpin_->setValue(current.viscaPort);

    zoomSpeedSpin_ = new QSpinBox(this);
    zoomSpeedSpin_->setRange(0, kMaxSpeedByte);
    zoomSpeedSpin_->setValue(current.speeds.zoom);

    panSpeedSpin_ = new QSpinBox(this);
    panSpeedSpin_->setRange(0, kMaxSpeedByte);
    panSpeedSpin_->setValue(current.speeds.pan);

    tiltSpeedSpin_ = new QSpinBox(this);
    tiltSpeedSpin_->setRange(0, kMaxSpeedByte);
    tiltSpeedSpin_->setValue(current.speeds.tilt);

    finePanSpeedSpin_ = new QSpinBox(this);
    finePanSpeedSpin_->setRange(0, kMaxSpeedByte);
    finePanSpeedSpin_->setValue(current.speeds.finePan);

    fineTiltSpeedSpin_ = new QSpinBox(this);
    fineTiltSpeedSpin_->setRange(0, kMaxSpeedByte);
    fineTiltSpeedSpin_->setValue(current.speeds.fineTilt);

    deadzoneSpin_ = new QDoubleSpinBox(this);
    deadzoneSpin_->setRange(0.0, 1.0);
    deadzoneSpin_->setSingleStep(0.05);
    deadzoneSpin_->setValue(current.deadzone);

    auto *form = new QFormLayout();
    form->addRow(tr("Camera 1 IP:"), cam1IpEdit_);
    form->addRow(tr("Camera 2 IP:"), cam2IpEdit_);
    form->addRow(tr("VISCA port:"), portSpin_);
    form->addRow(tr("Zoom speed:"), zoomSpeedSpin_);
    form->addRow(tr("Pan speed:"), panSpeedSpin_);
    form->addRow(tr("Tilt speed:"), tiltSpeedSpin_);
    form->addRow(tr("Fine pan speed:"), finePanSpeedSpin_);
    form->addRow(tr("Fine tilt speed:"), fineTiltSpeedSpin_);
    form->addRow(tr("Deadzone:"), deadzoneSpin_);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
    setLayout(layout);
}

PtzSettings PtzSettingsDialog::settings() const {
    PtzSettings result;
    result.cam1Ip = cam1IpEdit_->text().trimmed().toStdString();
    result.cam2Ip = cam2IpEdit_->text().trimmed().toStdString();
    result.viscaPort = static_cast<uint16_t>(portSpin_->value());
    result.speeds.zoom = static_cast<uint8_t>(zoomSpeedSpin_->value());
    result.speeds.pan = static_cast<uint8_t>(panSpeedSpin_->value());
    result.speeds.tilt = static_cast<uint8_t>(tiltSpeedSpin_->value());
    result.speeds.finePan = static_cast<uint8_t>(finePanSpeedSpin_->value());
    result.speeds.fineTilt = static_cast<uint8_t>(fineTiltSpeedSpin_->value());
    result.deadzone = static_cast<float>(deadzoneSpin_->value());
    return result;
}
