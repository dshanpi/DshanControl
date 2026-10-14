#include "../../components/hmi/src/mainwindow.h"
#include "../../components/hmi/src/visuals.h"
#include "../../components/hmi/src/updatepanel.h"
#include <QCheckBox>
#include <QFileDialog>
#include <QDir>
#include <QTemporaryDir>
#include <QRawFont>
#include <QCryptographicHash>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QSpinBox>
#include <QTcpServer>
#include <QTcpSocket>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSlider>
#include <QStyleOptionSlider>
#include <QStackedWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QtTest>
#include <cstring>
#include <limits>

class FixtureReply : public QNetworkReply
{
  public:
    FixtureReply(const QJsonObject &data, const QString &kind) : payload(QJsonDocument(data).toJson())
    {
        setProperty("kind", kind);
        open(QIODevice::ReadOnly);
        setFinished(true);
    }
    void abort() override {}
    qint64 bytesAvailable() const override
    {
        return payload.size() - offset + QNetworkReply::bytesAvailable();
    }

  protected:
    qint64 readData(char *data, qint64 max) override
    {
        const qint64 count = qMin(max, qint64(payload.size()) - offset);
        if (count <= 0)
            return -1;
        std::memcpy(data, payload.constData() + offset, size_t(count));
        offset += count;
        return count;
    }

  private:
    QByteArray payload;
    qint64 offset = 0;
};

