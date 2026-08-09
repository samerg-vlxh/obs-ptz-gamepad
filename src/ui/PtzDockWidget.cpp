#include "PtzDockWidget.h"

#include <QDateTime>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
constexpr int kMaxLogLines = 200;
}  // namespace

PtzDockWidget::PtzDockWidget(QWidget *parent) : QWidget(parent) {
    controllerStatusLabel_ = new QLabel(tr("Controller: not connected"), this);
    activeCameraLabel_ = new QLabel(tr("Active camera: -"), this);
    stickValuesLabel_ = new QLabel(tr("L: 0.00, 0.00   R: 0.00, 0.00"), this);

    eventLog_ = new QPlainTextEdit(this);
    eventLog_->setReadOnly(true);
    eventLog_->setMaximumBlockCount(kMaxLogLines);

    settingsButton_ = new QPushButton(tr("Settings..."), this);
    connect(settingsButton_, &QPushButton::clicked, this, &PtzDockWidget::settingsRequested);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(controllerStatusLabel_);
    layout->addWidget(activeCameraLabel_);
    layout->addWidget(stickValuesLabel_);
    layout->addWidget(eventLog_);
    layout->addWidget(settingsButton_);
    setLayout(layout);
}

void PtzDockWidget::onControllerStatusChanged(const QString &status) {
    controllerStatusLabel_->setText(status);
}

void PtzDockWidget::onActiveCameraChanged(const QString &label) {
    activeCameraLabel_->setText(label);
}

void PtzDockWidget::onLogEvent(const QString &message) {
    eventLog_->appendPlainText(QDateTime::currentDateTime().toString("HH:mm:ss") + "  " + message);
}

void PtzDockWidget::onStickValuesChanged(const QString &values) {
    stickValuesLabel_->setText(values);
}
