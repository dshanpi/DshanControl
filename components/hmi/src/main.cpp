#include <QApplication>
#include <QFont>
#include <QScreen>
#include "mainwindow.h"

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
    return app.exec();
}
