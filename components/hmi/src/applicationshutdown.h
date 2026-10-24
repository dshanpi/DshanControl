#pragma once
#include "mediasession.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QWidget>
// Own the normal last-window-close path while the main event loop is alive.
// Direct QCoreApplication::exit/quit and OS signals are outside this path.
class ApplicationShutdown final : public QObject
{
 public:
    ApplicationShutdown(QApplication& app, QWidget*& window)
        : QObject(&app), app(app), window(window)
    {
        app.setQuitOnLastWindowClosed(false);
        connect(&app, &QApplication::lastWindowClosed, this, [this]() {
            // The close event must finish before destroying its receiver.
            QTimer::singleShot(0, this, [this]() { requestShutdown(); });
        });
        poll.setInterval(20);
        connect(&poll, &QTimer::timeout, this, [this]() {
            if (!MediaSession::hasRetiringWorkers()) finish(false);
            else if (elapsed.elapsed() >= 3500) finish(true);
        });
    }
    void requestShutdown()
    {
        if (draining) return;
        draining = true;
        elapsed.start();
        QWidget* retiringWindow = window;
        window = nullptr;
        delete retiringWindow;
        poll.start();
    }
    bool deadlineExpired() const { return timedOut; }
 private:
    void finish(bool expired)
    {
        poll.stop();
        timedOut = expired;
        if (expired) qCritical("Media retirement deadline expired during application shutdown");
        app.quit();
    }
    QApplication& app;
    QWidget*& window;
    QTimer poll;
    QElapsedTimer elapsed;
    bool draining = false, timedOut = false;
};
