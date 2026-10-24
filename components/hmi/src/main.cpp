#include <QApplication>
#include <QFont>
#include <QScreen>
#include "mainwindow.h"
#include "applicationheartbeat.h"
#include "desktop/desktopshell.h"
#include "displayprobe.h"
#include "applicationshutdown.h"
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("DshanControl");
    QString fontError;
    if (!DesktopV1::DesktopCanvas::loadFonts(&fontError)) {
        qCritical("%s", qPrintable(fontError));
        return 2;
    }
    const auto arguments = app.arguments();
    const int displayRender = arguments.indexOf("--display-probe-render");
    if (displayRender >= 0)
        return displayRender + 1 < arguments.size() &&
               DisplayProbe::reference().save(arguments[displayRender + 1]) ? 0 : 2;
    if (arguments.contains("--desktop-self-test"))
        return DesktopV1::runDesktopChecks(app);
    const int render = arguments.indexOf("--desktop-render");
    if (render >= 0)
        return render + 1 < arguments.size()
            ? DesktopV1::renderFixtures(app, arguments[render + 1]) : 2;
    const bool windowed = arguments.contains("--windowed");
    if (!windowed) app.setOverrideCursor(Qt::BlankCursor);
    QFont font("Dshan Sans", 12);
    font.setStyleStrategy(QFont::PreferAntialias);
    app.setFont(font);
    QWidget *window = arguments.contains("--legacy-desktop")
        ? static_cast<QWidget *>(new MainWindow)
        : static_cast<QWidget *>(new DesktopV1::DesktopShell);
    if (windowed) window->show();
    else {
        if (app.primaryScreen()) window->setGeometry(app.primaryScreen()->geometry());
        window->showFullScreen();
    }
    const int capture = arguments.indexOf("--desktop-capture");
    if (capture >= 0 && capture + 1 < arguments.size()) {
        const QString path = arguments[capture + 1];
        QTimer::singleShot(1000, window, [window, path]() {
            if (!window->grab().save(path)) qWarning("Could not save desktop capture");
        });
    }
    const auto heartbeatFile = qEnvironmentVariable("DSHANCONTROL_HMI_HEARTBEAT");
    if (!heartbeatFile.isEmpty())
        new ApplicationHeartbeat(heartbeatFile,
            qEnvironmentVariable("DSHANCONTROL_APPLICATION_COMMIT"),
            QUrl(qEnvironmentVariable("HEALTH_URL", "http://127.0.0.1/api/health")), &app);
    ApplicationShutdown shutdown(app, window);
    const int result = app.exec();
    delete window;
    return result == 0 && shutdown.deadlineExpired() ? 2 : result;
}
