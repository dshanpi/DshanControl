#include <QtWidgets>
#include <QtNetwork>
#include <QtTest>
#include <functional>
#include <memory>
#include <tuple>
// Inspect painted hit regions, but deliver real mouse events to each control.
#define private public
#include "../../components/hmi/src/desktop/desktopcanvas.h"
#undef private

class DesktopFixtureIsolationTest : public QObject
{
    Q_OBJECT
    QTemporaryDir stateDir;
    QTcpServer rejectingProxy;
    QLocalServer rejectingProvider;
    int httpConnections = 0, ipcConnections = 0;
    QByteArray originalState, originalSocket;
    bool stateWasSet = false, socketWasSet = false;
    QNetworkProxy originalProxy;

    void render(DesktopV1::DesktopCanvas &canvas)
    {
        QImage image(canvas.size(), QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        canvas.render(&image);
        QCoreApplication::processEvents();
    }

  private slots:
    void initTestCase()
    {
        QVERIFY(stateDir.isValid());
        stateWasSet = qEnvironmentVariableIsSet("DSHANCONTROL_DESKTOP_STATE_DIR");
        socketWasSet = qEnvironmentVariableIsSet("DSHANCONTROL_DESKTOP_SOCKET");
        originalState = qgetenv("DSHANCONTROL_DESKTOP_STATE_DIR");
        originalSocket = qgetenv("DSHANCONTROL_DESKTOP_SOCKET");
        qputenv("DSHANCONTROL_DESKTOP_STATE_DIR", stateDir.filePath("layout").toUtf8());
        const auto socket = stateDir.filePath("provider.sock");
        qputenv("DSHANCONTROL_DESKTOP_SOCKET", socket.toUtf8());
        QVERIFY(rejectingProvider.listen(socket));
        connect(&rejectingProvider, &QLocalServer::newConnection, this, [this]() {
            while (rejectingProvider.hasPendingConnections()) {
                ++ipcConnections;
                auto *connection = rejectingProvider.nextPendingConnection();
                connection->abort(); connection->deleteLater();
            }
        });
        QVERIFY(rejectingProxy.listen(QHostAddress::LocalHost));
        originalProxy = QNetworkProxy::applicationProxy();
        QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,
            "127.0.0.1", rejectingProxy.serverPort()));
        connect(&rejectingProxy, &QTcpServer::newConnection, this, [this]() {
            while (rejectingProxy.hasPendingConnections()) {
                ++httpConnections;
                auto *connection = rejectingProxy.nextPendingConnection();
                connection->abort(); connection->deleteLater();
            }
        });
        QString error;
        QVERIFY2(DesktopV1::DesktopCanvas::loadFonts(&error), qPrintable(error));
    }

    void everyPaintedFixtureControlStaysLocal()
    {
        int clicks = 0, screens = 0;
        const auto check = [&](int state, int page, int group, const QString &query) {
            DesktopV1::DesktopCanvas inventory(true);
            inventory.group = group;
            inventory.renderState(state, page);
            inventory.search->setText(query);
            render(inventory);
            QList<QPoint> targets;
            // Later hit regions take precedence in mouseReleaseEvent. Skip regions
            // whose center is covered by a later target; this includes backdrops.
            for (int i = 0; i < inventory.hits.size(); ++i) {
                const auto center = inventory.hits[i].first.center();
                bool covered = false;
                for (int j = i + 1; j < inventory.hits.size(); ++j)
                    covered |= inventory.hits[j].first.contains(center);
                if (!covered) targets << center.toPoint();
            }
            QVERIFY(!targets.isEmpty());
            ++screens;
            for (const auto &target : targets) {
                DesktopV1::DesktopCanvas canvas(true);
                QSignalSpy routes(&canvas, &DesktopV1::DesktopCanvas::appRequested);
                QSignalSpy contexts(&canvas, &DesktopV1::DesktopCanvas::appContextRequested);
                QSignalSpy brightness(&canvas, &DesktopV1::DesktopCanvas::brightnessRequested);
                canvas.group = group;
                canvas.renderState(state, page);
                canvas.search->setText(query);
                canvas.show(); render(canvas);
                QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, target);
                QCoreApplication::processEvents();
                QCOMPARE(routes.count(), 0);
                QCOMPARE(contexts.count(), 0);
                QCOMPARE(brightness.count(), 0);
                QVERIFY(canvas.model.fixture);
                QVERIFY(!canvas.layoutStore);
                QVERIFY(!canvas.imageCache);
                QVERIFY(!canvas.keyboard);
                QVERIFY(!QFileInfo::exists(stateDir.filePath("layout")));
                ++clicks;
            }
        };
        for (int page = 0; page < 2; ++page)
            for (int state = 0; state < 6; ++state)
                check(state, page, 0, QString());
        for (int group = 1; group < 4; ++group)
            check(2, 0, group, QString());
        check(2, 0, 0, QString::fromUtf8("网络"));
        check(2, 0, 0, "no-matching-application");
        QTest::qWait(50);
        QCOMPARE(httpConnections, 0);
        QCOMPARE(ipcConnections, 0);
        QVERIFY(clicks > 100);
        QCOMPARE(screens, 17);
        qInfo("FIXTURE_COVERAGE screens=%d actual_control_clicks=%d http_connections=%d ipc_connections=%d",
              screens, clicks, httpConnections, ipcConnections);
    }

    void liveUnavailableNeverReturnsToExampleData()
    {
        DesktopV1::DesktopCanvas live;
        QVERIFY(!live.model.fixture);
        for (int i = 0; i < 3; ++i) {
            live.applyHmi({});
            live.applyServices({});
            QVERIFY(!live.model.fixture);
            QVERIFY(!live.model.online);
            QCOMPARE(live.model.temperature, QString("--"));
            QCOMPARE(live.model.alarmCount, QString("--"));
            QVERIFY(live.model.cloudState != DesktopV1::DisplayModel::example().cloudState);
        }
    }

    void liveSmallTargetsHaveMinimumAreasAndDoNotOverlapPeers()
    {
        int measured = 0;
        const auto validate = [&](const QList<QRectF> &regions) {
            for (int i = 0; i < regions.size(); ++i) {
                QVERIFY(regions[i].width() >= 44);
                QVERIFY(regions[i].height() >= 44);
                QVERIFY(QRectF(0, 0, 1024, 600).contains(regions[i]));
                for (int j = i + 1; j < regions.size(); ++j)
                    QVERIFY2(!regions[i].intersects(regions[j]), "Sibling controls overlap");
                ++measured;
            }
        };
        DesktopV1::DesktopCanvas canvas(false);
        canvas.animationsEnabled = false;
        canvas.renderState(3); render(canvas);
        QList<QRectF> deletes;
        for (const auto &hit : canvas.hits)
            if (hit.first.width() == 44 && hit.first.height() == 44)
                deletes << hit.first;
        QCOMPARE(deletes.size(), 7);
        QList<QRectF> editor = deletes;
        for (const auto &hit : canvas.hits)
            if (hit.first.top() == 64) editor << hit.first;
        QCOMPARE(editor.size(), 10); // Seven deletes plus three editor toolbar actions.
        validate(editor);
        canvas.discardLayout();
        canvas.change(2); render(canvas);
        QList<QRectF> drawer;
        for (const auto &hit : canvas.hits)
            if (hit.first.top() == 174 || hit.first.top() == 117)
                drawer << hit.first;
        QCOMPARE(drawer.size(), 5); // Four categories and drawer close.
        validate(drawer);
        canvas.change(4); render(canvas);
        QList<QRectF> library;
        for (const auto &hit : canvas.hits)
            if (hit.first.top() == 77) library << hit.first;
        QCOMPARE(library.size(), 1);
        validate(library);
        canvas.libraryGroup = 1; render(canvas);
        QList<QRectF> back;
        for (const auto &hit : canvas.hits)
            if (hit.first.top() == 112) back << hit.first;
        QCOMPARE(back.size(), 1);
        validate(back);
        qInfo("SMALL_TARGET_GEOMETRY live_controls=%d minimum=44px logical_size=1024x600", measured);
    }

    void deleteTargetEdgesOnlyRemoveTheirOwnDraftWidget()
    {
        int checks = 0;
        for (int slot = 0; slot < 7; ++slot) {
            for (int edge = 0; edge < 5; ++edge) {
                DesktopV1::DesktopCanvas canvas(false);
                canvas.animationsEnabled = false;
                canvas.renderState(3); canvas.show(); render(canvas);
                QList<QRectF> deletes;
                for (const auto &hit : canvas.hits)
                    if (hit.first.width() == 44 && hit.first.height() == 44)
                        deletes << hit.first;
                QCOMPARE(deletes.size(), 7);
                const auto r = deletes[slot];
                const QList<QPointF> points = {r.center(), r.topLeft() + QPointF(1,1),
                    r.topRight() + QPointF(-1,1), r.bottomLeft() + QPointF(1,-1),
                    r.bottomRight() + QPointF(-1,-1)};
                const auto before = canvas.layoutSnapshot();
                const auto layoutPath = stateDir.filePath("layout/layout-v1.json");
                const bool fileExisted = QFileInfo::exists(layoutPath);
                QByteArray savedBytes;
                if (fileExisted) {
                    QFile file(layoutPath); QVERIFY(file.open(QIODevice::ReadOnly));
                    savedBytes = file.readAll();
                }
                QList<QJsonObject> initialWidgets;
                for (int other = 0; other < 7; ++other) initialWidgets << canvas.widgetAt(other);
                QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, points[edge].toPoint());
                if (canvas.widgetAt(slot)["type"].toString() != "empty")
                    qInfo("DELETE_MISROUTE slot=%d edge=%d x=%d y=%d resulting_state=%d",
                          slot, edge, points[edge].toPoint().x(), points[edge].toPoint().y(), canvas.state);
                QCOMPARE(canvas.widgetAt(slot)["type"].toString(), QString("empty"));
                for (int other = 0; other < 7; ++other) {
                    if (other == slot) continue;
                    QCOMPARE(canvas.widgetAt(other), initialWidgets[other]);
                }
                QVERIFY(canvas.layoutSnapshot() != before);
                QVERIFY(canvas.layoutEditing());
                canvas.discardLayout();
                QCOMPARE(canvas.layoutSnapshot(), before);
                QCOMPARE(QFileInfo::exists(layoutPath), fileExisted);
                if (fileExisted) {
                    QFile file(layoutPath); QVERIFY(file.open(QIODevice::ReadOnly));
                    QCOMPARE(file.readAll(), savedBytes);
                }
                ++checks;
            }
        }
        qInfo("DELETE_BOUNDARIES live_controls=7 positions_per_control=5 actual_clicks=%d saved_layout_unchanged=true", checks);
    }

    void cleanupTestCase()
    {
        QNetworkProxy::setApplicationProxy(originalProxy);
        if (stateWasSet) qputenv("DSHANCONTROL_DESKTOP_STATE_DIR", originalState);
        else qunsetenv("DSHANCONTROL_DESKTOP_STATE_DIR");
        if (socketWasSet) qputenv("DSHANCONTROL_DESKTOP_SOCKET", originalSocket);
        else qunsetenv("DSHANCONTROL_DESKTOP_SOCKET");
    }
};
QTEST_MAIN(DesktopFixtureIsolationTest)
#include "desktop_fixture_isolation_test.moc"
