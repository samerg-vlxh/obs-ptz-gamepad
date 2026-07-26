#pragma once

#include <QDialog>

#include "../settings/PtzSettings.h"

class QLineEdit;
class QSpinBox;
class QDoubleSpinBox;

// Modal dialog editing IPs/port/speeds/deadzone - the UX improvement over ptz_v4.py's hardcoded
// constants. Caller is responsible for exec()'ing it, checking the QDialog::Accepted result, and
// then reading settings() to apply + persist the edited values.
class PtzSettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit PtzSettingsDialog(const PtzSettings &current, QWidget *parent = nullptr);

    PtzSettings settings() const;

private:
    QLineEdit *cam1IpEdit_;
    QLineEdit *cam2IpEdit_;
    QSpinBox *portSpin_;
    QSpinBox *zoomSpeedSpin_;
    QSpinBox *panSpeedSpin_;
    QSpinBox *tiltSpeedSpin_;
    QSpinBox *finePanSpeedSpin_;
    QSpinBox *fineTiltSpeedSpin_;
    QDoubleSpinBox *deadzoneSpin_;
};
