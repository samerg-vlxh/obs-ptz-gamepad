#pragma once

#include <QWidget>

class QLabel;
class QPlainTextEdit;
class QPushButton;

// Registered via obs_frontend_add_dock_by_id(), which takes a plain QWidget (not QDockWidget) -
// OBS wraps it internally. All three slots are the only entry points ever touched from outside
// the Qt main thread; callers must go through QMetaObject::invokeMethod(..., Qt::QueuedConnection,
// ...) rather than calling them directly.
class PtzDockWidget : public QWidget {
    Q_OBJECT

public:
    explicit PtzDockWidget(QWidget *parent = nullptr);

public slots:
    void onControllerStatusChanged(const QString &status);
    void onActiveCameraChanged(const QString &label);
    void onLogEvent(const QString &message);

signals:
    void settingsRequested();

private:
    QLabel *controllerStatusLabel_;
    QLabel *activeCameraLabel_;
    QPlainTextEdit *eventLog_;
    QPushButton *settingsButton_;
};
