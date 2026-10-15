#include <QApplication>
#include <QFont>
#include <QScreen>
#include "mainwindow.h"
#include "applicationheartbeat.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("DshanControl");
    app.setOverrideCursor(Qt::BlankCursor);
    QFont font("WenQuanYi Zen Hei", 12);
    font.setStyleStrategy(QFont::PreferAntialias);
    app.setFont(font);
    MainWindow window;
    if (app.primaryScreen()) window.setGeometry(app.primaryScreen()->geometry());
    window.showFullScreen();
    const auto heartbeatFile = qEnvironmentVariable("DSHANCONTROL_HMI_HEARTBEAT");
    if (!heartbeatFile.isEmpty())
        new ApplicationHeartbeat(heartbeatFile,
            qEnvironmentVariable("DSHANCONTROL_APPLICATION_COMMIT"),
            QUrl(qEnvironmentVariable("HEALTH_URL", "http://127.0.0.1/api/health")), &app);
    return app.exec();
}
