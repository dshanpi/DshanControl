#include "desktopcanvas.h"
namespace DesktopV1
{
static const QColor Ink("#142C4B");
int renderFixtures(QApplication &app, const QString &directory)
{
    DesktopCanvas w(true);
    QString dir = directory;
    QDir().mkpath(dir);
    QList<QPair<QString, int>> shots = {
        {"UI新桌面_01组件桌面.png", 0}, {"UI新桌面_02顶部下拉.png", 1},
        {"UI新桌面_03全部应用.png", 2}, {"UI新桌面_04编辑组件.png", 3},
        {"UI新桌面_05添加组件.png", 4}, {"UI新桌面_06组件展开.png", 5}};
    QStringList files;
    for (const auto &s : shots)
    {
        w.selected = 3;
        w.change(s.second);
        w.show();
        app.processEvents();
        QImage image(w.size(), QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        w.render(&image);
        if (!image.save(dir + "/" + s.first))
            return 1;
        files << s.first;
    }
    w.resize(1280, 720);
    w.change(0);
    app.processEvents();
    QImage wide(w.size(), QImage::Format_ARGB32);
    wide.fill(Qt::transparent);
    w.render(&wide);
    wide.save(dir + "/UI新桌面_07宽屏1280x720.png");
    w.resize(1024, 600);
    w.desk = 1;
    w.change(0);
    app.processEvents();
    QImage custom(w.size(), QImage::Format_ARGB32);
    custom.fill(Qt::transparent);
    w.render(&custom);
    custom.save(dir + "/UI新桌面_09图片组件.png");
    w.desk = 0;
    w.change(2);
    w.search->setText("网络");
    app.processEvents();
    QImage filtered(w.size(), QImage::Format_ARGB32);
    filtered.fill(Qt::transparent);
    w.render(&filtered);
    filtered.save(dir + "/UI新桌面_08应用搜索.png");
    QImage board(2136, 2028, QImage::Format_RGB32);
    board.fill(QColor("#E6EDF6"));
    QPainter p(&board);
    QStringList titles = {"01 / 组件桌面", "02 / 顶部下拉：控制中心", "03 / 底部上拉：全部应用",
                          "04 / 编辑组件", "05 / 添加组件库",         "06 / 组件展开"};
    for (int i = 0; i < files.size(); i++)
    {
        int x = 28 + (i % 2) * 1052, y = 20 + (i / 2) * 668;
        w.txt(p, x, y, 1024, 38, titles[i], 22, Ink, true);
        p.drawImage(QRectF(x, y + 46, 1024, 600), QImage(dir + "/" + files[i]));
    }
    p.end();
    board.save(dir + "/UI新桌面_00设计总览.png");
    return 0;
}
int runDesktopChecks(QApplication &app)
{
    DesktopCanvas w(true);
    w.show();
    app.processEvents();
    auto drag = [&](QPoint a, QPoint b) {
        QMouseEvent press(QEvent::MouseButtonPress, a, Qt::LeftButton, Qt::LeftButton,
                          Qt::NoModifier);
        QApplication::sendEvent(&w, &press);
        QMouseEvent release(QEvent::MouseButtonRelease, b, Qt::LeftButton, Qt::NoButton,
                            Qt::NoModifier);
        QApplication::sendEvent(&w, &release);
        app.processEvents();
    };
    auto check = [](bool ok, const char *name) {
        if (!ok)
            qFatal("FAIL: %s", name);
        qInfo("PASS: %s", name);
    };
    check(catalog().size() == 12, "twelve stable application routes");
    drag(QPoint(500, 24), QPoint(500, 170));
    check(w.state == 1, "top swipe opens control center");
    drag(QPoint(512, 458), QPoint(512, 458));
    check(w.state == 0, "control center closes");
    drag(QPoint(512, 570), QPoint(512, 340));
    check(w.state == 2, "bottom swipe opens application drawer");
    w.search->setText("网络");
    app.processEvents();
    check(w.search->text() == "网络", "real search input");
    drag(QPoint(960, 140), QPoint(960, 140));
    check(w.state == 0 && w.search->text().isEmpty(), "close clears search");
    drag(QPoint(200, 250), QPoint(50, 250));
    check(w.desk == 1, "horizontal page switch");
    w.renderState(3);
    app.processEvents();
    drag(QPoint(473, 124), QPoint(473, 124));
    check(w.hidden.contains(0), "widget removal preview");
    DesktopCanvas live(false);
    check(live.model.temperature == "--" && live.model.alarmCount == "--",
          "live mode has no fixture telemetry");
    QString route;
    QObject::connect(&live, &DesktopCanvas::appRequested, [&](const QString &id) { route = id; });
    for (int i = 0; i < catalog().size(); ++i)
    {
        live.openApp(i);
        check(route == catalog()[i].id, "stable route dispatch");
    }
    for (int i = 0; i < 6; ++i)
    {
        w.renderState(i);
        app.processEvents();
        QImage image(w.size(), QImage::Format_ARGB32);
        w.render(&image);
        check(!image.isNull(), "shared native renderer");
    }
    return 0;
}
} // namespace DesktopV1