class HmiUiTest : public QObject
{
    Q_OBJECT
    QJsonObject snapshot()
    {
        // Explicit test-only fixture: never loaded by the production executable.
        return QJsonDocument::fromJson(R"({"ok":true,"timestamp":1791817200,
            "platform":{"board_id":"preview-only","capabilities":["ethercat","modbus-rtu","canopen","can-fd"],"channels":{
                "can0":{"kind":"can","device":"can0"},"can1":{"kind":"can","device":"can1"},
                "serial0":{"kind":"serial","role":"modbus-rtu","device":"/dev/ttyAS5"},"ec":{"kind":"ethernet","role":"ethercat","device":"ethTEST"},"lan":{"kind":"ethernet","device":"eth0"},"wifi":{"kind":"wifi","device":"wlan0"}}},
            "system":{"cpu":18,"memory":42,"temperature":46.2,"temperature_source":"real","storage":26,"uptime_seconds":31336},
            "summary":{"status":"正常","online_devices":12,"today_alarms":2},
            "buses":[{"name":"can0","state":"运行","online":true,"source":"demo","bitrate":500000},
                     {"name":"can1","state":"未连接","online":false,"source":"demo"},
                     {"name":"RS485","state":"4 个从站","online":true,"source":"demo"},
                     {"name":"EtherCAT","state":"4 个从站","online":true,"source":"demo"}],
            "networks":[{"name":"eth0","state":"已连接","address":"192.168.1.100","online":true,"source":"demo"},
                        {"name":"wlan0","state":"已连接","address":"192.168.1.200","online":true,"source":"demo"}],
            "sensors":[{"name":"温度","value":36.5,"unit":"°C","state":"正常","source":"demo"},
                       {"name":"压力","value":0.62,"unit":"MPa","state":"正常","source":"demo"},
                       {"name":"流量","value":24.8,"unit":"L/min","state":"正常","source":"demo"}],
            "alarms":{"active":2,"recent":[{"id":12,"ts":1791817100,"severity":"warning","message":"通信响应超时","source":"demo","state":"active"},{"id":13,"ts":1791817140,"severity":"warning","message":"温度接近阈值","source":"demo","state":"active"}]},
            "services":{"thingsboard":{"running":true},"ntp":{"running":true}},
            "settings":{"brightness":220,"demo_mode":true,"refresh_ms":1000,"page_cycle_seconds":0}})")
            .object();
    }
    void seedTrend(MainWindow &w)
    {
        auto data = snapshot();
        for (int point = 0; point < 60; ++point) {
            auto sensors = snapshot()["sensors"].toArray();
            for (int i = 0; i < sensors.size(); ++i) {
                auto item = sensors[i].toObject();
                item["value"] = item["value"].toDouble() + (i == 1 ? .025 : 1.2) * qSin(point * 0.6 + i);
                sensors[i] = item;
            }
            data["timestamp"] = 1791817141 + point;
            data["sensors"] = sensors;
            w.updateSnapshot(data);
        }
        w.refreshTimer.stop();
    }
    void isolate(MainWindow &w)
    {
        w.refreshTimer.stop();
        w.networkManager.disconnect(&w);
        for (auto *r : w.networkManager.findChildren<QNetworkReply *>())
            r->abort();
    }
  private slots:
    void initTestCase()
    {
        // Each clean test run, including a single selected test, owns its output.
        // Do not depend on a later layout test to create this directory.
        QVERIFY(QDir().mkpath("screenshots"));
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    }

    void brightnessTouchInteraction()
    {
        MainWindow w;
        isolate(w);
        w.resize(1024, 600);
        w.show();
        QTest::mouseClick(w.navButtons[5], Qt::LeftButton);
        w.updateSnapshot(snapshot());
        w.refreshTimer.stop();
        QTest::qWait(50);
        auto *slider = w.brightnessSlider;
        QVERIFY(slider->isVisible());
        QVERIFY(slider->height() >= 48);
        // A tap anywhere along the full-height track reaches that position,
        // instead of performing QSlider's default one-page step.
        slider->setValue(255);
        QSignalSpy pressed(slider, &QSlider::sliderPressed);
        QSignalSpy released(slider, &QSlider::sliderReleased);
        QTest::mouseClick(slider, Qt::LeftButton, Qt::NoModifier,
                          QPoint(slider->width() / 2, 3));
        QVERIFY(qAbs(slider->value() - 133) <= 3);
        QCOMPARE(pressed.count(), 1);
        QCOMPARE(released.count(), 1);
        QVERIFY(!slider->isSliderDown());
        // Starting near the handle keeps its value; dragging stays attached to
        // the finger, including outside the widget, and clamps at either end.
        const int before = slider->value();
        const QPoint grab(slider->width() / 2 + 18, 3);
        QTest::mousePress(slider, Qt::LeftButton, Qt::NoModifier, grab);
        QCOMPARE(slider->value(), before);
        QMouseEvent move(QEvent::MouseMove, QPointF(slider->width() + 100, 3),
                         Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(slider, &move);
        QCOMPARE(slider->value(), 255);
        QTest::mouseRelease(slider, Qt::LeftButton, Qt::NoModifier,
                            QPoint(slider->width() + 100, 3));
        QTest::mouseClick(slider, Qt::LeftButton, Qt::NoModifier, QPoint(0, 3));
        QCOMPARE(slider->value(), 10);
        auto *less = w.findChild<QPushButton *>("brightnessDecrease");
        auto *more = w.findChild<QPushButton *>("brightnessIncrease");
        QVERIFY(less && more);
        QVERIFY(less->width() >= 44 && more->height() >= 44);
        QTest::mouseClick(less, Qt::LeftButton);
        QCOMPARE(slider->value(), 10);
        QTest::mouseClick(more, Qt::LeftButton);
        QCOMPARE(slider->value(), 13);
        QVERIFY(w.settingsDirty);
        w.updateSnapshot(snapshot());
        w.refreshTimer.stop();
        QCOMPARE(slider->value(), 13); // periodic refresh must not undo the edit
        QTest::keyClick(slider, Qt::Key_Right);
        QCOMPARE(slider->value(), 16);
        QVERIFY(w.grab().save("screenshots/brightness-touch.png"));
    }

    void nativeUpdateTasksRequireDownloadedIdentityAndPreserveErrors()
    {
        auto json = [](const QJsonObject &object) {
            return QJsonDocument(object).toJson(QJsonDocument::Compact);
        };
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        auto write = [&](const QString &name, const QByteArray &bytes) {
            QFile file(temp.path() + "/" + name);
            return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
        };
        QVERIFY(write("updater", "#!/bin/sh\ncd \"$(dirname \"$0\")\"\nprintf '%s\\n' \"$@\" > request\ncat result\n"));
        QVERIFY(QFile::setPermissions(temp.path() + "/updater", QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        QVERIFY(write("updater.json", "{\"task_service\":true}"));
        const bool hadConfig = qEnvironmentVariableIsSet("DSHANCONTROL_UPDATER_CONFIG");
        const bool hadExecutable = qEnvironmentVariableIsSet("DSHANCONTROL_UPDATER_EXECUTABLE");
        const QByteArray previousConfig = qgetenv("DSHANCONTROL_UPDATER_CONFIG");
        const QByteArray previousExecutable = qgetenv("DSHANCONTROL_UPDATER_EXECUTABLE");
        qputenv("DSHANCONTROL_UPDATER_CONFIG", (temp.path() + "/updater.json").toLocal8Bit());
        qputenv("DSHANCONTROL_UPDATER_EXECUTABLE", (temp.path() + "/updater").toLocal8Bit());
        UpdatePanel panel;
        if (hadConfig) qputenv("DSHANCONTROL_UPDATER_CONFIG", previousConfig);
        else qunsetenv("DSHANCONTROL_UPDATER_CONFIG");
        if (hadExecutable) qputenv("DSHANCONTROL_UPDATER_EXECUTABLE", previousExecutable);
        else qunsetenv("DSHANCONTROL_UPDATER_EXECUTABLE");
        panel.show();
        const QString checked(32, 'a'), downloaded(32, 'b'), failed(32, 'c');
        const QJsonObject candidate{{"version", "1.2.3"}, {"package_sha256", QString(64, 'd')}};
        auto response = [&](const QString &id, const QString &operation, const QString &state) {
            return QJsonObject{{"task_id", id}, {"operation", operation}, {"status", state},
                               {"stage", "completed"}, {"candidate", candidate}};
        };
        QVERIFY(write("result", json(response(checked, "check", "success"))));
        panel.requestNative("update.status", {});
        QTRY_VERIFY(panel.download->isEnabled());
        QVERIFY(!panel.install->isEnabled());
        QVERIFY(panel.status->text().contains("历史"));
        QVERIFY(write("result", json(response(downloaded, "download", "success"))));
        panel.download->click();
        QTRY_VERIFY(panel.install->isEnabled());
        QFile request(temp.path() + "/request");
        QVERIFY(request.open(QIODevice::ReadOnly));
        auto args = request.readAll().split('\n');
        QCOMPARE(args[0], QByteArray("rpc"));
        QCOMPARE(args[1], QByteArray("update.start"));
        auto body = QJsonDocument::fromJson(args[2]).object();
        QCOMPARE(body["source_task_id"].toString(), checked);
        QCOMPARE(body["operation"].toString(), QString("download"));
        request.close();
        auto error = response(failed, "install", "failed");
        error["error"] = QJsonObject{{"stage", "maintenance"}, {"code", "17"}, {"message", "<b>fixture refused</b>"}};
        QVERIFY(write("result", json(error)));
        QTimer::singleShot(0, &panel, []() {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (box) QTest::mouseClick(box->button(QMessageBox::Yes), Qt::LeftButton);
        });
        panel.install->click();
        QTRY_VERIFY(panel.status->text().contains("17"));
        QVERIFY(panel.status->text().contains("maintenance"));
        QCOMPARE(panel.status->textFormat(), Qt::PlainText);
        QVERIFY(!panel.install->isEnabled());
        QVERIFY(!panel.download->isEnabled());
        QVERIFY(request.open(QIODevice::ReadOnly));
        args = request.readAll().split('\n');
        body = QJsonDocument::fromJson(args[2]).object();
        QCOMPARE(body["source_task_id"].toString(), downloaded);
        QCOMPARE(body["version"].toString(), QString("1.2.3"));
        QCOMPARE(body["package_sha256"].toString(), candidate["package_sha256"].toString());
        QCOMPARE(body["confirmed"], QJsonValue(true));
        QTest::qWait(1100);
        QVERIFY(!panel.taskPoll.isActive()); // Failure does not start another request.
    }

    void responsiveScreens()
    {
        MainWindow w;
        isolate(w);
        w.show();
        const QList<QSize> sizes = {QSize(1024, 600), QSize(1280, 720), QSize(800, 480), QSize(600, 1024),
                                    QSize(1024, 768)};
        QDir().mkpath("screenshots");
        for (QSize size : sizes) {
            w.resize(size);
            QTest::qWait(80);
            QCOMPARE(w.size(), size);
            for (int page = 0; page < 6; ++page) {
                QTest::mouseClick(w.navButtons[page], Qt::LeftButton);
                w.updateSnapshot(snapshot());
                if (page == 0 || page == 3)
                    seedTrend(w);
                if (page == 4)
                    w.handleReply(new FixtureReply(
                        {{"ok", true}, {"alarms", snapshot()["alarms"].toObject()["recent"]}}, "alarms"));
                w.refreshTimer.stop();
                QTest::qWait(80);
                QCOMPARE(w.size(), size);
                auto *scroll = qobject_cast<QScrollArea *>(w.pages->currentWidget());
                QVERIFY(scroll);
                w.grab().save(
                    QString("screenshots/%1x%2-%3.png").arg(size.width()).arg(size.height()).arg(page));
                if (size.width() >= 1024)
                    QVERIFY2(scroll->verticalScrollBar()->maximum() == 0,
                             qPrintable(QString("vertical overflow: %1x%2 page %3: %4")
                                            .arg(size.width())
                                            .arg(size.height())
                                            .arg(page)
                                            .arg(scroll->verticalScrollBar()->maximum())));
                QVERIFY2(scroll->horizontalScrollBar()->maximum() == 0,
                         qPrintable(QString("horizontal overflow: %1x%2 page %3: %4")
                                        .arg(size.width())
                                        .arg(size.height())
                                        .arg(page)
                                        .arg(scroll->horizontalScrollBar()->maximum())));
                QVERIFY(w.grab().save(
                    QString("screenshots/%1x%2-%3.png").arg(size.width()).arg(size.height()).arg(page)));
            }
            QTest::mouseClick(w.navButtons[1], Qt::LeftButton);
            auto *tabs = w.findChild<QTabWidget *>("protocolTabs");
            for (int tab = 0; tab < tabs->count(); ++tab) {
                tabs->setCurrentIndex(tab);
                QTest::qWait(50);
                for (auto *area : w.pages->currentWidget()->findChildren<QScrollArea *>())
                    if (area->isVisible()) {
                        QCOMPARE(area->horizontalScrollBar()->maximum(), 0);
                        if (size.width() >= 1024)
                            QVERIFY2(area->verticalScrollBar()->maximum() == 0,
                                     qPrintable(QString("%1x%2 tab %3 scroll %4")
                                         .arg(size.width()).arg(size.height()).arg(tab)
                                         .arg(area->verticalScrollBar()->maximum())));
                    }
                QVERIFY(w.grab().save(
                    QString("screenshots/%1x%2-bus%3.png").arg(size.width()).arg(size.height()).arg(tab)));
            }
            tabs->setCurrentIndex(0);
            auto *canTabs = w.findChild<QTabWidget *>("canControls");
            for (int tab = 0; tab < canTabs->count(); ++tab) {
                canTabs->setCurrentIndex(tab);
                QTest::qWait(30);
                for (auto *area : w.pages->currentWidget()->findChildren<QScrollArea *>())
                    if (area->isVisible()) {
                        QCOMPARE(area->horizontalScrollBar()->maximum(), 0);
                        if (size.width() >= 1024)
                            QVERIFY2(area->verticalScrollBar()->maximum() == 0,
                                     qPrintable(QString("%1x%2 tab %3 scroll %4")
                                         .arg(size.width()).arg(size.height()).arg(tab)
                                         .arg(area->verticalScrollBar()->maximum())));
                    }
                QVERIFY(w.grab().save(
                    QString("screenshots/%1x%2-can%3.png").arg(size.width()).arg(size.height()).arg(tab)));
            }
            canTabs->setCurrentIndex(0);
            QTest::mouseClick(w.navButtons[2], Qt::LeftButton);
            auto *network = w.findChild<QTabWidget *>("networkTabs");
            for (int tab = 0; tab < network->count(); ++tab) {
                network->setCurrentIndex(tab);
                QTest::qWait(30);
                for (auto *area : w.pages->currentWidget()->findChildren<QScrollArea *>())
                    if (area->isVisible()) {
                        QCOMPARE(area->horizontalScrollBar()->maximum(), 0);
                        if (size.width() >= 1024)
                            QVERIFY2(area->verticalScrollBar()->maximum() == 0,
                                     qPrintable(QString("%1x%2 tab %3 scroll %4")
                                         .arg(size.width()).arg(size.height()).arg(tab)
                                         .arg(area->verticalScrollBar()->maximum())));
                    }
                QVERIFY(w.grab().save(QString("screenshots/%1x%2-network%3.png")
                                          .arg(size.width())
                                          .arg(size.height())
                                          .arg(tab)));
            }
            network->setCurrentIndex(0);
            QTest::mouseClick(w.navButtons[5], Qt::LeftButton);
            w.handleReply(new FixtureReply({{"ok", true},
                                            {"audit", QJsonArray{QJsonObject{{"ts", 1791817100},
                                                                             {"action", "can_apply"},
                                                                             {"target", "can0"},
                                                                             {"result", "success"}},
                                                                 QJsonObject{{"ts", 1791817040},
                                                                             {"action", "service"},
                                                                             {"target", "ntp"},
                                                                             {"result", "success"}},
                                                                 QJsonObject{{"ts", 1791817000},
                                                                             {"action", "ethercat_cycle"},
                                                                             {"target", "eth0"},
                                                                             {"result", "failed"}}}}},
                                           "audit"));
            auto *settings = w.findChild<QTabWidget *>("settingsTabs");
            for (int tab = 0; tab < settings->count(); ++tab) {
                settings->setCurrentIndex(tab);
                QTest::qWait(30);
                for (auto *area : w.pages->currentWidget()->findChildren<QScrollArea *>())
                    if (area->isVisible()) {
                        QCOMPARE(area->horizontalScrollBar()->maximum(), 0);
                        if (size.width() >= 1024)
                            QVERIFY2(area->verticalScrollBar()->maximum() == 0,
                                     qPrintable(QString("%1x%2 tab %3 scroll %4")
                                         .arg(size.width()).arg(size.height()).arg(tab)
                                         .arg(area->verticalScrollBar()->maximum())));
                    }
                QVERIFY(w.grab().save(QString("screenshots/%1x%2-settings%3.png")
                                          .arg(size.width())
                                          .arg(size.height())
                                          .arg(tab)));
            }
            settings->setCurrentIndex(0);
        }
    }
    void visualTouchAndScrollReachability()
    {
        MainWindow w;
        isolate(w);
        w.show();
        QDir().mkpath("screenshots");
        QStringList errors;
        QJsonArray measurements;
        const QList<QSize> sizes = {QSize(1024, 600), QSize(1280, 720), QSize(800, 480),
                                    QSize(600, 1024), QSize(1024, 768)};
        const QStringList views = {"0", "can0", "can1", "bus1", "bus2", "network0", "network1",
                                   "network2", "3", "4", "settings0", "settings1", "settings2", "settings3"};
        const QList<int> pages = {0, 1, 1, 1, 1, 2, 2, 2, 3, 4, 5, 5, 5, 5};
        for (const auto &size : sizes) {
            w.resize(size);
            for (int v = 0; v < views.size(); ++v) {
                const auto view = views[v];
                w.showPage(pages[v]);
                if (pages[v] == 1) {
                    w.findChild<QTabWidget *>("protocolTabs")->setCurrentIndex(v < 3 ? 0 : v - 2);
                    w.findChild<QTabWidget *>("canControls")->setCurrentIndex(v == 2 ? 1 : 0);
                }
                if (pages[v] == 2) w.findChild<QTabWidget *>("networkTabs")->setCurrentIndex(v - 5);
                if (pages[v] == 5) w.findChild<QTabWidget *>("settingsTabs")->setCurrentIndex(v - 10);
                w.updateSnapshot(snapshot());
                w.refreshTimer.stop();
                if (pages[v] == 4)
                    w.handleReply(new FixtureReply({{"ok", true},
                        {"alarms", snapshot()["alarms"].toObject()["recent"]}}, "alarms"));
                if (pages[v] == 5)
                    w.handleReply(new FixtureReply({{"ok", true}, {"audit", QJsonArray{
                        QJsonObject{{"ts", 1791817100}, {"action", "service"}, {"target", "ntp"}, {"result", "failed"}},
                        QJsonObject{{"ts", 1791817000}, {"action", "can_apply"}, {"target", "can0"}, {"result", "success"}}
                    }}}, "audit"));
                if (pages[v] == 0 || pages[v] == 3) seedTrend(w);
                QTest::qWait(40);
                auto *outer = qobject_cast<QScrollArea *>(w.pages->currentWidget());
                QList<QScrollArea *> areas{outer};
                areas.append(outer->findChildren<QScrollArea *>());
                int scrolling = 0;
                for (auto *area : areas) {
                    area->verticalScrollBar()->setValue(0);
                    if (area->isVisible() && area->verticalScrollBar()->maximum() > 0) ++scrolling;
                }
                const QString stem = QString("%1x%2-review-%3").arg(size.width()).arg(size.height()).arg(view);
                QVERIFY(w.grab().save("screenshots/" + stem + ".png"));
                if (view.startsWith("can") && scrolling > 1)
                    errors << stem + " nested CAN scrolling";
                for (auto *control : outer->findChildren<QWidget *>()) {
                    if (!control->isVisible() || !(qobject_cast<QPushButton *>(control) ||
                        qobject_cast<QComboBox *>(control) || qobject_cast<QSpinBox *>(control) ||
                        (qobject_cast<QLineEdit *>(control) && !qobject_cast<QSpinBox *>(control->parentWidget()))))
                        continue;
                    measurements.append(QJsonObject{{"view", stem}, {"control", control->objectName()},
                                                     {"width", control->width()}, {"height", control->height()}});
                    if (control->height() < 44 || control->width() < 44)
                        errors << stem + " small target " + control->objectName();
                }
                for (auto *table : outer->findChildren<QTableWidget *>())
                    if (table->isVisible()) {
                        if (table->horizontalScrollBar()->maximum() != 0)
                            errors << stem + " horizontal table overflow";
                        for (int row = 0; row < table->rowCount(); ++row)
                            if (!table->isRowHidden(row) && table->rowHeight(row) < 40)
                                errors << stem + " small table row";
                    }
                // A programmatic click alone could hit a hidden control. Scroll
                // through its real ancestor viewports and require the entire
                // button to be exposed before exercising a confirmation below.
                for (auto *button : outer->findChildren<QPushButton *>()) {
                    if (!button->isVisible()) continue;
                    for (auto *parent = button->parentWidget(); parent; parent = parent->parentWidget())
                        if (auto *area = qobject_cast<QScrollArea *>(parent))
                            area->ensureWidgetVisible(button, 0, 8);
                    QTest::qWait(5);
                    if (!button->visibleRegion().contains(button->rect()))
                        errors << stem + " unreachable button " + button->text();
                }
                if (view == "0") {
                    outer->verticalScrollBar()->setValue(outer->verticalScrollBar()->maximum());
                    QTest::qWait(20);
                    QVERIFY(w.dashboard->visibleRegion().contains(w.dashboard->alarmAction.toRect()));
                    QVERIFY(w.grab().save("screenshots/" + stem + "-end.png"));
                    QTest::mouseClick(w.dashboard, Qt::LeftButton, Qt::NoModifier,
                                      w.dashboard->alarmAction.center().toPoint());
                    QCOMPARE(w.pages->currentIndex(), 4);
                    w.showPage(0);
                } else
                    QVERIFY(w.grab().save("screenshots/" + stem + "-end.png"));
                if (view.startsWith("can")) {
                    for (const QString &name : {QString("canApply"), v == 2 ? QString("canSdoWrite") : QString("canNmtSend")}) {
                        auto *button = w.findChild<QPushButton *>(name);
                        outer->ensureWidgetVisible(button, 0, 8);
                        QTest::qWait(20);
                        if (!button->visibleRegion().contains(button->rect()))
                            errors << stem + " clipped action " + name;
                        QVERIFY(w.grab().save("screenshots/" + stem + "-" + name + ".png"));
                    }
                    if (v == 2) {
                        auto *button = w.findChild<QPushButton *>("canSdoWrite");
                        bool confirmedVisible = false;
                        QTimer::singleShot(0, &w, [&]() {
                            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
                            if (box) {
                                confirmedVisible = box->text().contains("SDO");
                                QTest::mouseClick(box->button(QMessageBox::No), Qt::LeftButton);
                            }
                        });
                        QTest::mouseClick(button, Qt::LeftButton);
                        QVERIFY(confirmedVisible);
                    }
                }
                if (size == QSize(1024, 600)) {
                    for (auto *area : areas) area->verticalScrollBar()->setValue(0);
                    w.markOffline();
                    QTest::qWait(20);
                    QVERIFY(w.alarmSummaryLabel->styleSheet().contains("#b52f40"));
                    QVERIFY(w.grab().save("screenshots/" + stem + "-offline.png"));
                    auto empty = snapshot();
                    empty["platform"] = QJsonObject();
                    for (const auto &key : {"buses", "networks", "sensors"}) empty[key] = QJsonArray();
                    empty["services"] = QJsonObject();
                    empty["summary"] = QJsonObject{{"status", "未验证"}};
                    empty["alarms"] = QJsonObject{{"active", 0}, {"recent", QJsonArray()}};
                    w.updateSnapshot(empty);
                    w.refreshTimer.stop();
                    w.handleReply(new FixtureReply({{"ok", true}, {"alarms", QJsonArray()}}, "alarms"));
                    w.handleReply(new FixtureReply({{"ok", true}, {"audit", QJsonArray()}}, "audit"));
                    QTest::qWait(20);
                    QVERIFY(!w.alarmSummaryLabel->styleSheet().contains("#168345"));
                    QVERIFY(w.grab().save("screenshots/" + stem + "-empty.png"));
                }
            }
        }
        QFile evidence("touch-measurements.json");
        QVERIFY(evidence.open(QIODevice::WriteOnly));
        evidence.write(QJsonDocument(QJsonObject{{"controls", measurements},
                         {"errors", QJsonArray::fromStringList(errors)}}).toJson());
        QVERIFY2(errors.isEmpty(), qPrintable(errors.join("\n")));
    }
    void networkClassificationAndSelection()
    {
        MainWindow w;
        isolate(w);
        auto data = snapshot();
        w.updateSnapshot(data);
        w.refreshTimer.stop();
        QCOMPARE(w.networkTables[0]->rowCount(), 1);
        QCOMPARE(w.networkTables[1]->rowCount(), 1);
        QCOMPARE(w.networkTables[0]->item(0, 0)->text(), QString("eth0"));
        QCOMPARE(w.networkTables[1]->item(0, 0)->text(), QString("wlan0"));
        auto networks = data["networks"].toArray();
        networks.append(QJsonObject{{"name", "eth1"}, {"state", "down"}, {"source", "real"}});
        data["networks"] = networks;
        auto platform = data["platform"].toObject();
        auto channels = platform["channels"].toObject();
        channels["lan2"] = QJsonObject{{"kind", "ethernet"}, {"device", "eth1"}};
        platform["channels"] = channels; data["platform"] = platform;
        w.updateSnapshot(data);
        w.refreshTimer.stop();
        w.networkTables[0]->setCurrentCell(1, 0);
        QJsonArray reordered{networks[2], networks[1], networks[0]};
        data["networks"] = reordered;
        w.updateSnapshot(data);
        w.refreshTimer.stop();
        QCOMPARE(w.networkTables[0]->currentRow(), 0);
        QCOMPARE(w.networkFieldGroups[0][0]->text(), QString("eth1"));
        data["networks"] = QJsonArray();
        w.updateSnapshot(data);
        w.refreshTimer.stop();
        QCOMPARE(w.networkFieldGroups[0][0]->text(), QString("--"));
        QCOMPARE(w.networkTables[1]->rowCount(), 0);
    }
    void networkMappingRemovalAndUnknownStates()
    {
        MainWindow w;
        isolate(w);
        auto data = snapshot();
        auto platform = data["platform"].toObject();
        auto channels = platform["channels"].toObject();
        channels["renamedWifi"] = QJsonObject{{"kind", "wifi"}, {"device", "ethRADIO"}};
        platform["channels"] = channels; data["platform"] = platform;
        auto networks = data["networks"].toArray();
        networks.append(QJsonObject{{"name", "ethRADIO"}});
        networks.append(QJsonObject{{"name", "ethCELL"}, {"kind", "cellular"}});
        networks.append(QJsonObject{{"name", "enUNKNOWN"}});
        networks.append(QJsonObject{{"name", ""}});
        data["networks"] = networks;
        w.updateSnapshot(data); w.refreshTimer.stop();
        QCOMPARE(w.networkTables[0]->rowCount(), 1);
        QCOMPARE(w.networkTables[1]->rowCount(), 2);
        w.networkTables[1]->setCurrentCell(1, 0);
        QCOMPARE(w.networkFieldGroups[1][0]->text(), QString("ethRADIO"));
        QCOMPARE(w.networkFieldGroups[1][1]->text(), QString("--"));
        QCOMPARE(w.networkFieldGroups[1][2]->text(), QString("未知"));
        networks.removeAt(2); data["networks"] = networks;
        for (int refresh = 0; refresh < 2; ++refresh) {
            w.updateSnapshot(data); w.refreshTimer.stop();
            QCOMPARE(w.networkTables[1]->rowCount(), 1);
            QCOMPARE(w.networkTables[1]->currentRow(), -1);
            QCOMPARE(w.networkFieldGroups[1][0]->text(), QString("--"));
        }
        w.networkTables[1]->setCurrentCell(0, 0);
        w.updateSnapshot(data); w.refreshTimer.stop();
        QCOMPARE(w.networkFieldGroups[1][0]->text(), QString("wlan0"));
        w.markOffline();
        for (int kind = 0; kind < 2; ++kind) {
            QCOMPARE(w.networkTables[kind]->rowCount(), 0);
            for (auto *field : w.networkFieldGroups[kind]) QCOMPARE(field->text(), QString("--"));
        }
        data["platform"] = QJsonObject();
        w.updateSnapshot(data); w.refreshTimer.stop();
        QCOMPARE(w.networkTables[0]->rowCount(), 0);
        QCOMPARE(w.networkTables[1]->rowCount(), 0);
    }
    void monitorPauseDoesNotFreezeOtherPages()
    {
        MainWindow w;
        isolate(w);
        auto data = snapshot();
        w.updateSnapshot(data);
        w.refreshTimer.stop();
        const QString before = w.deviceTable->item(0, 1)->text();
        w.pauseMonitor->setChecked(true);
        auto readings = data["sensors"].toArray();
        auto temperature = readings[0].toObject();
        temperature["value"] = 49.0;
        readings[0] = temperature;
        data["sensors"] = readings;
        data["timestamp"] = data["timestamp"].toDouble() + 1;
        w.updateSnapshot(data);
        w.refreshTimer.stop();
        QCOMPARE(w.deviceTable->item(0, 1)->text(), before);
        QCOMPARE(w.dashboard->temperatures.last().y(), 49.0);
        w.pauseMonitor->setChecked(false);
        w.refreshTimer.stop();
        QCOMPARE(w.deviceTable->item(0, 1)->text(), QString("49.0"));
        readings[0] = QJsonObject{{"name", "温度"}, {"source", "unavailable"}};
        data["sensors"] = readings;
        w.updateSnapshot(data);
        w.refreshTimer.stop();
        QCOMPARE(w.deviceTable->item(0, 1)->text(), QString("--"));
        w.pauseMonitor->setChecked(true);
        w.markOffline();
        QCOMPARE(w.deviceTable->rowCount(), 0);
        QVERIFY(w.deviceTrend->temperatures.isEmpty());
    }
    void auditAndAlarmFiltersUseRawValues()
    {
        MainWindow w;
        isolate(w);
        w.handleReply(new FixtureReply(
            {{"ok", true},
             {"audit", QJsonArray{QJsonObject{{"action", "service"}, {"result", "success"}},
                                  QJsonObject{{"action", "can_apply"}, {"result", "failed"}}}}},
            "audit"));
        w.auditResult->setCurrentIndex(2);
        QVERIFY(w.auditTable->isRowHidden(0));
        QVERIFY(!w.auditTable->isRowHidden(1));
        QCOMPARE(w.auditTable->item(1, 3)->text(), QString("失败"));
        QCOMPARE(w.auditTable->item(1, 1)->text(), QString("CAN 参数配置"));
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        w.handleReply(new FixtureReply({{"ok", true},
                                        {"alarms", QJsonArray{QJsonObject{{"id", 1},
                                                                          {"ts", double(now)},
                                                                          {"source", "can0"},
                                                                          {"severity", "warning"},
                                                                          {"state", "active"}},
                                                              QJsonObject{{"id", 2},
                                                                          {"ts", double(now - 172800)},
                                                                          {"source", "demo"},
                                                                          {"severity", "warning"},
                                                                          {"state", "active"}}}}},
                                       "alarms"));
        w.alarmSource->setCurrentIndex(w.alarmSource->findData("demo"));
        QVERIFY(w.alarmTable->isRowHidden(0));
        QVERIFY(!w.alarmTable->isRowHidden(1));
        w.alarmTable->setCurrentCell(1, 0);
        w.alarmTime->setCurrentIndex(1);
        QVERIFY(w.alarmTable->isRowHidden(1));
        QCOMPARE(w.alarmTable->currentRow(), -1);
    }
    void bundledResourceIntegrity()
    {
        QFile manifest(":/hmi/fonts/manifest.json");
        QVERIFY(manifest.open(QIODevice::ReadOnly));
        const auto metadata = QJsonDocument::fromJson(manifest.readAll()).object();
        QCOMPARE(metadata["family"].toString(), QString("Dshan Sans"));
        QCOMPARE(metadata["fonts"].toArray().size(), 2);
        QString characters = "°℃›●▶▼…–—✓×·▣";
        const QString sourceDir = QFINDTESTDATA("../../components/hmi/src");
        QVERIFY(!sourceDir.isEmpty());
        for (const auto &name : QDir(sourceDir).entryList({"*.cpp"}, QDir::Files)) {
            QFile source(sourceDir + "/" + name);
            QVERIFY(source.open(QIODevice::ReadOnly));
            characters += QString::fromUtf8(source.readAll());
        }
        for (const auto &value : metadata["fonts"].toArray()) {
            const auto record = value.toObject();
            QFile file(":/hmi/fonts/" + record["output"].toString());
            QVERIFY(file.open(QIODevice::ReadOnly));
            const auto bytes = file.readAll();
            QRawFont raw(bytes, 16, QFont::PreferDefaultHinting);
            QVERIFY(raw.isValid());
            QCOMPARE(raw.familyName(), QString("Dshan Sans"));
            for (const auto character : characters) {
                if (character.isPrint())
                    QVERIFY2(raw.supportsCharacter(character), qPrintable(QString("%1 missing U+%2 (%3)")
                        .arg(record["output"].toString()).arg(character.unicode(), 4, 16, QChar('0')).arg(character)));
            }
            QCOMPARE(QString(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()),
                     record["sha256"].toString());
        }
        QFile license(":/hmi/fonts/OFL.txt");
        QVERIFY(license.open(QIODevice::ReadOnly));
        const auto licenseBytes = license.readAll();
        QVERIFY(licenseBytes.contains("SIL OPEN FONT LICENSE Version 1.1"));
        QCOMPARE(QString(QCryptographicHash::hash(licenseBytes, QCryptographicHash::Sha256).toHex()),
                 metadata["license_sha256"].toString());
        QVERIFY(!QPixmap(":/hmi/icons/up.xpm").isNull());
        QVERIFY(!QPixmap(":/hmi/icons/down.xpm").isNull());
    }
    void bundledFontsAndTimeSeries()
    {
        MainWindow w;
        isolate(w);
        QCOMPARE(QFontInfo(Visuals::font(14)).family(), QString("Dshan Sans"));
        QCOMPARE(QFontInfo(Visuals::font(14, true)).family(), QString("Dshan Sans"));
        auto data = snapshot();
        w.dashboard->setSnapshot(data);
        QCOMPARE(w.dashboard->pressures.size(), 1);
        QVERIFY(qAbs(w.dashboard->pressures.last().y() - .62) < .00001);
        auto readings = data["sensors"].toArray();
        auto pressure = readings[1].toObject();
        pressure["unit"] = "kPa";
        pressure["value"] = 624.;
        readings[1] = pressure;
        data["sensors"] = readings;
        w.dashboard->setSnapshot(data);
        QVERIFY(qAbs(w.dashboard->pressures.last().y() - .624) < .00001);
        w.dashboard->setSnapshot(data);
        QCOMPARE(w.dashboard->pressures.size(), 1);
        data["timestamp"] = data["timestamp"].toDouble() + 61;
        w.dashboard->setSnapshot(data);
        QCOMPARE(w.dashboard->pressures.size(), 1);
        data["sensors"] = QJsonArray();
        w.dashboard->setSnapshot(data);
        QVERIFY(w.dashboard->temperatures.isEmpty());
        QVERIFY(w.dashboard->pressures.isEmpty());
    }
    void exactTimeWindowAndClockRecovery()
    {
        MainWindow w;
        isolate(w);
        auto data = snapshot();
        const double base = data["timestamp"].toDouble();
        auto apply = [&](double offset, double temperature) {
            data["timestamp"] = base + offset;
            auto sensors = data["sensors"].toArray();
            auto sensor = sensors[0].toObject();
            sensor["value"] = temperature;
            sensors[0] = sensor;
            data["sensors"] = sensors;
            w.handleReply(new FixtureReply(data, "snapshot"));
        };
        apply(0, 10);
        apply(59.999, 20);
        apply(60, 30);
        for (auto *chart : {w.dashboard, w.deviceTrend}) {
            QCOMPARE(chart->temperatures.size(), 3);
            QCOMPARE(chart->temperatures.first(), QPointF(base, 10));
            QCOMPARE(chart->pressures.size(), 3);
        }
        apply(60, 40); // Exact duplicate replaces, including at the boundary.
        for (auto *chart : {w.dashboard, w.deviceTrend}) {
            QCOMPARE(chart->temperatures.size(), 3);
            QCOMPARE(chart->temperatures.last(), QPointF(base + 60, 40));
        }
        apply(60.001, 50); // Only the point outside [now - 60, now] expires.
        for (auto *chart : {w.dashboard, w.deviceTrend}) {
            QCOMPARE(chart->temperatures.size(), 3);
            QCOMPARE(chart->temperatures.first().x(), base + 59.999);
            QCOMPARE(chart->pressures.first().x(), base + 59.999);
        }
        apply(30, -5); // Backwards clock clears both old timelines.
        for (auto *chart : {w.dashboard, w.deviceTrend}) {
            QCOMPARE(chart->temperatures, QVector<QPointF>({QPointF(base + 30, -5)}));
            QCOMPARE(chart->pressures.size(), 1);
        }
        apply(31, 0);
        QCOMPARE(w.deviceTrend->temperatures.size(), 2);
        apply(1000, 15); // Long gap must not retain stale points.
        QCOMPARE(w.dashboard->temperatures.size(), 1);
        QCOMPARE(w.deviceTrend->temperatures.size(), 1);
        w.markOffline();
        apply(1001, 16);
        for (auto *chart : {w.dashboard, w.deviceTrend}) {
            QCOMPARE(chart->temperatures, QVector<QPointF>({QPointF(base + 1001, 16)}));
            QCOMPARE(chart->pressures.size(), 1);
        }
    }
    void resumeDoesNotJoinAcrossUnobservedDiscontinuity()
    {
        for (bool missing : {false, true}) {
            MainWindow w;
            isolate(w);
            auto data = snapshot();
            const double base = data["timestamp"].toDouble();
            w.updateSnapshot(data);
            w.pauseMonitor->setChecked(true);
            data["timestamp"] = base + 20;
            w.updateSnapshot(data);
            data["timestamp"] = base + 10; // Rollback stays ahead of frozen sample.
            if (missing)
                data["sensors"] = QJsonArray();
            w.updateSnapshot(data);
            data["timestamp"] = base + 11;
            data["sensors"] = snapshot()["sensors"];
            w.updateSnapshot(data);
            QCOMPARE(w.deviceTrend->temperatures.first().x(), base);
            QCOMPARE(w.deviceTrend->snapshot()["timestamp"].toDouble(), base);
            w.pauseMonitor->setChecked(false);
            QCOMPARE(w.deviceTrend->temperatures.size(), 1);
            QCOMPARE(w.deviceTrend->pressures.size(), 1);
            QCOMPARE(w.deviceTrend->temperatures.first().x(), base + 11);
            QCOMPARE(w.deviceTrend->snapshot(), w.latestSnapshot);
        }
    }
    void pauseKeepsPollingAndOtherViewsLive()
    {
        const bool live = qEnvironmentVariableIsSet("HMI_PAUSE_LIVE_HTTP");
        MainWindow w;
        if (live) {
            w.refreshTimer.stop();
            QTRY_VERIFY_WITH_TIMEOUT(!w.requestActive, 5000);
            w.networkManager.setProxy(QNetworkProxy(
                QNetworkProxy::HttpProxy, "127.0.0.1",
                qEnvironmentVariableIntValue("HMI_PAUSE_HTTP_PROXY_PORT")));
            w.refreshTimer.start();
            w.requestSnapshot();
        }
        if (!live)
            isolate(w);
        auto data = snapshot();
        auto deliver = [&](const QJsonObject &value) {
            w.handleReply(new FixtureReply(value, "snapshot"));
        };
        if (live) {
            QTRY_VERIFY_WITH_TIMEOUT(!w.latestSnapshot.isEmpty(), 5000);
        } else {
            deliver(data);
        }
        const auto frozen = w.deviceTrend->snapshot();
        const auto points = w.deviceTrend->temperatures;
        const QString reading = w.deviceTable->item(0, 1)->text();
        const QString metric = w.monitorStatus->values()[0].value;
        qRegisterMetaType<QNetworkReply *>();
        QSignalSpy finished(&w.networkManager, &QNetworkAccessManager::finished);
        w.pauseMonitor->setChecked(true);
        QVERIFY(w.refreshTimer.isActive() == live);
        if (live) {
            // The Flask fixture changes sampled temperature, inserts an alarm,
            // and records HTTP methods + SQLite telemetry on subsequent polls.
            QTRY_VERIFY_WITH_TIMEOUT(w.dashboard->snapshot()["system"].toObject()["temperature"].toDouble() >= 43, 6000);
            QVERIFY(finished.count() >= 2);
            QVERIFY(w.refreshTimer.isActive());
        } else {
            data["timestamp"] = data["timestamp"].toDouble() + 2;
            data["system"] = QJsonObject{{"cpu", 77}};
            data["summary"] = QJsonObject{{"status", "有告警"}};
            data["alarms"] = QJsonObject{{"active", 1}, {"recent", QJsonArray{
                QJsonObject{{"id", 99}, {"message", "暂停期间告警"}}}}};
            data["services"] = QJsonObject{{"thingsboard", QJsonObject{{"running", false}}}};
            auto networks = data["networks"].toArray();
            auto network = networks[0].toObject();
            network["address"] = "192.0.2.17";
            networks[0] = network;
            data["networks"] = networks;
            deliver(data);
            QCOMPARE(w.cpuValue->text().simplified(), QString("CPU 77%"));
            QCOMPARE(w.serviceStates[0]->text(), QString("● 未运行"));
            QCOMPARE(w.networkTables[0]->item(0, 2)->text(), QString("192.0.2.17"));
            QCOMPARE(w.dashboard->snapshot()["alarms"], data["alarms"]);
        }
        QCOMPARE(w.alarmSummaryLabel->text(), QString("● 系统有告警"));
        QCOMPARE(w.deviceTrend->snapshot(), frozen);
        QCOMPARE(w.deviceTrend->temperatures, points);
        QCOMPARE(w.deviceTable->item(0, 1)->text(), reading);
        QCOMPARE(w.monitorStatus->values()[0].value, metric);
        w.pauseMonitor->setChecked(false);
        QCOMPARE(w.deviceTrend->snapshot(), w.latestSnapshot);
        QCOMPARE(w.deviceTrend->temperatures.size(), 1);
        if (live) {
            w.pauseMonitor->setChecked(true);
            // Fixture returns an invalid timestamp after several successful polls.
            QTRY_VERIFY_WITH_TIMEOUT(w.latestSnapshot.isEmpty(), 6000);
            QVERIFY(w.deviceTrend->temperatures.isEmpty());
            QCOMPARE(w.deviceTable->rowCount(), 0);
            QVERIFY(w.refreshTimer.isActive());
            QTRY_VERIFY_WITH_TIMEOUT(!w.latestSnapshot.isEmpty(), 6000);
            QVERIFY(w.deviceTrend->temperatures.isEmpty());
            w.pauseMonitor->setChecked(false);
            QCOMPARE(w.deviceTrend->temperatures.size(), 1);
        }
    }
    void missingSummaryIsNotZero()
    {
        MainWindow w;
        isolate(w);
        auto data = snapshot();
        data["system"] = QJsonObject();
        data["summary"] = QJsonObject();
        data["alarms"] = QJsonObject();
        w.updateSnapshot(data);
        w.refreshTimer.stop();
        QCOMPARE(w.dashboard->summaryValues(), QStringList({"--", "--", "--", "--"}));
        QVERIFY(w.cpuValue->text().contains("--"));
        QVERIFY(w.memoryValue->text().contains("--"));
        QVERIFY(w.temperatureValue->text().contains("--"));
        QVERIFY(w.storageValue->text().contains("--"));
        // The backend serializes missing measurements as explicit JSON null.
        data["system"] = QJsonObject{{"cpu", QJsonValue()}, {"memory", QJsonValue()},
                                    {"temperature", QJsonValue()}, {"storage", QJsonValue()},
                                    {"uptime_seconds", QJsonValue()},
                                    {"temperature_source", "unavailable"}};
        w.updateSnapshot(data);
        QVERIFY(w.cpuValue->text().contains("--"));
        QVERIFY(w.memoryValue->text().contains("--"));
        QVERIFY(w.temperatureValue->text().contains("--"));
        QVERIFY(w.storageValue->text().contains("--"));
        QCOMPARE(w.dashboard->summaryValues(), QStringList({"--", "--", "--", "--"}));
        const QList<QJsonValue> invalid = {QJsonValue(), "0", true, -1, 1e100};
        for (const auto &value : invalid) {
            data["summary"] = QJsonObject{{"online_devices", value}};
            data["alarms"] = QJsonObject{{"active", value}};
            data["system"] = QJsonObject{{"uptime_seconds", value}};
            w.dashboard->setSnapshot(data);
            QCOMPARE(w.dashboard->summaryValues(), QStringList({"--", "--", "--", "--"}));
        }
        data["summary"] = QJsonObject{{"status", "正常"}, {"online_devices", 0}};
        data["alarms"] = QJsonObject{{"active", 0}};
        data["system"] = QJsonObject{{"uptime_seconds", 0}};
        w.dashboard->setSnapshot(data);
        QCOMPARE(w.dashboard->summaryValues(), QStringList({"正常", "0", "0", "00:00:00"}));
    }
    void unverifiedSummaryRemainsVisibleWhilePaused()
    {
        MainWindow w;
        isolate(w);
        w.resize(1024, 600);
        w.show();
        auto data = snapshot();
        data["system"] = QJsonObject{{"temperature_source", "unavailable"}};
        data["sensors"] = QJsonArray();
        data["buses"] = QJsonArray{QJsonObject{{"name", "CAN0"}, {"online", false}, {"state", "DOWN"}}};
        data["networks"] = QJsonArray();
        w.pauseMonitor->setChecked(true);
        for (const QString &status : {QString("未验证"), QString("有告警"), QString("演示"), QString("未验证")}) {
            data["summary"] = QJsonObject{{"status", status}, {"online_devices", 0}};
            data["alarms"] = QJsonObject{{"active", status == "有告警" ? 1 : 0}};
            w.handleReply(new FixtureReply(data, "snapshot"));
            w.refreshTimer.stop();
            QCOMPARE(w.dashboard->summaryValues()[0], status);
            QCOMPARE(w.alarmSummaryLabel->text(), "● 系统" + status);
            QVERIFY(!w.alarmSummaryLabel->styleSheet().contains("#168345"));
            QVERIFY(w.cpuValue->text().contains("--"));
            QVERIFY(w.dashboard->temperatures.isEmpty());
        }
        QTest::qWait(50);
        QDir().mkpath("screenshots");
        QVERIFY(w.grab().save("screenshots/1024x600-summary-unverified.png"));
        // Optional replay of snapshots captured from the real loopback HTTP
        // smoke test. The ordinary suite above remains self-contained.
        const QString replayPath = qEnvironmentVariable("HMI_SUMMARY_HTTP_REPLAY");
        if (!replayPath.isEmpty()) {
            QFile file(replayPath);
            QVERIFY(file.open(QIODevice::ReadOnly));
            const auto records = QJsonDocument::fromJson(file.readAll()).array();
            QVERIFY(!records.isEmpty());
            for (const auto &record : records) {
                const auto received = record.toObject();
                const auto expected = received["summary"].toObject()["status"].toString();
                QVERIFY(expected == "未验证" || expected == "有告警" || expected == "演示");
                w.handleReply(new FixtureReply(received, "snapshot"));
                w.refreshTimer.stop();
                QCOMPARE(w.dashboard->summaryValues()[0], expected);
                QCOMPARE(w.alarmSummaryLabel->text(), "● 系统" + expected);
                QVERIFY(!w.alarmSummaryLabel->styleSheet().contains("#168345"));
            }
        }
        w.markOffline();
        QCOMPARE(w.alarmSummaryLabel->text(), QString("● 数据服务离线"));
        QVERIFY(!w.dashboard->connected);
    }
    void sensorIdentityUnitsAndAvailability()
    {
        MainWindow w;
        isolate(w);
        auto data = snapshot();
        auto temperature = data["sensors"].toArray()[0].toObject();
        auto pressure = data["sensors"].toArray()[1].toObject();
        temperature["name"] = "温度设定值";
        data["sensors"] = QJsonArray{pressure, temperature};
        w.updateSnapshot(data);
        QCOMPARE(w.monitorStatus->values()[0].value, QString("--"));
        QVERIFY(w.dashboard->temperatures.isEmpty());
        QCOMPARE(w.monitorStatus->values()[1].value, QString("0.62 MPa"));
        temperature["name"] = "温度";
        temperature["unit"] = "°F";
        pressure["unit"] = "bar";
        data["sensors"] = QJsonArray{pressure, temperature};
        w.updateSnapshot(data);
        QVERIFY(w.dashboard->temperatures.isEmpty());
        QVERIFY(w.dashboard->pressures.isEmpty());
        QCOMPARE(w.monitorStatus->values()[0].value, QString("--"));
        QCOMPARE(w.monitorStatus->values()[1].value, QString("--"));
        temperature["unit"] = "°C";
        pressure["unit"] = "kPa";
        pressure["value"] = 620;
        data["sensors"] = QJsonArray{pressure, temperature};
        w.updateSnapshot(data);
        QCOMPARE(w.dashboard->temperatures.last().y(), 36.5);
        QCOMPARE(w.dashboard->pressures.last().y(), .62);
        QCOMPARE(w.deviceTrend->pressures.last().y(), .62);
        QCOMPARE(w.monitorStatus->values()[1].value, QString("0.62 MPa"));
        const QList<QJsonObject> invalid = {
            {{"value", QJsonValue()}}, {{"value", "36.5"}}, {{"value", true}},
            {{"value", std::numeric_limits<double>::infinity()}},
            {{"value", std::numeric_limits<double>::quiet_NaN()}},
            {{"source", "unavailable"}}, {{"online", false}}, {{"state", "离线"}},
            {{"state", "offline"}}, {{"state", "未连接"}}};
        for (const auto &patch : invalid) {
            auto sensor = temperature;
            for (auto it = patch.begin(); it != patch.end(); ++it)
                sensor[it.key()] = it.value();
            data["sensors"] = QJsonArray{sensor};
            w.updateSnapshot(data);
            QCOMPARE(w.deviceTable->item(0, 1)->text(), QString("--"));
            QCOMPARE(w.deviceTable->item(0, 3)->text(), QString("未获取"));
            QCOMPARE(w.monitorStatus->values()[0].value, QString("--"));
            QVERIFY(w.dashboard->temperatures.isEmpty());
            QVERIFY(w.deviceTrend->temperatures.isEmpty());
        }
        w.refreshTimer.stop();
        w.resize(1024, 600);
        w.show();
        QTest::mouseClick(w.navButtons[3], Qt::LeftButton);
        QTest::qWait(80);
        QDir().mkpath("screenshots");
        QVERIFY(w.grab().save("screenshots/1024x600-invalid-sensor.png"));
    }
    void invalidTimestampClearsPausedMonitor()
    {
        MainWindow w;
        isolate(w);
        const QList<QJsonValue> invalid = {QJsonValue(), "1791817200", true, -1, 1e100,
                                           std::numeric_limits<double>::infinity()};
        for (const auto &value : invalid) {
            w.pauseMonitor->setChecked(false);
            w.updateSnapshot(snapshot());
            QVERIFY(!w.deviceTrend->temperatures.isEmpty());
            w.pauseMonitor->setChecked(true);
            auto data = snapshot();
            data["timestamp"] = value;
            w.updateSnapshot(data);
            QVERIFY(!w.dashboard->online());
            QVERIFY(w.dashboard->temperatures.isEmpty());
            QVERIFY(w.deviceTrend->temperatures.isEmpty());
            QCOMPARE(w.deviceTable->rowCount(), 0);
            QCOMPARE(w.monitorStatus->values()[0].value, QString("--"));
        }
        w.refreshTimer.stop();
    }
    void filterCannotLeaveHiddenAlarmSelected()
    {
        MainWindow w;
        isolate(w);
        const QJsonObject active{{"id", 11}, {"state", "active"}, {"severity", "warning"}};
        const QJsonObject ack{{"id", 22}, {"state", "acknowledged"}, {"severity", "info"}};
        w.handleReply(new FixtureReply({{"ok", true}, {"alarms", QJsonArray{active, ack}}}, "alarms"));
        w.alarmTable->setCurrentCell(1, 0);
        w.alarmTabs->setCurrentIndex(1);
        QVERIFY(w.alarmTable->isRowHidden(1));
        QCOMPARE(w.alarmTable->currentRow(), -1);
        w.alarmSeverity->setCurrentIndex(3);
        QVERIFY(w.alarmTable->isRowHidden(0));
    }
    void historySelectionTracksAlarmId()
    {
        MainWindow w;
        isolate(w);
        const QJsonObject first{{"id", 11}, {"message", "first"}};
        const QJsonObject second{{"id", 22}, {"message", "second"}};
        w.handleReply(new FixtureReply({{"ok", true}, {"alarms", QJsonArray{first, second}}}, "alarms"));
        w.alarmTable->setCurrentCell(1, 0);
        w.updateSnapshot(snapshot());
        w.refreshTimer.stop();
        QCOMPARE(w.alarmTable->rowCount(), 2); // Recent snapshot must not replace history.
        w.handleReply(new FixtureReply({{"ok", true}, {"alarms", QJsonArray{second, first}}}, "alarms"));
        QCOMPARE(w.alarmTable->currentRow(), 0);
        QCOMPARE(w.alarmTable->item(0, 0)->text(), QString("22"));
        w.handleReply(new FixtureReply({{"ok", true}, {"alarms", QJsonArray{first}}}, "alarms"));
        QCOMPARE(w.alarmTable->currentRow(), -1);
    }
    void alarmDetailsFiltersAndFreshness()
    {
        MainWindow w; isolate(w); w.updateSnapshot(snapshot()); w.refreshTimer.stop();
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        QJsonObject a{{"id", 2147483655.0}, {"ts", double(now - 10)}, {"severity", "warning"},
                      {"source", "sensor-a"}, {"state", "active"}, {"message", "<b>温度异常</b>"}};
        auto b = a; b["id"] = 22; b["source"] = "sensor-b"; b["severity"] = "error"; b["message"] = "second";
        auto c = a; c["id"] = 23; c["state"] = "acknowledged"; c["severity"] = "info";
        auto future = a; future["id"] = 24; future["ts"] = double(now + 3600);
        auto old = a; old["id"] = 25; old["ts"] = double(now - 8 * 86400);
        auto invalid = a; invalid["id"] = 26; invalid["ts"] = "bad";
        auto day = a; day["id"] = 27; day["ts"] = double(now - 2 * 86400);
        auto load = [&](const QJsonArray &rows) {
            w.handleReply(new FixtureReply({{"ok", true}, {"alarms", rows}}, "alarms"));
        };
        load({a, b, c, future, old, invalid, day});
        w.alarmTable->setCurrentCell(0, 1);
        QCOMPARE(w.alarmTable->item(0, 0)->text(), QString("2147483655"));
        QVERIFY(w.alarmDetail->text().contains("ID 2147483655"));
        QVERIFY(w.alarmDetail->text().contains("<b>温度异常</b>"));
        QCOMPARE(w.alarmDetail->textFormat(), Qt::PlainText);
        QVERIFY(w.alarmAck->isEnabled());
        load({b, a, c, future, old, invalid, day});
        QCOMPARE(w.alarmTable->currentRow(), 1);
        QVERIFY(w.alarmDetail->text().contains("2147483655"));
        w.alarmSeverity->setCurrentIndex(2);
        QCOMPARE(w.alarmTable->currentRow(), -1);
        QCOMPARE(w.alarmDetail->text(), QString("选择一条告警查看详情"));
        QVERIFY(!w.alarmAck->isEnabled());
        w.alarmSeverity->setCurrentIndex(0);
        w.alarmSource->setCurrentIndex(w.alarmSource->findData("sensor-a"));
        QVERIFY(w.alarmTable->isRowHidden(0));
        w.alarmTime->setCurrentIndex(1);
        for (int row : {3, 4, 5, 6}) QVERIFY(w.alarmTable->isRowHidden(row));
        QVERIFY(!w.alarmTable->isRowHidden(1));
        w.alarmTime->setCurrentIndex(2); QVERIFY(!w.alarmTable->isRowHidden(6));
        w.alarmTabs->setCurrentIndex(1); QVERIFY(w.alarmTable->isRowHidden(2));
        w.alarmTabs->setCurrentIndex(2); QVERIFY(w.alarmTable->isRowHidden(1));
        w.alarmTable->setCurrentCell(2, 1); QVERIFY(!w.alarmAck->isEnabled());
        w.alarmTabs->setCurrentIndex(0); w.alarmTime->setCurrentIndex(0);
        load({b});
        QCOMPARE(w.alarmSource->currentData().toString(), QString("sensor-a"));
        QVERIFY(w.alarmTable->isRowHidden(0));
        QCOMPARE(w.alarmDetail->text(), QString("选择一条告警查看详情"));
        load({a}); w.alarmTable->setCurrentCell(0, 1); QVERIFY(w.alarmAck->isEnabled());
        w.markOffline(); QVERIFY(!w.alarmAck->isEnabled());
        load({a}); QVERIFY(!w.alarmAck->isEnabled());
        w.updateSnapshot(snapshot()); w.refreshTimer.stop(); QVERIFY(w.alarmAck->isEnabled());
        w.alarmTable->clearSelection(); QVERIFY(!w.alarmAck->isEnabled());
        for (const auto &id : {QJsonValue(), QJsonValue(1.5), QJsonValue("22"), QJsonValue(9007199254740992.0)}) {
            auto bad = a; bad["id"] = id; load({bad}); w.alarmTable->setCurrentCell(0, 1);
            QCOMPARE(w.alarmTable->item(0, 0)->text(), QString("--"));
            QVERIFY(!w.alarmAck->isEnabled());
        }
        w.resize(1024, 600); w.show(); w.showPage(4);
        load({a, b, c}); w.alarmTable->setCurrentCell(0, 1);
        QTest::qWait(30); QVERIFY(w.grab().save("screenshots/alarm-selected.png"));
        load({}); QTest::qWait(30); QVERIFY(w.grab().save("screenshots/alarm-empty.png"));
        w.markOffline(); QTest::qWait(30); QVERIFY(w.grab().save("screenshots/alarm-offline.png"));
    }
    void alarmCsvExportAndAuditSemantics()
    {
        MainWindow w; isolate(w);
        const QString message = "=1+2,\"温度\"\r\nnext";
        QJsonObject a{{"id", 2147483655.0}, {"ts", 100}, {"severity", "warning"},
                      {"source", "\t@source"}, {"state", "active"}, {"message", message}};
        auto hidden = a; hidden["id"] = 22; hidden["severity"] = "error"; hidden["message"] = "hidden-record";
        w.handleReply(new FixtureReply({{"ok", true}, {"alarms", QJsonArray{a, hidden}}}, "alarms"));
        w.alarmSeverity->setCurrentIndex(1);
        const QByteArray expected = w.visibleAlarmCsv();
        QVERIFY(expected.startsWith("\xef\xbb\xbf\"ID\","));
        QVERIFY(expected.contains("\"2147483655\""));
        QVERIFY(!expected.contains("hidden-record"));
        QVERIFY(expected.contains(QString("\"'=1+2,\"\"温度\"\"\r\nnext\"").toUtf8()));
        QVERIFY(expected.contains("\"'\t@source\""));
        for (const QString &prefix : {QString("+"), QString("-"), QString("@"), QString("  ="), QString("\r="), QString("\n=")}) {
            w.alarmTable->item(0, 4)->setText(prefix + "formula");
            QVERIFY(w.visibleAlarmCsv().contains(("\"'" + prefix + "formula\"").toUtf8()));
        }
        w.alarmTable->item(0, 4)->setText("   ");
        QVERIFY(w.visibleAlarmCsv().contains("\"   \""));
        w.alarmTable->item(0, 4)->setText(message);
        QTemporaryDir folder; QVERIFY(folder.isValid());
        const QString filename = folder.path() + "/visible.csv";
        auto *exportButton = w.findChild<QPushButton *>("alarmExport"); QVERIFY(exportButton);
        QStringList dialogs;
        QString selectedDestination;
        int dialogTicks = 0;
        QTimer dialogTimer;
        connect(&dialogTimer, &QTimer::timeout, &w, [&]() {
            if (auto *dialog = qobject_cast<QFileDialog *>(QApplication::activeModalWidget())) {
                if (++dialogTicks > 500) { dialog->reject(); return; }
                // QFileSystemModel loads asynchronously. Set the directory and
                // leaf name separately, then allow an event turn before accepting.
                if (!dialog->property("exportFixturePrepared").toBool()) {
                    dialog->setDirectory(folder.path());
                    dialog->selectFile("visible.csv");
                    dialog->setProperty("exportFixturePrepared", true);
                    return;
                }
                selectedDestination = dialog->selectedFiles().value(0);
                if (selectedDestination != filename) {
                    dialog->selectFile("visible.csv");
                    return;
                }
                dialogs << "save";
                // A timer refresh while choosing a filename must not change the export selection.
                w.handleReply(new FixtureReply({{"ok", true}, {"alarms", QJsonArray{hidden}}}, "alarms"));
                QMetaObject::invokeMethod(dialog, "accept");
            } else if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
                dialogs << box->windowTitle(); box->accept();
            }
        });
        dialogTimer.start(10); exportButton->click(); dialogTimer.stop();
        QCOMPARE(selectedDestination, filename);
        QVERIFY(dialogs.contains("导出完成"));
        QFile file(filename); QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), expected); file.close();
        const QString output = qEnvironmentVariable("HMI_ALARM_CSV_OUT");
        if (!output.isEmpty()) QVERIFY(QFile::copy(filename, output));
        // Cancel preserves an existing file; failed open is reported as failure.
        disconnect(&dialogTimer, nullptr, &w, nullptr); dialogs.clear();
        connect(&dialogTimer, &QTimer::timeout, &w, [&]() {
            if (auto *dialog = qobject_cast<QFileDialog *>(QApplication::activeModalWidget())) dialog->reject();
        });
        dialogTimer.start(10); exportButton->click(); dialogTimer.stop();
        QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), expected); file.close();
        disconnect(&dialogTimer, nullptr, &w, nullptr);
        connect(&dialogTimer, &QTimer::timeout, &w, [&]() {
            if (auto *dialog = qobject_cast<QFileDialog *>(QApplication::activeModalWidget())) {
                dialog->setOption(QFileDialog::DontConfirmOverwrite);
                dialog->setDirectory("/proc");
                dialog->selectFile("version");
                QMetaObject::invokeMethod(dialog, "accept");
            } else if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
                dialogs << box->windowTitle(); box->accept();
            }
        });
        dialogTimer.start(10); exportButton->click(); dialogTimer.stop();
        QCOMPARE(dialogs, QStringList{"导出失败"});
        QJsonArray audit;
        for (const auto &result : {"success", "failed", "pending", "unknown", ""})
            audit.append(QJsonObject{{"ts", 100}, {"action", "alarm_ack"}, {"result", result}, {"detail", "original detail"}});
        w.handleReply(new FixtureReply({{"ok", true}, {"audit", audit}}, "audit"));
        for (int filter = 0; filter < 3; ++filter) {
            w.auditResult->setCurrentIndex(filter);
            for (int row = 0; row < 5; ++row) QCOMPARE(w.auditTable->isRowHidden(row), filter != 0 && row != filter - 1);
        }
        QCOMPARE(w.auditTable->item(2, 3)->text(), QString("pending"));
        QCOMPARE(w.auditTable->item(2, 3)->toolTip(), QString("original detail"));
        QCOMPARE(w.auditTable->item(0, 1)->text(), QString("告警确认"));
        w.resize(1024, 600); w.show(); w.showPage(5);
        w.findChild<QTabWidget *>("settingsTabs")->setCurrentIndex(2);
        w.auditResult->setCurrentIndex(0); QTest::qWait(30);
        QVERIFY(w.grab().save("screenshots/audit-unknown-results.png"));
    }
    void alarmAckUsesSelectedIdAndRecoversFromFailure()
    {
        MainWindow w; isolate(w); w.updateSnapshot(snapshot()); w.refreshTimer.stop();
        QTcpServer server; QVERIFY(server.listen(QHostAddress::LocalHost));
        QStringList paths;
        connect(&server, &QTcpServer::newConnection, this, [&]() {
            auto *socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket]() {
                QByteArray bytes = socket->property("request").toByteArray() + socket->readAll();
                socket->setProperty("request", bytes);
                int split = bytes.indexOf("\r\n\r\n"); if (split < 0) return;
                int length = 0;
                for (const auto &line : bytes.left(split).split('\n'))
                    if (line.toLower().startsWith("content-length:")) length = line.mid(15).trimmed().toInt();
                if (bytes.size() < split + 4 + length || socket->property("handled").toBool()) return;
                socket->setProperty("handled", true);
                QVERIFY(bytes.startsWith("POST "));
                paths << QUrl(QString::fromUtf8(bytes.split(' ')[1])).path();
                const QByteArray body = "{\"ok\":false,\"error\":\"fixture failure\"}";
                socket->write("HTTP/1.1 409 Conflict\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: " +
                              QByteArray::number(body.size()) + "\r\n\r\n" + body);
                socket->disconnectFromHost();
            });
        });
        w.networkManager.setProxy(QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", server.serverPort()));
        connect(&w.networkManager, &QNetworkAccessManager::finished, &w, &MainWindow::handleReply);
        auto load = [&]() {
            w.handleReply(new FixtureReply({{"ok", true}, {"alarms", QJsonArray{
                QJsonObject{{"id", 22}, {"state", "active"}, {"severity", "warning"}},
                QJsonObject{{"id", 2147483655.0}, {"state", "active"}, {"severity", "error"}}}}}, "alarms"));
        };
        load(); w.alarmTable->setCurrentCell(1, 1);
        w.alarmSeverity->setCurrentIndex(1); w.alarmAck->click();
        QTest::qWait(30); QCOMPARE(paths.size(), 0);
        w.alarmSeverity->setCurrentIndex(0); w.alarmTable->setCurrentCell(1, 1);
        w.markOffline(); w.alarmAck->click(); QTest::qWait(30); QCOMPARE(paths.size(), 0);
        load(); QVERIFY(!w.alarmAck->isEnabled());
        w.updateSnapshot(snapshot()); w.refreshTimer.stop(); QVERIFY(w.alarmAck->isEnabled());
        QStringList dialogs;
        QTimer timer; connect(&timer, &QTimer::timeout, &w, [&]() {
            if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
                dialogs << box->windowTitle(); box->accept();
            }
        }); timer.start(10);
        w.alarmAck->click(); QVERIFY(!w.alarmAck->isEnabled()); w.alarmAck->click();
        QTRY_COMPARE(paths.size(), 1); QTRY_COMPARE(dialogs.size(), 1);
        QVERIFY(!w.alarmAckPending); QVERIFY(!w.alarmAck->isEnabled());
        load(); QVERIFY(w.alarmAck->isEnabled()); w.alarmAck->click();
        QTRY_COMPARE(paths.size(), 2); QTRY_COMPARE(dialogs.size(), 2); timer.stop();
        QCOMPARE(paths, (QStringList{"/api/hmi/v1/alarms/2147483655/ack", "/api/hmi/v1/alarms/2147483655/ack"}));
        QCOMPARE(dialogs, (QStringList{"操作失败", "操作失败"}));
        const QString output = qEnvironmentVariable("HMI_ALARM_REQUESTS_OUT");
        if (!output.isEmpty()) {
            QFile file(output); QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(QJsonDocument(QJsonArray::fromStringList(paths)).toJson());
        }
        load(); w.alarmTable->setCurrentCell(1, 1);
        QTimer::singleShot(0, &w, []() {
            if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) box->accept();
        });
        w.handleReply(new FixtureReply({{"ok", false}, {"error", "history unavailable"}}, "alarms"));
        QVERIFY(!w.alarmAck->isEnabled());
        load(); QVERIFY(w.alarmAck->isEnabled());
        const int port = qEnvironmentVariableIntValue("HMI_ALARM_HTTP_PORT");
        if (port > 0) {
            w.networkManager.setProxy(QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", port));
            w.alarmTable->setRowCount(0);
            w.sendJson("GET", "/alarms?limit=500", {}, "alarms");
            QTRY_COMPARE(w.alarmTable->rowCount(), 2);
            int target = -1;
            for (int row = 0; row < 2; ++row)
                if (w.alarmTable->item(row, 0)->text() == "2147483655") target = row;
            QVERIFY(target >= 0); w.alarmTable->setCurrentCell(target, 1);
            QVERIFY(w.alarmAck->isEnabled());
            dialogs.clear(); timer.start(10); w.alarmAck->click();
            QTRY_VERIFY(dialogs.contains("操作完成"));
            QTRY_COMPARE(w.alarmTable->item(w.alarmTable->currentRow(), 5)->data(Qt::UserRole).toString(), QString("acknowledged"));
            QVERIFY(!w.alarmAck->isEnabled());
            w.sendJson("GET", "/audit", {}, "audit");
            QTRY_COMPARE(w.auditTable->rowCount(), 1);
            QCOMPARE(w.auditTable->item(0, 2)->text(), QString("2147483655"));
            QCOMPARE(w.auditTable->item(0, 3)->data(Qt::UserRole).toString(), QString("success"));
            timer.stop(); w.refreshTimer.stop();
        }

    }
    void settingsAndServicesRequestsRemainTruthful()
    {
        MainWindow w; isolate(w); w.updateSnapshot(snapshot()); w.refreshTimer.stop();
        w.resize(1024, 600); w.show(); w.pages->setCurrentIndex(5);
        w.navButtons[0]->setChecked(false); w.navButtons[5]->setChecked(true);
        QTcpServer server; QVERIFY(server.listen(QHostAddress::LocalHost));
        QJsonArray requests;
        QStringList dialogs;
        int mode = 0;
        connect(&server, &QTcpServer::newConnection, this, [&]() {
            auto *socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket]() {
                QByteArray bytes = socket->property("request").toByteArray() + socket->readAll();
                socket->setProperty("request", bytes);
                const int split = bytes.indexOf("\r\n\r\n"); if (split < 0) return;
                int length = 0;
                for (const auto &line : bytes.left(split).split('\n'))
                    if (line.toLower().startsWith("content-length:")) length = line.mid(15).trimmed().toInt();
                if (bytes.size() < split + 4 + length || socket->property("handled").toBool()) return;
                socket->setProperty("handled", true);
                const QString method = QString::fromUtf8(bytes.split(' ')[0]);
                const QString path = QUrl(QString::fromUtf8(bytes.split(' ')[1])).path();
                const auto body = QJsonDocument::fromJson(bytes.mid(split + 4, length)).object();
                QJsonObject response{{"ok", true}};
                if (method == "GET") {
                    response = path.endsWith("/snapshot") ? snapshot() : QJsonObject{{"ok", true}, {"audit", QJsonArray()}};
                } else {
                    requests.append(QJsonObject{{"method", method}, {"path", path}, {"body", body}});
                    if (path.endsWith("/settings")) {
                        if (mode == 0) w.brightnessSlider->setValue(90); // edit while save is in flight
                        response["settings"] = body;
                        if (mode == 2) response = {{"ok", false}, {"error", "storage failure"}};
                    } else if (body["action"] == QJsonValue("brightness") && mode != 1) {
                        response["result"] = QJsonObject{{"brightness", body["params"].toObject()["value"]}};
                    } else response = {{"ok", false}, {"error", "device command failure"}};
                }
                const auto output = QJsonDocument(response).toJson();
                socket->write("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: " +
                              QByteArray::number(output.size()) + "\r\n\r\n" + output);
                socket->disconnectFromHost();
            });
        });
        w.networkManager.setProxy(QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", server.serverPort()));
        connect(&w.networkManager, &QNetworkAccessManager::finished, &w, &MainWindow::handleReply);
        int revoke = 0;
        QTimer timer; connect(&timer, &QTimer::timeout, &w, [&]() {
            w.refreshTimer.stop();
            if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
                if (box->standardButtons().testFlag(QMessageBox::Yes)) {
                    QCOMPARE(box->defaultButton(), box->button(QMessageBox::No));
                    if (revoke == 1) w.markOffline();
                    if (revoke == 2) { w.updateSnapshot(snapshot()); w.refreshTimer.stop(); }
                    box->button(QMessageBox::Yes)->click();
                } else { dialogs << box->text(); box->accept(); }
            }
        }); timer.start(10);
        auto *save = w.findChild<QPushButton *>("saveSettings");
        w.brightnessSlider->setValue(120); w.refreshCombo->setCurrentIndex(2);
        save->click(); QVERIFY(w.settingsPending); QVERIFY(!save->isEnabled()); save->click();
        QTRY_COMPARE(dialogs.size(), 1); QTRY_COMPARE(requests.size(), 2);
        QVERIFY(dialogs.last().contains("背光已应用"));
        QVERIFY(w.settingsDirty); QCOMPARE(w.brightnessSlider->value(), 90);
        w.updateSnapshot(snapshot()); w.refreshTimer.stop(); QCOMPARE(w.brightnessSlider->value(), 90);
        QCOMPARE(requests[0].toObject()["method"].toString(), QString("PUT"));
        QCOMPARE(requests[0].toObject()["body"].toObject()["brightness"].toInt(), 120);
        QVERIFY(!requests[0].toObject()["body"].toObject().contains("page_cycle_seconds"));
        QCOMPARE(requests[1].toObject()["body"].toObject()["params"].toObject()["value"].toInt(), 120);
        mode = 1; save->click(); QTRY_COMPARE(dialogs.size(), 2); QTRY_COMPARE(requests.size(), 4);
        QVERIFY(dialogs.last().contains("设置已保存，背光应用未确认")); QVERIFY(!w.settingsPending);
        mode = 2; w.brightnessSlider->setValue(130); save->click();
        QTRY_COMPARE(dialogs.size(), 3); QTRY_COMPARE(requests.size(), 5);
        QVERIFY(dialogs.last().contains("设置保存未确认")); QVERIFY(w.settingsDirty);
        QCOMPARE(w.brightnessSlider->value(), 130);
        revoke = 1; save->click(); QTest::qWait(40); QCOMPARE(requests.size(), 5); QVERIFY(!save->isEnabled());
        QVERIFY(w.grab().save("screenshots/settings-offline-edit.png"));
        auto ready = snapshot(); ready["services"] = QJsonObject{{"thingsboard", QJsonObject{{"running", false}, {"available", true}}}};
        auto *start = w.findChild<QPushButton *>("service-thingsboard-start");
        w.updateSnapshot(ready); w.refreshTimer.stop(); QVERIFY(start->isEnabled());
        start->click(); QTest::qWait(40); QCOMPARE(requests.size(), 5); QVERIFY(!start->isEnabled());
        w.updateSnapshot(ready); w.refreshTimer.stop(); revoke = 2;
        start->click(); QTest::qWait(40); QCOMPARE(requests.size(), 5); QVERIFY(!start->isEnabled());
        w.updateSnapshot(ready); w.refreshTimer.stop(); revoke = 0;
        start->click(); QTRY_COMPARE(requests.size(), 6); QTRY_COMPARE(dialogs.size(), 4);
        QVERIFY(dialogs.last().contains("device command failure"));
        QCOMPARE(requests.last().toObject()["body"].toObject()["params"].toObject()["service"].toString(), QString("thingsboard"));
        w.findChild<QTabWidget *>("settingsTabs")->setCurrentIndex(1);
        QTest::qWait(30); QVERIFY(w.grab().save("screenshots/services-available.png"));
        w.markOffline(); QTest::qWait(30); QVERIFY(w.grab().save("screenshots/services-offline.png"));
        for (auto *button : w.serviceButtons) QVERIFY(!button->isEnabled());
        QVERIFY(!w.rebootButton->isEnabled());
        for (auto *label : w.serviceStates) QVERIFY(label->text().contains("未获取"));
        const int port = qEnvironmentVariableIntValue("HMI_SETTINGS_HTTP_PORT");
        if (port > 0) {
            w.networkManager.setProxy(QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", port));
            w.updateSnapshot(snapshot()); w.refreshTimer.stop();
            w.brightnessSlider->setValue(140); w.demoCheck->setChecked(false);
            dialogs.clear(); save->click();
            QTRY_COMPARE(dialogs.size(), 1);
            QVERIFY(dialogs.last().contains("背光已应用"));
            QVERIFY(!w.settingsDirty); QVERIFY(!w.settingsPending);
            QTRY_COMPARE(w.brightnessSlider->value(), 140);
        }
        timer.stop();
        const auto output = qEnvironmentVariable("HMI_SETTINGS_REQUESTS_OUT");
        if (!output.isEmpty()) { QFile file(output); QVERIFY(file.open(QIODevice::WriteOnly)); file.write(QJsonDocument(requests).toJson()); }
    }

    void updatePanelIdentityAndProcessFailures()
    {
        QTemporaryDir temp; QVERIFY(temp.isValid());
        auto write = [&](const QString &name, const QByteArray &bytes) {
            QFile file(temp.path() + "/" + name); if (!file.open(QIODevice::WriteOnly)) return false;
            return file.write(bytes) == bytes.size();
        };
        QVERIFY(QDir().mkpath(temp.path() + "/apps/current"));
        const auto json = [](const QJsonObject &obj) { return QJsonDocument(obj).toJson(); };
        QVERIFY(write("config", json({{"application_root", temp.path() + "/apps"},
            {"system_contract", temp.path() + "/system"}, {"public_key", temp.path() + "/public"}})));
        QVERIFY(write("apps/current/installed.json", json({{"version", "1.0.0"}})));
        QVERIFY(write("system", json({{"board_id", "host-fixture"}, {"contract_id", "host-contract"}})));
        QVERIFY(write("public", "host fixture, not a production key"));
        // Real QProcess; test-only command/output files, never a runtime updater replacement.
        QVERIFY(write("updater", "#!/bin/sh\ncd \"$(dirname \"$0\")\"\ncat result\nexit $(cat exit-code)\n"));
        QVERIFY(QFile::setPermissions(temp.path() + "/updater", QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        QVERIFY(write("exit-code", "0"));
        MainWindow w; isolate(w); w.resize(1024,600); w.show(); w.pages->setCurrentIndex(5);
        w.navButtons[0]->setChecked(false); w.navButtons[5]->setChecked(true);
        auto *tabs = w.findChild<QTabWidget *>("settingsTabs"); tabs->setCurrentIndex(3);
        UpdatePanel *panel = nullptr;
        for (auto *widget : w.findChildren<QWidget *>())
            if ((panel = dynamic_cast<UpdatePanel *>(widget))) break;
        QVERIFY(panel);
        panel->configPath = temp.path() + "/config"; panel->updaterExecutable = temp.path() + "/updater";
        panel->reloadIdentity(); QVERIFY(panel->check->isEnabled());
        QCOMPARE(panel->version->text(), QString("1.0.0"));
        auto *install = panel->findChild<QPushButton *>("installUpdate"); QVERIFY(!install->isEnabled());
        const QJsonObject candidate{{"version", "1.2.3"}, {"sequence", 2147483655.0},
            {"manifest_url", "host-manifest"}, {"signature_url", "host-signature"}, {"package_url", "host-package"}};
        const QList<QByteArray> outputs{json(candidate), "null", "{\"version\":\"1.2.3\"}",
            "{\"version\":\"<b>forged</b>\"}", "invalid-json", json(candidate)};
        const QStringList expected{"版本 1.2.3", "无兼容更新", "检查结果无效", "检查结果无效", "检查结果无效", "检查失败"};
        for (int i = 0; i < outputs.size(); ++i) {
            QVERIFY(write("result", outputs[i])); QVERIFY(write("exit-code", i == 5 ? "1" : "0"));
            panel->check->click(); QVERIFY(!panel->check->isEnabled());
            QTRY_VERIFY(panel->check->isEnabled());
            QCOMPARE(panel->availableVersion->text(), expected[i]); QVERIFY(!install->isEnabled());
            if (i == 0) {
                QCOMPARE(panel->preflightValues.last()->text(), QString("清单已验证"));
                QVERIFY(panel->status->text().contains("维护窗口"));
            } else {
                QVERIFY(!panel->releaseSummary->text().contains("已找到"));
                for (auto *label : panel->preflightValues) {
                    QVERIFY(!label->text().contains("通过" ) || label->text().contains("未通过"));
                    QVERIFY(!label->styleSheet().contains("168345"));
                }
            }
            QTest::qWait(30);
            QVERIFY(w.grab().save(QString("screenshots/update-result-%1.png").arg(i)));
        }
        panel->process.start(temp.path() + "/missing");
        QTRY_COMPARE(panel->availableVersion->text(), QString("检查未完成"));
        QVERIFY(!install->isEnabled());
        QVERIFY(write("system", "{\"board_id\":\"\",\"contract_id\":\"\"}"));
        panel->check->click(); QVERIFY(!panel->check->isEnabled());
        QCOMPARE(panel->board->text(), QString("未配置独立应用更新"));
        for (auto *label : panel->preflightValues) QCOMPARE(label->text(), QString("未检查"));
        QVERIFY(write("system", json({{"board_id", "host-fixture"}, {"contract_id", "host-contract"}})));
        QVERIFY(write("apps/current/installed.json", "{\"version\":null}"));
        panel->reloadIdentity(); QVERIFY(!panel->check->isEnabled());
        QCOMPARE(panel->version->text(), QString("未提供应用版本记录"));
        QVERIFY(!panel->board->isVisible());
        QVERIFY(!panel->contract->isVisible());
        bool detailsVisible = false;
        QTimer::singleShot(0, &w, [&]() {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (box) {
                detailsVisible = box->textFormat() == Qt::PlainText &&
                    box->text().contains("host-fixture") && box->text().contains("host-contract");
                box->grab().save("screenshots/update-compatibility-details.png");
                QTest::mouseClick(box->button(QMessageBox::Ok), Qt::LeftButton);
            }
        });
        panel->findChild<QPushButton *>("updateCompatibilityDetails")->click();
        QVERIFY(detailsVisible);
        QTest::qWait(30); QVERIFY(w.grab().save("screenshots/update-missing-identity.png"));
    }

    void boardCapabilitiesFailClosed()
    {
        MainWindow w;
        isolate(w);
        auto data = snapshot();
        w.updateSnapshot(data);
        w.refreshTimer.stop();
        QCOMPARE(w.canInterfaces.first()->count(), 2);
        data["platform"] = QJsonObject{{"channels", QJsonObject()}, {"capabilities", QJsonArray()}};
        w.updateSnapshot(data);
        w.refreshTimer.stop();
        QCOMPARE(w.canInterfaces.first()->count(), 0);
        for (const auto &control : w.capabilityControls)
            QVERIFY(!control.first->isEnabled());
    }
    void serialEthercatValidateConfirmAndSend()
    {
        MainWindow w;
        isolate(w);
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        QList<QJsonObject> requests;
        connect(&server, &QTcpServer::newConnection, this, [&]() {
            auto *socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket]() {
                QByteArray bytes = socket->property("request").toByteArray() + socket->readAll();
                socket->setProperty("request", bytes);
                int split = bytes.indexOf("\r\n\r\n");
                if (split < 0) return;
                int length = 0;
                for (const auto &line : bytes.left(split).split('\n'))
                    if (line.toLower().startsWith("content-length:"))
                        length = line.mid(15).trimmed().toInt();
                if (bytes.size() < split + 4 + length) return;
                QVERIFY(bytes.startsWith("POST http://127.0.0.1/api/hmi/v1/actions "));
                requests << QJsonDocument::fromJson(bytes.mid(split + 4, length)).object();
                const QByteArray reply = "{\"ok\":true}";
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: " +
                              QByteArray::number(reply.size()) + "\r\n\r\n" + reply);
                socket->disconnectFromHost();
            });
        });
        w.networkManager.setProxy(QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", server.serverPort()));
        auto restore = [&]() { w.updateSnapshot(snapshot()); w.refreshTimer.stop(); };
        restore();
        auto *save = w.findChild<QPushButton *>("rsSave");
        auto *port = w.findChild<QLineEdit *>("rsPort");
        auto *baud = w.findChild<QLineEdit *>("rsBaud");
        auto *poll = w.findChild<QLineEdit *>("rsPoll");
        auto *write = w.findChild<QPushButton *>("ecWrite");
        auto *cycle = w.findChild<QPushButton *>("ecCycle");
        auto *index = w.findChild<QLineEdit *>("ecIndex");
        auto *value = w.findChild<QLineEdit *>("ecValue");
        auto clickDialog = [&](QPushButton *button, const QString &title, QMessageBox::StandardButton answer,
                               bool offline = false, bool revoke = false) {
            bool seen = false;
            QTimer::singleShot(0, &w, [&]() {
                auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
                if (!box) return;
                seen = true;
                QCOMPARE(box->windowTitle(), title);
                if (title == "操作确认")
                    QCOMPARE(box->defaultButton(), box->button(QMessageBox::No));
                if (offline) w.markOffline();
                if (revoke) {
                    auto data = snapshot(); data["platform"] = QJsonObject();
                    w.updateSnapshot(data); w.refreshTimer.stop();
                }
                box->button(answer)->click();
            });
            button->click();
            QVERIFY(seen);
        };
        QVERIFY(save->isEnabled()); QVERIFY(write->isEnabled()); QVERIFY(cycle->isEnabled());
        QCOMPARE(port->text(), QString("/dev/ttyAS5"));
        for (const auto &bad : {"bad", "299", "4000001", "1.5"}) {
            baud->setText(bad); clickDialog(save, "参数错误", QMessageBox::Ok);
        }
        baud->setText("4000000");
        for (const auto &bad : {"bad", "49", "3600001"}) {
            poll->setText(bad); clickDialog(save, "参数错误", QMessageBox::Ok);
        }
        poll->setText("3600000"); port->setText("/dev/ttyAS9");
        clickDialog(save, "参数错误", QMessageBox::Ok);
        port->setText("/dev/ttyAS5");
        for (const auto &bad : {"bad", "-1", "65536"}) {
            index->setText(bad); clickDialog(write, "参数错误", QMessageBox::Ok);
        }
        index->setText("0xffff");
        for (const auto &bad : {QString(), QString("f"), QString("GG"), QString("01;reboot"), QString("ff ").repeated(33)}) {
            value->setText(bad); clickDialog(write, "参数错误", QMessageBox::Ok);
        }
        value->setText("06 00");
        for (auto *button : {save, write, cycle}) {
            clickDialog(button, "操作确认", QMessageBox::No);
            clickDialog(button, "操作确认", QMessageBox::Yes, true);
            restore();
            clickDialog(button, "操作确认", QMessageBox::Yes, false, true);
            restore();
        }
        // Changing the master while the modal dialog is open cannot redirect the action.
        QTimer::singleShot(0, &w, [&]() {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            auto data = snapshot(); auto platform = data["platform"].toObject();
            auto channels = platform["channels"].toObject();
            auto ec = channels["ec"].toObject(); ec["device"] = "ethCHANGED";
            channels["ec"] = ec; platform["channels"] = channels; data["platform"] = platform;
            w.updateSnapshot(data); w.refreshTimer.stop();
            box->button(QMessageBox::Yes)->click();
        });
        cycle->click();
        QTest::qWait(50); QCOMPARE(requests.size(), 0); restore();
        // Refresh must preserve edits, but saving still validates the selected mapping.
        QCOMPARE(baud->text(), QString("4000000"));
        QCOMPARE(poll->text(), QString("3600000"));
        w.findChild<QSpinBox *>("rsUnit")->setValue(247);
        clickDialog(save, "操作确认", QMessageBox::Yes);
        QTRY_COMPARE(requests.size(), 1);
        QCOMPARE(requests[0]["action"].toString(), QString("rs485_config"));
        QCOMPARE(requests[0]["params"].toObject()["unit_id"].toInt(), 247);
        QCOMPARE(requests[0]["params"].toObject()["baudrate"].toInt(), 4000000);
        QCOMPARE(requests[0]["params"].toObject()["poll_ms"].toInt(), 3600000);
        w.findChild<QSpinBox *>("ecSlave")->setValue(65535);
        w.findChild<QSpinBox *>("ecSub")->setValue(255);
        clickDialog(write, "操作确认", QMessageBox::Yes);
        QTRY_COMPARE(requests.size(), 2);
        QCOMPARE(requests[1]["action"].toString(), QString("ethercat_sdo_write"));
        QCOMPARE(requests[1]["params"].toObject()["interface"].toString(), QString("ethTEST"));
        QCOMPARE(requests[1]["params"].toObject()["index"].toInt(), 65535);
        QCOMPARE(requests[1]["params"].toObject()["slave"].toInt(), 65535);
        QCOMPARE(requests[1]["params"].toObject()["subindex"].toInt(), 255);
        QCOMPARE(requests[1]["params"].toObject()["value"].toString(), QString("06 00"));
        clickDialog(cycle, "操作确认", QMessageBox::Yes);
        QTRY_COMPARE(requests.size(), 3);
        QCOMPARE(requests[2]["action"].toString(), QString("ethercat_cycle"));
        QCOMPARE(requests[2]["params"].toObject()["count"].toInt(), 1000);
        QCOMPARE(requests[2]["params"].toObject()["period_us"].toInt(), 1000);
        for (const auto &request : requests) QVERIFY(request["confirmed"].toBool());
        const QString output = qEnvironmentVariable("HMI_SERIAL_ETHERCAT_REQUESTS_OUT");
        if (!output.isEmpty()) {
            QJsonArray records;
            for (const auto &request : requests) records.append(request);
            QFile file(output); QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(QJsonDocument(records).toJson());
        }
    }
    void serialEthercatCapabilitiesAndEmptyStates()
    {
        MainWindow w; isolate(w);
        const auto buttons = {w.findChild<QPushButton *>("rsSave"), w.findChild<QPushButton *>("ecWrite"),
                              w.findChild<QPushButton *>("ecCycle")};
        for (auto *button : buttons) QVERIFY(!button->isEnabled());
        for (const auto &fault : {"capability", "missing", "empty", "blank", "type", "kind", "role"}) {
            auto data = snapshot(); auto platform = data["platform"].toObject();
            auto channels = platform["channels"].toObject();
            if (QString(fault) == "capability") platform["capabilities"] = QJsonArray();
            else for (const auto &name : {"serial0", "ec"}) {
                auto channel = channels[name].toObject();
                if (QString(fault) == "missing") { channels.remove(name); continue; }
                if (QString(fault) == "empty") channel["device"] = "";
                if (QString(fault) == "blank") channel["device"] = "  ";
                if (QString(fault) == "type") channel["device"] = 42;
                if (QString(fault) == "kind") channel["kind"] = "display";
                if (QString(fault) == "role") channel["role"] = "management";
                channels[name] = channel;
            }
            platform["channels"] = channels; data["platform"] = platform;
            w.updateSnapshot(data); w.refreshTimer.stop();
            for (auto *button : buttons) QVERIFY(!button->isEnabled());
            w.updateSnapshot(snapshot()); w.refreshTimer.stop();
            for (auto *button : buttons) QVERIFY(button->isEnabled());
        }
        auto *protocol = w.findChild<QTabWidget *>("protocolTabs");
        for (int tab : {1, 2}) {
            for (auto *table : protocol->widget(tab)->findChildren<QTableWidget *>())
                QCOMPARE(table->rowCount(), 0);
        }
        auto data = snapshot();
        data["platform"] = QJsonObject();
        data["buses"] = QJsonArray();
        data["summary"] = QJsonObject{{"status", "未验证"}};
        data["settings"] = QJsonObject{{"demo_mode", false}};
        w.updateSnapshot(data); w.refreshTimer.stop();
        w.resize(1024, 600); w.show(); w.showPage(1);
        for (int tab : {1, 2}) {
            protocol->setCurrentIndex(tab); QTest::qWait(30);
            QVERIFY(w.grab().save(QString("screenshots/serial-ethercat-unavailable-%1.png").arg(tab)));
        }
        w.markOffline();
        for (auto *button : buttons) QVERIFY(!button->isEnabled());
        for (int tab : {1, 2}) {
            protocol->setCurrentIndex(tab); QTest::qWait(30);
            QVERIFY(w.grab().save(QString("screenshots/serial-ethercat-offline-%1.png").arg(tab)));
        }
    }
    void cellularControlsValidateConfirmAndSend()
    {
        MainWindow w;
        isolate(w);
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        QList<QJsonObject> requests;
        connect(&server, &QTcpServer::newConnection, this, [&]() {
            auto *socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket]() {
                QByteArray bytes = socket->property("request").toByteArray() + socket->readAll();
                socket->setProperty("request", bytes);
                int split = bytes.indexOf("\r\n\r\n");
                if (split < 0) return;
                int length = 0;
                for (const auto &line : bytes.left(split).split('\n'))
                    if (line.toLower().startsWith("content-length:"))
                        length = line.mid(15).trimmed().toInt();
                if (bytes.size() < split + 4 + length) return;
                QVERIFY(bytes.startsWith("POST http://127.0.0.1/api/hmi/v1/actions "));
                requests << QJsonDocument::fromJson(bytes.mid(split + 4, length)).object();
                const QByteArray reply = "{\"ok\":true}";
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: " +
                              QByteArray::number(reply.size()) + "\r\n\r\n" + reply);
                socket->disconnectFromHost();
            });
        });
        w.networkManager.setProxy(QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", server.serverPort()));
        auto data = snapshot();
        auto platform = data["platform"].toObject();
        auto caps = platform["capabilities"].toArray(); caps.append("dual-cellular");
        platform["capabilities"] = caps;
        auto channels = platform["channels"].toObject();
        channels["cell"] = QJsonObject{{"kind", "cellular"}, {"role", "wan"}, {"device", "/dev/cdc-wdm0"}};
        platform["channels"] = channels; data["platform"] = platform;
        data["cellular"] = QJsonObject{{"ec20a", QJsonObject{{"device", "/dev/cdc-wdm0"}, {"interface", "ethCELL"}}}};
        auto networks = data["networks"].toArray();
        networks.append(QJsonObject{{"name", "ethCELL"}, {"kind", "cellular"}, {"state", "up"},
                                   {"online", true}, {"source", "real"}, {"address", "192.0.2.10/24"}});
        data["networks"] = networks;
        auto restore = [&]() { w.updateSnapshot(data); w.refreshTimer.stop(); };
        auto *start = w.findChild<QPushButton *>("ec20aConnect");
        auto *stop = w.findChild<QPushButton *>("ec20aDisconnect");
        QVERIFY(!start->isEnabled());
        restore();
        QVERIFY(start->isEnabled()); QVERIFY(stop->isEnabled());
        QVERIFY(!w.findChild<QPushButton *>("ec20bConnect")->isEnabled());
        QCOMPARE(w.networkTables[0]->rowCount(), 1);
        QCOMPARE(w.cellularInterfaces[0]->text(), QString("ethCELL"));
        QVERIFY(w.cellularAddresses[0]->text().contains("192.0.2.10/24"));
        QVERIFY(w.cellularStates[0]->text().contains("拨号状态未验证"));
        auto click = [&](QPushButton *button, QMessageBox::StandardButton answer, int change) {
            bool seen = false;
            QTimer::singleShot(0, &w, [&]() {
                auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
                if (!box) return;
                seen = true;
                QCOMPARE(box->windowTitle(), QString("操作确认"));
                QCOMPARE(box->defaultButton(), box->button(QMessageBox::No));
                if (change == 1) w.markOffline();
                if (change >= 2) {
                    auto revoked = data;
                    if (change == 2) revoked["platform"] = QJsonObject();
                    else revoked["cellular"] = QJsonObject{{"ec20a", QJsonObject{{"device", "/dev/cdc-wdm0"}, {"interface", "ethCHANGED"}}}};
                    w.updateSnapshot(revoked); w.refreshTimer.stop();
                }
                box->button(answer)->click();
            });
            button->click(); QVERIFY(seen);
        };
        click(start, QMessageBox::No, 0);
        for (int change = 1; change <= 3; ++change) {
            restore(); click(start, QMessageBox::Yes, change);
        }
        QTest::qWait(50); QCOMPARE(requests.size(), 0);
        for (const auto &bad : {QJsonObject(), QJsonObject{{"device", ""}, {"interface", "ethCELL"}},
                              QJsonObject{{"device", "/dev/unmapped"}, {"interface", "ethCELL"}}}) {
            auto invalid = data; invalid["cellular"] = QJsonObject{{"ec20a", bad}};
            w.updateSnapshot(invalid); w.refreshTimer.stop(); QVERIFY(!start->isEnabled());
        }
        restore(); click(start, QMessageBox::Yes, 0);
        QTRY_COMPARE(requests.size(), 1);
        click(stop, QMessageBox::Yes, 0); QTRY_COMPARE(requests.size(), 2);
        QCOMPARE(requests[0]["params"].toObject()["operation"].toString(), QString("connect"));
        QCOMPARE(requests[1]["params"].toObject()["operation"].toString(), QString("disconnect"));
        for (const auto &request : requests) {
            QVERIFY(request["confirmed"].toBool());
            QCOMPARE(request["action"].toString(), QString("ec20"));
            QCOMPARE(request["params"].toObject()["device"].toString(), QString("/dev/cdc-wdm0"));
            QCOMPARE(request["params"].toObject()["interface"].toString(), QString("ethCELL"));
        }
        const QString output = qEnvironmentVariable("HMI_CELLULAR_REQUESTS_OUT");
        if (!output.isEmpty()) {
            QJsonArray records; for (const auto &request : requests) records.append(request);
            QFile file(output); QVERIFY(file.open(QIODevice::WriteOnly)); file.write(QJsonDocument(records).toJson());
        }
        w.resize(1024, 600); w.show(); w.showPage(2);
        auto *tabs = w.findChild<QTabWidget *>("networkTabs"); tabs->setCurrentIndex(2);
        QTest::qWait(30); QVERIFY(w.grab().save("screenshots/network-cellular-mapped.png"));
        auto missing = data; missing["networks"] = QJsonArray();
        w.updateSnapshot(missing); w.refreshTimer.stop();
        QCOMPARE(w.cellularAddresses[0]->text(), QString("-- / 未获取"));
        QVERIFY(w.cellularStates[0]->text().contains("链路状态未获取"));
        w.markOffline(); QVERIFY(!start->isEnabled());
        QCOMPARE(w.cellularInterfaces[0]->text(), QString("--"));
        QCOMPARE(w.cellularStates[0]->text(), QString("数据服务离线"));
        for (int tab = 0; tab < 3; ++tab) {
            tabs->setCurrentIndex(tab); QTest::qWait(30);
            QVERIFY(w.grab().save(QString("screenshots/network-offline-%1.png").arg(tab)));
        }
        restore(); QVERIFY(start->isEnabled());
    }
    void canControlsValidateConfirmAndSend()
    {
        MainWindow w;
        isolate(w);
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        QList<QJsonObject> requests;
        connect(&server, &QTcpServer::newConnection, this, [&]() {
            auto *socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket]() {
                QByteArray bytes = socket->property("request").toByteArray() + socket->readAll();
                socket->setProperty("request", bytes);
                int split = bytes.indexOf("\r\n\r\n");
                if (split < 0) return;
                int length = 0;
                for (const auto &line : bytes.left(split).split('\n'))
                    if (line.toLower().startsWith("content-length:"))
                        length = line.mid(15).trimmed().toInt();
                if (bytes.size() < split + 4 + length) return;
                QVERIFY(bytes.startsWith("POST http://127.0.0.1/api/hmi/v1/actions "));
                requests << QJsonDocument::fromJson(bytes.mid(split + 4, length)).object();
                const QByteArray reply = "{\"ok\":true}";
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: " +
                              QByteArray::number(reply.size()) + "\r\n\r\n" + reply);
                socket->disconnectFromHost();
            });
        });
        w.networkManager.setProxy(QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", server.serverPort()));
        auto restore = [&]() { w.updateSnapshot(snapshot()); w.refreshTimer.stop(); };
        restore();
        auto *rate = w.findChild<QLineEdit *>("canBitrate");
        auto *dataRate = w.findChild<QLineEdit *>("canDataBitrate");
        auto *fd = w.findChild<QCheckBox *>("canFd");
        auto *apply = w.findChild<QPushButton *>("canApply");
        auto *nmt = w.findChild<QPushButton *>("canNmtSend");
        auto *sdo = w.findChild<QPushButton *>("canSdoWrite");
        auto *index = w.findChild<QLineEdit *>("canSdoIndex");
        auto *value = w.findChild<QLineEdit *>("canSdoValue");
        auto clickDialog = [&](QPushButton *button, const QString &title, QMessageBox::StandardButton answer,
                               bool offline = false, bool revoke = false) {
            bool seen = false;
            QTimer::singleShot(0, &w, [&]() {
                auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
                if (!box) return;
                seen = true;
                QCOMPARE(box->windowTitle(), title);
                if (title == "操作确认")
                    QCOMPARE(box->defaultButton(), box->button(QMessageBox::No));
                if (offline) w.markOffline();
                if (revoke) {
                    auto data = snapshot(); data["platform"] = QJsonObject();
                    w.updateSnapshot(data); w.refreshTimer.stop();
                }
                box->button(answer)->click();
            });
            button->click();
            QVERIFY(seen);
        };
        QVERIFY(!dataRate->isEnabled());
        rate->setText("bad");
        clickDialog(apply, "参数错误", QMessageBox::Ok);
        rate->setText("1000001");
        clickDialog(apply, "参数错误", QMessageBox::Ok);
        rate->setText("500000");
        fd->setChecked(true);
        QVERIFY(dataRate->isEnabled());
        dataRate->setText("499999");
        clickDialog(apply, "参数错误", QMessageBox::Ok);
        dataRate->setText("8000001");
        clickDialog(apply, "参数错误", QMessageBox::Ok);
        fd->setChecked(false);
        dataRate->setText("ignored while disabled");
        clickDialog(apply, "操作确认", QMessageBox::No);
        for (const auto &text : {"-1", "65536", "bad"}) {
            index->setText(text);
            clickDialog(sdo, "参数错误", QMessageBox::Ok);
        }
        index->setText("0xffff");
        for (const auto &text : {"-2147483649", "4294967296", "1.5"}) {
            value->setText(text);
            clickDialog(sdo, "参数错误", QMessageBox::Ok);
        }
        value->setText("0xffffffff");
        clickDialog(sdo, "操作确认", QMessageBox::No);
        clickDialog(nmt, "操作确认", QMessageBox::Yes, true);
        QVERIFY(!nmt->isEnabled());
        restore();
        clickDialog(nmt, "操作确认", QMessageBox::Yes, false, true);
        QTest::qWait(50);
        QCOMPARE(requests.size(), 0);
        restore();
        clickDialog(apply, "操作确认", QMessageBox::Yes);
        QTRY_COMPARE(requests.size(), 1);
        QCOMPARE(requests.last()["action"].toString(), QString("can_apply"));
        QCOMPARE(requests.last()["params"].toObject()["bitrate"].toInt(), 500000);
        QVERIFY(!requests.last()["params"].toObject()["fd"].toBool());
        QVERIFY(!requests.last()["params"].toObject().contains("data_bitrate"));
        fd->setChecked(true); dataRate->setText("8000000");
        clickDialog(apply, "操作确认", QMessageBox::Yes);
        QTRY_COMPARE(requests.size(), 2);
        QCOMPARE(requests.last()["params"].toObject()["data_bitrate"].toInt(), 8000000);
        w.canInterfaces[1]->setCurrentText("can1");
        w.findChild<QSpinBox *>("canNmtNode")->setValue(0);
        clickDialog(nmt, "操作确认", QMessageBox::Yes);
        QTRY_COMPARE(requests.size(), 3);
        QCOMPARE(requests.last()["action"].toString(), QString("canopen_nmt"));
        QCOMPARE(requests.last()["params"].toObject()["interface"].toString(), QString("can1"));
        QCOMPARE(requests.last()["params"].toObject()["node_id"].toInt(-1), 0);
        w.findChild<QSpinBox *>("canSdoNode")->setValue(127);
        w.findChild<QSpinBox *>("canSdoSubindex")->setValue(255);
        clickDialog(sdo, "操作确认", QMessageBox::Yes);
        QTRY_COMPARE(requests.size(), 4);
        QCOMPARE(requests.last()["action"].toString(), QString("canopen_sdo_write"));
        const auto params = requests.last()["params"].toObject();
        QCOMPARE(params["node_id"].toInt(), 127);
        QCOMPARE(params["index"].toInt(), 65535);
        QCOMPARE(params["subindex"].toInt(), 255);
        QCOMPARE(params["value"].toString(), QString("4294967295"));
        QCOMPARE(params["size"].toInt(), 4);
        for (const auto &request : requests) QVERIFY(request["confirmed"].toBool());
        const QString output = qEnvironmentVariable("HMI_CAN_REQUESTS_OUT");
        if (!output.isEmpty()) {
            QJsonArray records;
            for (const auto &request : requests) records.append(request);
            QFile file(output); QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(QJsonDocument(records).toJson());
        }
    }
    void canCapabilityRequiresNamedChannelAndRecovers()
    {
        MainWindow w;
        isolate(w);
        auto *apply = w.findChild<QPushButton *>("canApply");
        QVERIFY(!apply->isEnabled());
        for (const auto &device : {QJsonValue(), QJsonValue(""), QJsonValue("  "), QJsonValue(7)}) {
            auto data = snapshot();
            data["platform"] = QJsonObject{{"channels", QJsonObject{
                {"can0", QJsonObject{{"kind", "can"}, {"device", device}}}}}};
            w.updateSnapshot(data); w.refreshTimer.stop();
            QVERIFY(!apply->isEnabled());
            QCOMPARE(w.canInterfaces.first()->count(), 0);
        }
        w.updateSnapshot(snapshot()); w.refreshTimer.stop();
        QVERIFY(apply->isEnabled());
        w.canFdCheck->setChecked(true);
        auto noCapabilities = snapshot();
        auto platform = noCapabilities["platform"].toObject();
        platform.remove("capabilities"); noCapabilities["platform"] = platform;
        w.updateSnapshot(noCapabilities); w.refreshTimer.stop();
        QVERIFY(apply->isEnabled()); // Classic CAN remains available for a mapped channel.
        QVERIFY(!w.canFdCheck->isEnabled());
        QVERIFY(!w.canFdCheck->isChecked());
        QVERIFY(!w.findChild<QLineEdit *>("canDataBitrate")->isEnabled());
        QVERIFY(!w.findChild<QPushButton *>("canNmtSend")->isEnabled());
        QVERIFY(!w.findChild<QPushButton *>("canSdoWrite")->isEnabled());
        w.updateSnapshot(snapshot()); w.refreshTimer.stop();
        QVERIFY(w.canFdCheck->isEnabled());
        QVERIFY(w.findChild<QPushButton *>("canNmtSend")->isEnabled());
        w.canInterfaces.first()->setCurrentText("can1");
        w.updateSnapshot(snapshot()); w.refreshTimer.stop();
        QCOMPARE(w.canInterfaces.first()->currentText(), QString("can1"));
        w.markOffline();
        QVERIFY(!apply->isEnabled());
        w.updateSnapshot(snapshot()); w.refreshTimer.stop();
        QVERIFY(apply->isEnabled());
    }
    void editedSettingsSurviveRefresh()
    {
        MainWindow w;
        isolate(w);
        // Edits made before the first successful snapshot must survive as well.
        w.brightnessSlider->setValue(120);
        w.demoCheck->setChecked(false);
        w.refreshCombo->setCurrentIndex(3);
        w.updateSnapshot(snapshot());
        w.refreshTimer.stop();
        QCOMPARE(w.brightnessSlider->value(), 120);
        QVERIFY(!w.demoCheck->isChecked());
        QCOMPARE(w.refreshCombo->currentIndex(), 3);
        w.markOffline();
        w.updateSnapshot(snapshot()); w.refreshTimer.stop();
        QCOMPARE(w.brightnessSlider->value(), 120);
        QVERIFY(!w.demoCheck->isChecked());
        QCOMPARE(w.refreshCombo->currentIndex(), 3);
        w.settingsDirty = false;
        auto data = snapshot();
        auto settings = data["settings"].toObject(); settings["refresh_ms"] = 1500; data["settings"] = settings;
        w.updateSnapshot(data); w.refreshTimer.stop();
        QCOMPARE(w.refreshCombo->currentData().toInt(), 1500);
        QCOMPARE(w.refreshCombo->currentText(), QString("1.5 秒"));
    }
    void unavailableAndOfflineClearTelemetry()
    {
        MainWindow w;
        isolate(w);
        auto data = snapshot();
        w.updateSnapshot(data);
        w.refreshTimer.stop();
        QVERIFY(w.dashboard->online());
        QCOMPARE(w.dashboard->snapshot()["buses"].toArray().size(), 4);
        data["sensors"] = QJsonArray();
        data["networks"] = QJsonArray();
        w.updateSnapshot(data);
        w.refreshTimer.stop();
        QVERIFY(w.dashboard->snapshot()["sensors"].toArray().isEmpty());
        QVERIFY(w.dashboard->snapshot()["networks"].toArray().isEmpty());
        w.markOffline();
        QCOMPARE(w.deviceTable->rowCount(), 0);
        QCOMPARE(w.busTable->rowCount(), 0);
        QVERIFY(!w.dashboard->online());
        QVERIFY(w.connectionLabel->text().contains("离线"));
        w.resize(1024, 600);
        w.show();
        QTest::qWait(60);
        QVERIFY(w.grab().save("screenshots/1024x600-offline.png"));
    }
};
QTEST_MAIN(HmiUiTest)
#include "hmi_ui_test.moc"
