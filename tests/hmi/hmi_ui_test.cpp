#include <QScopedPointer>
#include "../../components/hmi/src/mainwindow.h"
#include "../../components/hmi/src/visuals.h"
#include "../../components/hmi/src/updatepanel.h"
#include "../../components/hmi/src/applicationheartbeat.h"
#include "../../components/hmi/src/desktopclient.h"
#include "../../components/hmi/src/cloudpanel.h"
#include "../../components/hmi/src/familypanel.h"
#include "../../components/hmi/src/mediapanel.h"
#include "../../components/hmi/src/desktop/desktopcanvas.h"
#include "../../components/hmi/src/desktop/desktoptrendpanel.h"
#include "../../components/hmi/src/desktop/desktopshell.h"
#include "../../components/hmi/src/displayprobe.h"
#include <QLocalServer>
#include <QLocalSocket>
#include <unistd.h>
#include <QSaveFile>
#include <QScreen>
#include <QWindow>
#include <time.h>
#include <QCheckBox>
#include <QFileDialog>
#include <QToolButton>
#include <QTreeView>
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
    QTemporaryDir isolatedState;
    QTcpServer isolatedProxy;
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
        w.desktopClient->poll.stop();
        w.desktopClient->disconnect();
        w.refreshTimer.stop();
        w.networkManager.disconnect(&w);
        for (auto *r : w.networkManager.findChildren<QNetworkReply *>())
            r->abort();
    }
  private slots:
    void applicationFrameworkRejectsContextBeforeNavigation()
    {
        MainWindow w;
        isolate(w);
        QVERIFY(w.openApplication("overview"));
        const auto before = w.pages->currentIndex();
        const QList<QPair<QString, QJsonObject>> requests = {
            {"devices", {{"point_id", 12}}},
            {"devices", {{"point_id", "bad point"}}},
            {"home", {{"device_id", "target\n"}}},
            {"video", {{"path", "/media/../unsafe.mp4"}}},
            {"settings", {{"action", "system_reboot"}}},
            {"not-registered", {}}
        };
        for (const auto &request : requests) {
            QVERIFY(!w.openApplication(request.first, request.second));
            QCOMPARE(w.pages->currentIndex(), before);
        }
    }
    void applicationFrameworkPublishesOnlySuccessfulNavigationState()
    {
        DesktopV1::DesktopShell shell;
        auto *application = shell.findChild<MainWindow *>();
        QVERIFY(application);
        isolate(*application);
        QSignalSpy changed(&shell, &DesktopV1::DesktopShell::activeApplicationChanged);
        QVERIFY(shell.activeApplication().isEmpty());
        QVERIFY(shell.openApplication("overview"));
        QCOMPARE(shell.activeApplication(), QString("overview"));
        QCOMPARE(changed.size(), 1);
        QVERIFY(!shell.openApplication("home", {{"device_id", false}}));
        QCOMPARE(shell.activeApplication(), QString("overview"));
        QCOMPARE(changed.size(), 1);
        QVERIFY(shell.openApplication("overview"));
        QCOMPARE(changed.size(), 1);
        shell.canvas()->change(2);
        QCOMPARE(shell.activeApplication(), QString("overview"));
        shell.backToDesktop();
        QVERIFY(shell.activeApplication().isEmpty());
        QCOMPARE(changed.size(), 2);
        QVERIFY(changed.last()[0].toString().isEmpty());
        shell.backToDesktop();
        QCOMPARE(changed.size(), 2);
    }
    void mediaWorkerFailuresAreBoundedAndDoNotRestart()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto script = [&](const QString &name, const QByteArray &body) {
            const auto path = dir.filePath(name);
            QFile file(path);
            if (!file.open(QIODevice::WriteOnly)) return QString();
            file.write("#!/usr/bin/python3\n" + body);
            file.close();
            file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
            return path;
        };
        MediaSession session;
        session.executable = script("bad.py", "import time\nprint('{\"protocol\":1,\"event\":\"status\",\"available\":\"true\",\"state\":\"playing\"}', flush=True)\ntime.sleep(20)\n");
        session.start();
        QTRY_COMPARE(session.snapshot()["error"].toString(), QString("worker-invalid-status"));
        QTRY_COMPARE(session.process->state(), QProcess::NotRunning);
        session.start();
        QCOMPARE(session.process->state(), QProcess::NotRunning);
        session.executable = script("silent.py", "import time\ntime.sleep(20)\n");
        session.startupDeadline.setInterval(80);
        session.start(true);
        QTRY_COMPARE(session.snapshot()["error"].toString(), QString("worker-start-timeout"));
        QTRY_COMPARE(session.process->state(), QProcess::NotRunning);
        QVERIFY(session.queue.isEmpty());
    }
    void mediaResizeCannotOvertakeQueuedOpen()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = dir.filePath("delayed.py");
        const auto log = dir.filePath("commands.jsonl");
        QFile script(path);
        QVERIFY(script.open(QIODevice::WriteOnly));
        script.write("#!/usr/bin/python3\nimport json,sys,time\ntime.sleep(.35)\ns={'protocol':1,'event':'status','available':True,'capabilities':['suspend-during-prepare','visibility-state'],'state':'idle'}\nprint(json.dumps(s),flush=True)\nwidth=0\nfor line in sys.stdin:\n r=json.loads(line)\n with open('" + log.toUtf8() + "','a') as f: f.write(line)\n if r['command']=='rect': width=r['params']['width']\n elif r['command']=='open':\n  s['state']='prepared' if width==400 else 'error'\n  print(json.dumps(s),flush=True)\n elif r['command']=='close': break\n");
        script.close();
        QVERIFY(script.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        MediaSession session;
        session.executable = path;
        session.start();
        auto rectangle = [&](int width) {
            session.command("rect", {{"x", 0}, {"y", 0}, {"width", width}, {"height", 200}});
        };
        rectangle(400);
        session.command("open", {{"path", "/test.avi"}, {"volume", 60}});
        // An asynchronous layout pass during worker startup must not move the
        // prerequisite rectangle behind open; only trailing resizes may merge.
        rectangle(500);
        rectangle(600);
        QTRY_COMPARE(session.snapshot()["state"].toString(), QString("prepared"));
        session.process->closeWriteChannel();
        QTRY_COMPARE(session.process->state(), QProcess::NotRunning);
        QFile commands(log);
        QVERIFY(commands.open(QIODevice::ReadOnly));
        const auto lines = commands.readAll().trimmed().split('\n');
        QCOMPARE(lines.size(), 3);
        const auto first = QJsonDocument::fromJson(lines[0]).object();
        QCOMPARE(first["command"].toString(), QString("rect"));
        QCOMPARE(first["params"].toObject()["width"].toInt(), 400);
        QCOMPARE(QJsonDocument::fromJson(lines[1]).object()["command"].toString(), QString("open"));
        QCOMPARE(QJsonDocument::fromJson(lines[2]).object()["params"].toObject()["width"].toInt(), 600);
    }
    void mediaExitNeverWaitsForBlockedDecoder()
    {
        QTemporaryDir dir;
        const auto path = dir.filePath("blocked.py");
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("#!/usr/bin/python3\nimport signal,time\nsignal.signal(signal.SIGTERM, signal.SIG_IGN)\nprint('{\"protocol\":1,\"event\":\"status\",\"available\":true,\"capabilities\":[\"suspend-during-prepare\",\"visibility-state\"],\"state\":\"idle\"}',flush=True)\ntime.sleep(30)\n");
        f.close();
        QVERIFY(f.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        QScopedPointer<MediaSession> session(new MediaSession);
        session->executable = path;
        session->start();
        QTRY_VERIFY2(session->snapshot()["available"] == true,
                     qPrintable(QString("media startup did not become ready: snapshot=%1 process_state=%2 error=%3 pid=%4")
                                    .arg(QString::fromUtf8(QJsonDocument(session->snapshot()).toJson(QJsonDocument::Compact)))
                                    .arg(int(session->process->state()))
                                    .arg(session->process->errorString())
                                    .arg(session->process->processId())));
        QPointer<QProcess> worker(session->process);
        int ticks = 0;
        QTimer heartbeat;
        connect(&heartbeat, &QTimer::timeout, this, [&ticks]() { ++ticks; });
        heartbeat.start(20);
        QElapsedTimer elapsed;
        elapsed.start();
        session.reset();
        QVERIFY2(elapsed.elapsed() < 150, "GUI destructor waited for media child");
        QTRY_VERIFY_WITH_TIMEOUT(worker.isNull(), 4500);
        QVERIFY(ticks >= 40);
    }
    void mediaHidePreservesPositionAndCancelsPendingAutostart()
    {
        QTemporaryDir dir;
        const auto path = dir.filePath("worker.py");
        const auto log = dir.filePath("commands.jsonl");
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("#!/usr/bin/python3\nimport json,sys\ns={'protocol':1,'event':'status','available':True,'capabilities':['suspend-during-prepare','visibility-state'],'state':'playing','first_frame':True,'position_ms':12345,'duration_ms':60000,'visible':True,'width':640,'height':360}\nprint(json.dumps(s),flush=True)\nfor line in sys.stdin:\n r=json.loads(line)\n with open('" + log.toUtf8() + "','a') as f: f.write(line)\n if r['command']=='suspend': s.update(state='paused',visible=False)\n elif r['command']=='visible': s['visible']=r['params']['value']\n elif r['command']=='close': break\n else: continue\n print(json.dumps(s),flush=True)\n");
        f.close();
        QVERIFY(f.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        MediaPanel panel;
        panel.session.executable = path;
        panel.resize(1024, 489);
        panel.show();
        QTRY_VERIFY(panel.state["first_frame"] == true);
        panel.hide();
        QTRY_COMPARE(panel.state["state"].toString(), QString("paused"));
        QCOMPARE(panel.state["position_ms"].toInt(), 12345);
        QVERIFY(panel.state["visible"] == false);
        panel.show();
        QTRY_VERIFY(panel.state["visible"] == true);
        QCOMPARE(panel.state["state"].toString(), QString("paused"));
        QCOMPARE(panel.state["position_ms"].toInt(), 12345);
        QFile commands(log);
        QVERIFY(commands.open(QIODevice::ReadOnly));
        const auto before = commands.readAll();
        QVERIFY(before.contains("suspend"));
        QVERIFY(!before.contains("close"));
        QVERIFY(!before.contains("\"command\":\"open\""));
        commands.close();
        panel.apply({{"available", true}, {"state", "preparing"}, {"first_frame", false}});
        panel.hide();
        QTRY_COMPARE(panel.state["state"].toString(), QString("paused"));
        QVERIFY(commands.open(QIODevice::ReadOnly));
        QVERIFY(commands.readAll().count("suspend") >= 2);
    }

    void mediaViewportExposesOnlyActiveCedarRectangle()
    {
        MediaPanel panel;
        panel.session.executable = "/missing-test-worker";
        panel.resize(1024, 489);
        panel.show();
        QTest::qWait(30);
        QJsonObject data{{"available", true}, {"backend", "cedar-tplayer"},
                         {"state", "playing"}, {"first_frame", true}, {"visible", true},
                         {"width", 640}, {"height", 360}};
        panel.apply(data);
        panel.geometry();
        panel.geometryTimer->stop();
        auto image = [&]() {
            QImage result(panel.stage->size(), QImage::Format_ARGB32_Premultiplied);
            result.fill(QColor(12, 34, 56));
            panel.stage->render(&result);
            return result.convertToFormat(QImage::Format_ARGB32);
        };
        auto frame = image();
        QCOMPARE(qAlpha(frame.pixel(frame.width()/2, frame.height()/2)), 0);
        QVERIFY(qAlpha(frame.pixel(0, 0)) == 255);
        for (const auto &name : {"paused", "error", "completed", "idle"})
        {
            data["state"] = name;
            panel.apply(data);
            frame = image();
            for (int y=0; y<frame.height(); ++y)
                for (int x=0; x<frame.width(); ++x)
                    QVERIFY(qAlpha(frame.pixel(x,y)) == 255);
        }
        data["state"] = "playing";
        data["visible"] = false;
        panel.apply(data);
        frame = image();
        QCOMPARE(qAlpha(frame.pixel(frame.width()/2,frame.height()/2)),255);
        data["visible"] = true;
        data["first_frame"] = false;
        panel.apply(data);
        frame = image();
        QCOMPARE(qAlpha(frame.pixel(frame.width()/2,frame.height()/2)),255);
    }

    void componentRoutesReuseExistingPagesAndHideLegacyNavigation()
    {
        MainWindow w;
        isolate(w);
        w.setDesktopEmbedded(true);
        w.show();
        QTest::qWait(20);
        QVERIFY(!w.nav->isVisible());
        const QStringList ids = {"overview", "fieldbus", "network", "devices", "alarms",
            "settings", "cloud_bind", "reporting", "voice", "home", "video", "update"};
        const QList<int> indices = {0,1,2,3,4,5,6,6,7,7,8,5};
        for (int i = 0; i < ids.size(); ++i) {
            QVERIFY(w.openApplication(ids[i]));
            QCOMPARE(w.pages->currentIndex(), indices[i]);
            QVERIFY(!w.nav->isVisible());
        }
        QCOMPARE(w.findChild<QTabWidget *>("settingsTabs")->currentIndex(), 3);
        QCOMPARE(w.findChildren<QWidget *>("applicationUpdatePanel").size(), 1);
        QVERIFY(!w.openApplication("unregistered-app"));
        w.openApplication("reporting");
        QCOMPARE(w.findChild<SectionTabs *>("cloudTabs")->currentIndex(), 1);
        w.openApplication("cloud_bind");
        QCOMPARE(w.findChild<SectionTabs *>("cloudTabs")->currentIndex(), 0);
        w.openApplication("home");
        QCOMPARE(w.findChild<SectionTabs *>("familyTabs")->currentIndex(), 1);
        w.openApplication("voice");
        QCOMPARE(w.findChild<SectionTabs *>("familyTabs")->currentIndex(), 0);
        w.setDesktopEmbedded(false);
        QVERIFY(w.nav->isVisible());
    }

    void embeddedSettingsReportPhysicalScreenInsteadOfContentArea()
    {
        DesktopV1::DesktopShell shell;
        auto *application = shell.findChild<MainWindow *>();
        QVERIFY(application);
        isolate(*application);
        shell.show();
        QVERIFY(shell.openApplication("settings"));
        QTest::qWait(30);
        auto *screen = shell.windowHandle()->screen();
        QVERIFY(screen);
        for (const auto &size : {QSize(1024, 600), QSize(1280, 720)}) {
            shell.resize(size);
            QTest::qWait(30);
            QVERIFY(application->height() < shell.height());
            const QSize pixels = screen->size() * screen->devicePixelRatio();
            QCOMPARE(application->screenResolution->text(),
                     QString("%1 × %2（已检测）").arg(pixels.width()).arg(pixels.height()));
        }
    }

    void componentDesktopProjectsLiveStateAndClearsLostData()
    {
        DesktopV1::DesktopCanvas canvas(false);
        auto root = snapshot();
        root["settings"] = QJsonObject{{"brightness", 220}, {"demo_mode", false}};
        root["networks"] = QJsonArray{QJsonObject{{"kind", "wifi"}, {"source", "real"},
            {"online", true}, {"address", "192.168.1.68/24"}}};
        root["alarms"] = QJsonObject{{"active", 0}, {"recent", QJsonArray()}};
        canvas.applyHmi(root);
        QVERIFY(canvas.model.networkOnline);
        QVERIFY(canvas.model.wirelessOnline);
        QVERIFY(!canvas.model.wiredOnline);
        QCOMPARE(canvas.model.wireless, QString("192.168.1.68/24"));
        QCOMPARE(canvas.model.wired, QString("未提供接口"));
        QCOMPARE(canvas.model.alarmCount, QString("0"));
        QCOMPARE(canvas.model.alarmText, QString("暂无活动告警"));
        QCOMPARE(canvas.model.temperature, QString("--")); // SoC temperature is not an environment binding.
        root["alarms"] = QJsonObject{{"active", 1}, {"recent", QJsonArray{
            QJsonObject{{"state", "active"}, {"message", "not a current real alarm"}, {"source", "demo"}},
            QJsonObject{{"state", "active"}, {"message", "实际故障"}, {"source", "real"}, {"ts", 1791817100}}}}};
        canvas.applyHmi(root);
        QCOMPARE(canvas.model.alarmText, QString("实际故障"));
        root["alarms"] = QJsonObject{{"active", 1.5}};
        canvas.applyHmi(root);
        QCOMPARE(canvas.model.alarmCount, QString("--"));
        root["networks"] = QJsonArray{QJsonObject{{"kind", "wifi"}, {"source", "demo"}, {"online", true}}};
        canvas.applyHmi(root);
        QVERIFY(!canvas.model.networkOnline);
        QCOMPARE(canvas.model.wireless, QString("演示数据"));
        canvas.applyHmi({});
        QVERIFY(!canvas.model.online);
        QVERIFY(!canvas.model.networkOnline);
        QCOMPARE(canvas.model.alarmCount, QString("--"));
        QCOMPARE(canvas.model.busStates, QStringList({"未获取", "未获取", "未获取"}));
        QCOMPARE(canvas.brightness, -1);
    }

    void displayPatternRetainsExactColorsAndPixelGeometry()
    {
        const auto image = DisplayProbe::reference();
        QCOMPARE(image.size(), QSize(1024, 600));
        QCOMPARE(image.pixelColor(30, 80), QColor(Qt::red));
        QCOMPARE(image.pixelColor(150, 80), QColor(Qt::green));
        QCOMPARE(image.pixelColor(275, 80), QColor(Qt::blue));
        QCOMPARE(image.pixelColor(30, 194), QColor("#142C4B"));
        QCOMPARE(image.pixelColor(190, 194), QColor("#1468FA"));
        QCOMPARE(image.pixelColor(25, 280), QColor(Qt::black));
        QCOMPARE(image.pixelColor(960, 280), QColor(Qt::white));
        QCOMPARE(image.pixelColor(0, 0), QColor(Qt::black));
        QCOMPARE(image.pixelColor(1023, 599), QColor(Qt::black));
    }

    void componentDesktopKeepsProviderAvailabilitySeparateFromCloud()
    {
        DesktopV1::DesktopCanvas canvas(false);
        canvas.applyServices({{"service_connected", true}, {"reporting", QJsonObject{{"enabled", true}}}});
        QCOMPARE(canvas.model.cloudState, QString("设备尚未登记"));
        QVERIFY(!canvas.model.cloudOnline);
        canvas.applyServices({{"service_connected", true},
            {"cloud", QJsonObject{{"configured", true}, {"mqtt", "offline"}}},
            {"reporting", QJsonObject{{"enabled", true}}}});
        QCOMPARE(canvas.model.cloudState, QString("上报已启用 · 云端离线"));
        canvas.applyServices({{"service_connected", true},
            {"cloud", QJsonObject{{"configured", true}, {"mqtt", "connected"}}},
            {"reporting", QJsonObject{{"enabled", true}}}});
        QVERIFY(canvas.model.cloudOnline);
        canvas.applyServices({});
        QVERIFY(!canvas.model.cloudOnline);
        QCOMPARE(canvas.model.cloudState, QString("接入服务未连接"));
        DesktopV1::DesktopCanvas fixture(true);
        fixture.applyHmi({});
        fixture.applyServices({});
        QCOMPARE(fixture.model.connection, QString("网络已连接"));
        QCOMPARE(fixture.model.alarmCount, QString("02"));
    }

    void desktopClientSerializesVoiceReleaseAndNeverReplaysAControl()
    {
        QTemporaryDir temp; QVERIFY(temp.isValid());
        QLocalServer server; QVERIFY(server.listen(temp.filePath("desktop.sock")));
        QList<QJsonObject> requests;
        connect(&server,&QLocalServer::newConnection,this,[&](){
            auto *socket=server.nextPendingConnection();
            connect(socket,&QLocalSocket::readyRead,socket,[&,socket](){
                auto bytes=socket->property("received").toByteArray()+socket->readAll();socket->setProperty("received",bytes);
                if(!bytes.contains('\n')||socket->property("finished").toBool())return;
                socket->setProperty("finished",true);const auto request=QJsonDocument::fromJson(bytes).object();requests.append(request);
                if(request["method"]=="home.command"){socket->abort();return;}
                const QJsonObject reply{{"protocol",1},{"ok",true},{"result",QJsonObject{{"version",1}}}};
                socket->write(QJsonDocument(reply).toJson(QJsonDocument::Compact)+'\n');socket->disconnectFromServer();
            });connect(socket,&QLocalSocket::disconnected,socket,&QObject::deleteLater);
        });
        DesktopClient client;client.poll.stop();client.socketPath=temp.filePath("desktop.sock");client.serverUid=geteuid();
        client.request("voice.command",{{"command","start_listen"}});
        client.request("voice.command",{{"command","stop_listen"}});
        QTRY_VERIFY(requests.size()>=2);
        QCOMPARE(requests[0]["params"].toObject()["command"].toString(),QString("start_listen"));
        QCOMPARE(requests[1]["params"].toObject()["command"].toString(),QString("stop_listen"));
        client.request("home.command",{{"command_id",QString(32,'a')}});
        auto count=[&](){int n=0;for(const auto &request:requests)if(request["method"]=="home.command")++n;return n;};
        QTRY_COMPARE(count(),1);QTest::qWait(100);QCOMPARE(count(),1);
    }
    void voiceUiCancellationStopsSession_data()
    {
        QTest::addColumn<int>("reason");
        const char *names[] = {"release", "button-touch-cancel", "panel-touch-cancel",
                               "window-deactivate", "application-deactivate", "hide", "tab-switch",
                               "free-touch-cancel", "free-window-deactivate", "free-hide", "free-tab-switch"};
        for (int i=0; i<11; ++i) QTest::newRow(names[i]) << i;
    }
    void voiceUiCancellationStopsSession()
    {
        QFETCH(int, reason);
        QTemporaryDir temporary; QLocalServer server;
        QVERIFY(server.listen(temporary.filePath("voice-ui.sock")));
        QStringList commands;
        connect(&server, &QLocalServer::newConnection, this, [&]() {
            auto *socket=server.nextPendingConnection();
            connect(socket,&QLocalSocket::readyRead,socket,[&,socket]() {
                auto bytes=socket->property("bytes").toByteArray()+socket->readAll();
                socket->setProperty("bytes",bytes); if(!bytes.contains('\n')) return;
                const auto request=QJsonDocument::fromJson(bytes.trimmed()).object();
                if(request["method"]=="voice.command") commands << request["params"].toObject()["command"].toString();
                socket->write(QJsonDocument(QJsonObject{{"protocol",1},{"ok",true},{"result",QJsonObject{{"voice",QJsonObject{{"configured",true},{"ready",true},{"state","idle"}}}}}}).toJson(QJsonDocument::Compact)+'\n');
                socket->disconnectFromServer();
            });
            connect(socket,&QLocalSocket::disconnected,socket,&QObject::deleteLater);
        });
        DesktopClient client; client.poll.stop(); client.socketPath=temporary.filePath("voice-ui.sock");client.serverUid=geteuid();
        FamilyPanel panel(&client); panel.resize(1024,600); panel.show();
        panel.apply({{"service_connected",true},{"voice",QJsonObject{{"configured",true},{"ready",true},{"state","idle"}}}});
        auto *talk=panel.findChild<QPushButton *>("voiceTalk");QVERIFY(talk);QVERIFY(talk->isEnabled());QVERIFY(talk->isVisible());
        QTest::qWait(30);
        const bool freeChat = reason >= 7;
        const QString startCommand = freeChat ? "enter_free_chat" : "start_listen";
        if (freeChat) {
            auto *mode=panel.findChild<QComboBox *>("voiceMode");QVERIFY(mode);
            mode->setCurrentIndex(1);QVERIFY(QMetaObject::invokeMethod(mode,"activated",Q_ARG(int,1)));
            const int triggers[] = {2,3,5,6};reason=triggers[reason-7];
        } else QTest::mousePress(talk,Qt::LeftButton);
        QTRY_COMPARE(commands.count(startCommand),1);
        if(reason==0) QTest::mouseRelease(talk,Qt::LeftButton);
        else if(reason==1) { QEvent e(QEvent::TouchCancel);QApplication::sendEvent(talk,&e); }
        else if(reason==2) { QEvent e(QEvent::TouchCancel);QApplication::sendEvent(&panel,&e); }
        else if(reason==3) { QEvent e(QEvent::WindowDeactivate);QApplication::sendEvent(&panel,&e); }
        else if(reason==4) { QEvent e(QEvent::ApplicationDeactivate);QApplication::sendEvent(qApp,&e); }
        else if(reason==5) panel.hide();
        else panel.selectSection(1);
        QTRY_VERIFY_WITH_TIMEOUT(commands.contains("stop_listen"),1500);
        QCOMPARE(commands.count(startCommand),1);
        // A delayed release after cancellation must not enqueue another stop or restart.
        QTest::mouseRelease(talk,Qt::LeftButton);QTest::qWait(80);
        QCOMPARE(commands.count("stop_listen"),1);
        QCOMPARE(commands.count(startCommand),1);
        QVERIFY(!talk->isDown());
        if(freeChat) QCOMPARE(commands.count("exit_free_chat"),1);
    }
    void newDesktopPagesHaveTruthfulEmptyStatesAndFitTheActualWindow()
    {
        MainWindow w;isolate(w);w.markOffline();w.show();QTest::qWait(30);
        QCOMPARE(w.pages->count(),9);
        for(const auto size:{QSize(1024,600),QSize(800,480),QSize(1280,720)}){
            w.resize(size);QTest::qWait(40);QCOMPARE(w.size(),size);
            for(int page=6;page<=8;++page){w.showPage(page);QTest::qWait(50);
                auto *outer=qobject_cast<QScrollArea *>(w.pages->currentWidget());QVERIFY(outer);
                QVERIFY2(outer->horizontalScrollBar()->maximum()==0,qPrintable(QString("new desktop page horizontal overflow %1/%2").arg(page).arg(size.width())));
                if(size.width()>=1024)QVERIFY2(outer->verticalScrollBar()->maximum()==0,qPrintable(QString("new desktop page vertical overflow %1: %2").arg(page).arg(outer->verticalScrollBar()->maximum())));
                const auto sections=outer->findChildren<QTabWidget *>();
                if(sections.isEmpty())QVERIFY(w.grab().save(QString("screenshots/desktop-%1x%2-page%3.png").arg(size.width()).arg(size.height()).arg(page)));
                for(auto *tabs:sections)for(int tab=0;tab<tabs->count();++tab){tabs->setCurrentIndex(tab);QTest::qWait(40);
                    QVERIFY(w.grab().save(QString("screenshots/desktop-%1x%2-page%3-tab%4.png").arg(size.width()).arg(size.height()).arg(page).arg(tab)));
                    for(auto *b:tabs->currentWidget()->findChildren<QPushButton *>())if(b->isVisible())QVERIFY(b->height()>=44);
                }
            }
        }
        QVERIFY(!w.cloudPanel->findChild<QCheckBox *>("reportingEnabled")->isEnabled());
        QVERIFY(!w.familyPanel->findChild<QPushButton *>("voiceTalk")->isEnabled());
        QVERIFY(w.cloudPanel->findChild<QLabel *>("pairingCode")->text().contains("—"));
    }
    void householdUiPreservesReportedValueWhileCommandIsPending()
    {
        MainWindow w;isolate(w);w.resize(1024,600);w.show();w.showPage(7);w.updateSnapshot(snapshot());w.refreshTimer.stop();
        QJsonObject state{{"version",1},{"service_connected",true},{"home",QJsonObject{
            {"configured",true},{"authorization","authorized"},{"fresh",true},{"busy",false},{"revision",1},
            {"devices",QJsonArray{QJsonObject{{"id","lamp"},{"name","书房台灯"},{"online",true},{"room","书房"},
                {"status",QJsonArray{QJsonObject{{"code","switch"},{"value",false}}}}}}},
            {"functions",QJsonObject{{"lamp",QJsonArray{QJsonObject{{"code","switch"},{"type","Boolean"},{"writable",true},{"values",QJsonObject()}}}}}},
            {"pending",QJsonObject{{"lamp/switch",QJsonObject{{"state","awaiting_confirmation"},{"value",true}}}}}}}};
        w.familyPanel->apply(state);auto *tabs=w.familyPanel->findChild<QTabWidget *>("familyTabs");tabs->setCurrentIndex(1);
        auto *devices=w.familyPanel->findChild<QTableWidget *>("homeDevices");devices->setCurrentCell(0,0);QTest::qWait(30);w.familyPanel->apply(state);
        auto toggles=w.familyPanel->findChildren<QCheckBox *>();
        QCheckBox *control=nullptr;for(auto *toggle:toggles)if(toggle->property("code")=="switch")control=toggle;
        QVERIFY(control);QVERIFY(!control->isChecked());QVERIFY(!control->isEnabled());
        QVERIFY(w.grab().save("screenshots/desktop-home-command-pending-fixture.png"));
    }
    void functionalHeartbeatRequiresBackendIdentityAndLiveGuiLoop()
    {
        QTemporaryDir temp; QVERIFY(temp.isValid());
        QTcpServer server; QVERIFY(server.listen(QHostAddress::LocalHost));
        const QString commit(40, 'a');
        QJsonObject body{{"ok",true},{"service","omnigate-web"},{"application_commit",QString(40,'b')}};
        connect(&server,&QTcpServer::newConnection,this,[&] {
            auto *socket=server.nextPendingConnection();
            connect(socket,&QTcpSocket::readyRead,socket,[&,socket] {
                auto request=socket->property("request").toByteArray()+socket->readAll();
                socket->setProperty("request",request);
                if(!request.contains("\r\n\r\n") || socket->property("responded").toBool()) return;
                socket->setProperty("responded",true);
                auto bytes=QJsonDocument(body).toJson(QJsonDocument::Compact);
                socket->write("HTTP/1.1 200 OK\r\nContent-Length: "+QByteArray::number(bytes.size())+
                              "\r\nConnection: close\r\n\r\n"+bytes);
                socket->disconnectFromHost();
            });
            connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
        });
        const auto url=QUrl(QString("http://127.0.0.1:%1/api/health").arg(server.serverPort()));
        const auto file=temp.filePath("heartbeat.json");
        ApplicationHeartbeat writer(file,commit,url);
        QTest::qWait(1200); QVERIFY(!QFile::exists(file));
        body["application_commit"]=commit;
        QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(file),3000);
        auto read=[&] {QFile value(file); if(!value.open(QIODevice::ReadOnly))return QByteArray();return value.readAll();};
        auto before=read();auto value=QJsonDocument::fromJson(before).object();
        QCOMPARE(value["commit"].toString(),commit);
        QCOMPARE(qint64(value["pid"].toDouble()),QCoreApplication::applicationPid());
        QVERIFY(value["gui_event_loop"].toBool());QVERIFY(value["backend_identity_verified"].toBool());
        QVERIFY(!(QFileInfo(file).permissions() & (QFile::WriteGroup|QFile::WriteOther)));
        // A live PID with a blocked GUI thread must stop publishing functional health.
        QTest::qSleep(6500);
        QCOMPARE(read(),before);
        struct timespec now;QVERIFY(::clock_gettime(CLOCK_BOOTTIME,&now)==0);
        QVERIFY(double(now.tv_sec)+now.tv_nsec/1e9-value["monotonic_seconds"].toDouble()>=6);
        const auto outside=temp.filePath("must-preserve");QFile target(outside);QVERIFY(target.open(QIODevice::WriteOnly));
        target.write("preserve this file");target.close();
        const auto linked=temp.filePath("linked.json");QVERIFY(QFile::link(outside,linked));
        ApplicationHeartbeat unsafe(linked,commit,url);QTest::qWait(1200);
        QVERIFY(target.open(QIODevice::ReadOnly));QCOMPARE(target.readAll(),QByteArray("preserve this file"));
    }
    void initTestCase()
    {
        // The same binary can run on a board with a live production service.
        // Unmocked HTTP requests terminate at this rejecting local test proxy;
        // per-case fixture proxies still override it. Never forward a request.
        QVERIFY(isolatedState.isValid());
        QVERIFY(isolatedProxy.listen(QHostAddress::LocalHost));
        connect(&isolatedProxy, &QTcpServer::newConnection, this, [this]() {
            while (isolatedProxy.hasPendingConnections()) {
                auto *socket = isolatedProxy.nextPendingConnection();
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                connect(socket, &QTcpSocket::readyRead, socket, [socket]() {
                    const auto bytes = socket->property("headers").toByteArray() + socket->readAll();
                    if (bytes.size() > 65536) { socket->abort(); return; }
                    socket->setProperty("headers", bytes);
                    if (!bytes.contains("\r\n\r\n") || socket->property("rejected").toBool()) return;
                    socket->setProperty("rejected", true);
                    socket->write("HTTP/1.1 503 Test Isolation\r\nConnection: close\r\n"
                                  "X-Dshan-Test-Isolation: 1\r\nContent-Length: 0\r\n\r\n");
                    socket->disconnectFromHost();
                });
            }
        });
        QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,
            "127.0.0.1", isolatedProxy.serverPort()));
        QNetworkAccessManager gate;
        gate.setProxy(QNetworkProxy::applicationProxy());
        auto *probe = gate.get(QNetworkRequest(QUrl("http://127.0.0.1/api/health")));
        QTRY_VERIFY(probe->isFinished());
        // A failed init aborts all cases before any control-action test runs.
        QVERIFY2(probe->rawHeader("X-Dshan-Test-Isolation") == QByteArray("1"), qPrintable(probe->errorString()));
        QCOMPARE(probe->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 503);
        qputenv("DSHANCONTROL_DESKTOP_STATE_DIR", isolatedState.filePath("desktop").toUtf8());
        qputenv("DSHANCONTROL_DESKTOP_SOCKET", isolatedState.filePath("absent-desktop.sock").toUtf8());
        qputenv("DSHANCONTROL_NATIVE_PLATFORM_SOCKET", QByteArray());
        qputenv("DSHANCONTROL_UPDATER_CONFIG", isolatedState.filePath("absent-updater.json").toUtf8());
        qputenv("DSHANCONTROL_UPDATER_EXECUTABLE", isolatedState.filePath("absent-runtime").toUtf8());
        qputenv("DSHANCONTROL_DESKTOP_ANIMATIONS","0");
        QString fontError;
        QVERIFY2(DesktopV1::DesktopCanvas::loadFonts(&fontError), qPrintable(fontError));
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
        QVERIFY(write("updater", "#!/bin/sh\ncd \"$(dirname \"$0\")\"\nprintf '%s\\n' \"$@\" > request\nif [ \"$2\" = update.identity ]; then cat identity; else cat result; fi\n"));
        QVERIFY(QFile::setPermissions(temp.path() + "/updater", QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        QVERIFY(write("updater.json", "{\"task_service\":true}"));
        QVERIFY(write("identity", "{\"system\":{\"board_id\":\"fixture-board\",\"contract_id\":\"fixture-contract\"},\"installed\":{\"version\":\"0.1.0-dev\"},\"check_configured\":true}"));
        QVERIFY(write("result", "null"));
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
        QTRY_VERIFY(panel.nativeIdentityReady);
        QTRY_COMPARE(panel.process.state(), QProcess::NotRunning);
        const QString checked(32, 'a'), downloaded(32, 'b'), failed(32, 'c');
        const QJsonObject candidate{{"version", "1.2.3"}, {"package_sha256", QString(64, 'd')}, {"development_release", true}};
        auto response = [&](const QString &id, const QString &operation, const QString &state) {
            return QJsonObject{{"task_id", id}, {"operation", operation}, {"status", state},
                               {"stage", "completed"}, {"candidate", candidate}};
        };
        QVERIFY(write("result", json(response(checked, "check", "success"))));
        panel.requestNative("update.status", {});
        QTRY_VERIFY(panel.download->isEnabled());
        QVERIFY(panel.availableVersion->text().contains("开发草稿"));
        QVERIFY(panel.releaseSummary->text().contains("开发验证"));
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

    void nativeIdentityUsesPublicRpcAndNeverNeedsPrivateConfiguration()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        auto write = [&](const QString &name, const QByteArray &bytes) {
            QFile file(temp.path() + "/" + name);
            return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
        };
        QVERIFY(write("updater", "#!/bin/sh\ncd \"$(dirname \"$0\")\"\nprintf '%s\\n' \"$2\" >> requests\nif [ \"$2\" = update.identity ]; then cat identity; else cat result; fi\n"));
        QVERIFY(QFile::setPermissions(temp.path() + "/updater", QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        const QJsonObject system{{"board_id", "public-board"}, {"contract_id", "public-contract"}};
        QJsonObject identity{{"system", system}, {"installed", QJsonObject{{"version", "1.2.3"}}},
                             {"check_configured", false}};
        QVERIFY(write("identity", QJsonDocument(identity).toJson()));
        QVERIFY(write("result", "null"));
        const QList<QByteArray> names{"DSHANCONTROL_NATIVE_PLATFORM_SOCKET", "DSHANCONTROL_UPDATER_EXECUTABLE", "DSHANCONTROL_UPDATER_CONFIG"};
        QList<QByteArray> values;
        QList<bool> present;
        for (const auto &name : names) { values << qgetenv(name); present << qEnvironmentVariableIsSet(name); }
        qputenv(names[0], "/public-fixture/platform.sock");
        qputenv(names[1], (temp.path() + "/updater").toLocal8Bit());
        qputenv(names[2], (temp.path() + "/missing-private-configuration").toLocal8Bit());
        UpdatePanel panel;
        for (int i = 0; i < names.size(); ++i) {
            if (present[i]) qputenv(names[i], values[i]); else qunsetenv(names[i]);
        }
        QTRY_COMPARE(panel.process.state(), QProcess::NotRunning);
        QCOMPARE(panel.version->text(), QString("1.2.3"));
        QVERIFY(panel.nativeMode && !panel.nativeIdentityReady);
        QCOMPARE(panel.process.processEnvironment().value("DSHANCONTROL_PLATFORM_SOCKET"), QString("/public-fixture/platform.sock"));
        QVERIFY(!panel.download->isEnabled() && !panel.install->isEnabled());
        QVERIFY(panel.status->text().contains("尚未就绪"));
        identity["check_configured"] = "true"; // A string is not a capability.
        QVERIFY(write("identity", QJsonDocument(identity).toJson()));
        panel.checkUpdates();
        QTRY_COMPARE(panel.process.state(), QProcess::NotRunning);
        QVERIFY(!panel.nativeIdentityReady);
        identity["check_configured"] = true;
        QVERIFY(write("identity", QJsonDocument(identity).toJson()));
        panel.checkUpdates();
        QTRY_VERIFY(panel.nativeIdentityReady);
        QTRY_COMPARE(panel.process.state(), QProcess::NotRunning);
        QVERIFY(panel.check->isEnabled());
        QVERIFY(!QFileInfo::exists(panel.configPath));
        const QJsonObject completed{{"task_id", QString(32, '9')}, {"operation", "install"},
                                    {"status", "success"}, {"stage", "completed"}};
        QVERIFY(write("result", QJsonDocument(completed).toJson()));
        panel.requestNative("update.status", {});
        QTRY_VERIFY(panel.nativeMethod == "update.identity" && panel.process.state() == QProcess::NotRunning);
        QTest::qWait(200);
        QFile requests(temp.path() + "/requests");
        QVERIFY(requests.open(QIODevice::ReadOnly));
        const auto methods = requests.readAll().split('\n');
        QCOMPARE(methods.count("update.identity"), 4);
        QCOMPARE(methods.count("update.status"), 2);
        QCOMPARE(methods.count("update.start"), 0);
    }

    void nativeEmptyAndFailedCheckClearPendingLabels()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        auto write = [&](const QString &name, const QByteArray &bytes) {
            QFile file(temp.path() + "/" + name);
            return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
        };
        QVERIFY(QDir().mkpath(temp.path() + "/current"));
        QVERIFY(write("current/installed.json", "{\"version\":\"0.1.0-dev\"}"));
        QVERIFY(write("system.json", "{\"board_id\":\"fixture-board\",\"contract_id\":\"fixture-contract\"}"));
        QVERIFY(write("public.pem", "fixture-only public key placeholder"));
        QVERIFY(write("updater", "#!/bin/sh\ncd \"$(dirname \"$0\")\"\nif [ \"$2\" = update.identity ]; then cat identity; else cat result; fi\n"));
        QVERIFY(write("identity", "{\"system\":{\"board_id\":\"fixture-board\",\"contract_id\":\"fixture-contract\"},\"installed\":{\"version\":\"0.1.0-dev\"},\"check_configured\":true}"));
        QVERIFY(QFile::setPermissions(temp.path() + "/updater", QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        const QJsonObject config{{"task_service", true}, {"application_root", temp.path()},
            {"system_contract", temp.path() + "/system.json"}, {"public_key", temp.path() + "/public.pem"}};
        QVERIFY(write("updater.json", QJsonDocument(config).toJson()));
        QVERIFY(write("result", "null"));
        UpdatePanel panel;
        QTRY_COMPARE(panel.process.state(), QProcess::NotRunning);
        panel.configPath = temp.path() + "/updater.json";
        panel.updaterExecutable = temp.path() + "/updater";
        panel.reloadIdentity();
        QTRY_VERIFY(panel.nativeIdentityReady);
        QTRY_COMPARE(panel.process.state(), QProcess::NotRunning);
        const QString id(32, 'e');
        QJsonObject task{{"task_id", id}, {"operation", "check"}, {"status", "running"},
                         {"stage", "checking"}, {"candidate", QJsonValue::Null}};
        QVERIFY(write("result", QJsonDocument(task).toJson()));
        panel.checkUpdates();
        QTRY_VERIFY(panel.taskPoll.isActive());
        for (auto *label : panel.preflightValues) QCOMPARE(label->text(), QString("检查中"));
        task["status"] = "success";
        task["stage"] = "completed";
        QVERIFY(write("result", QJsonDocument(task).toJson()));
        QTRY_COMPARE(panel.status->text(), QString("检查完成：无兼容更新。"));
        QCOMPARE(panel.preflightValues[0]->text(), QString("无匹配版本"));
        QCOMPARE(panel.preflightValues[1]->text(), QString("无匹配版本"));
        QCOMPARE(panel.preflightValues[2]->text(), QString("无候选清单"));
        QVERIFY(!panel.download->isEnabled() && !panel.install->isEnabled());
        task["task_id"] = QString(32, 'f');
        task["status"] = "failed";
        task["error"] = QJsonObject{{"stage", "download"}, {"code", "network-timeout"}, {"message", "fixture timeout"}};
        QVERIFY(write("result", QJsonDocument(task).toJson()));
        panel.checkUpdates();
        QTRY_VERIFY(panel.status->text().contains("network-timeout"));
        for (auto *label : panel.preflightValues) QCOMPARE(label->text(), QString("查看失败原因"));
        QVERIFY(!panel.taskPoll.isActive());
        QVERIFY(!panel.download->isEnabled() && !panel.install->isEnabled());
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
        const auto searchRecord = metadata["search_font"].toObject();
        QFile searchFile(":/hmi/fonts/" + searchRecord["output"].toString());
        QVERIFY(searchFile.open(QIODevice::ReadOnly));
        const auto searchBytes = searchFile.readAll();
        QRawFont searchRaw(searchBytes, 16, QFont::PreferDefaultHinting);
        QVERIFY(searchRaw.isValid());
        QCOMPARE(searchRaw.familyName(), QString("Dshan Search"));
        QCOMPARE(QString(QCryptographicHash::hash(searchBytes, QCryptographicHash::Sha256).toHex()),
                 searchRecord["sha256"].toString());
        QFile searchPolicy(":/hmi/fonts/" + searchRecord["policy"].toString());
        QVERIFY(searchPolicy.open(QIODevice::ReadOnly));
        QCOMPARE(QString(QCryptographicHash::hash(searchPolicy.readAll(), QCryptographicHash::Sha256).toHex()),
                 searchRecord["policy_sha256"].toString());
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
                    dialog->setProperty("exportFixturePrepared", true);
                    return;
                }
                selectedDestination = dialog->selectedFiles().value(0);
                if (selectedDestination != filename) {
                    // Exercise the editable filename as a user would. selectFile
                    // can retain the default leaf while the save dialog is visible.
                    auto *nameEdit = dialog->findChild<QLineEdit *>("fileNameEdit");
                    if (!nameEdit) { dialog->reject(); return; }
                    nameEdit->setFocus();
                    QTest::keyClick(nameEdit, Qt::Key_A, Qt::ControlModifier);
                    QTest::keyClicks(nameEdit, "visible.csv");
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
    void alarmAckUsesSelectedIdAndRecoversFromFailure_data()
    {
        QTest::addColumn<int>("httpStatus");
        QTest::addColumn<QString>("backendError");
        QTest::newRow("not-found") << 404 << QString("告警不存在或不可见");
        QTest::newRow("conflict") << 409 << QString("告警状态不允许确认");
        QTest::newRow("server-error") << 500 << QString("告警存储暂时不可用");
        QTest::newRow("business-error") << 200 << QString("告警确认未被接受");
    }

    void alarmAckUsesSelectedIdAndRecoversFromFailure()
    {
        QFETCH(int, httpStatus);
        QFETCH(QString, backendError);
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
                const QByteArray body = QJsonDocument(QJsonObject{{"ok", false}, {"error", backendError}}).toJson(QJsonDocument::Compact);
                socket->write("HTTP/1.1 " + QByteArray::number(httpStatus) + " Fixture\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: " +
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
        QStringList dialogs, dialogTexts;
        QTimer timer; connect(&timer, &QTimer::timeout, &w, [&]() {
            if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
                dialogs << box->windowTitle(); dialogTexts << box->text(); box->accept();
            }
        }); timer.start(10);
        w.alarmAck->click(); QVERIFY(!w.alarmAck->isEnabled()); w.alarmAck->click();
        QTRY_COMPARE(paths.size(), 1); QTRY_COMPARE(dialogs.size(), 1);
        QVERIFY(!w.alarmAckPending); QVERIFY(!w.alarmAck->isEnabled());
        load(); QVERIFY(w.alarmAck->isEnabled()); w.alarmAck->click();
        QTRY_COMPARE(paths.size(), 2); QTRY_COMPARE(dialogs.size(), 2); timer.stop();
        QCOMPARE(paths, (QStringList{"/api/hmi/v1/alarms/2147483655/ack", "/api/hmi/v1/alarms/2147483655/ack"}));
        QCOMPARE(dialogs, (QStringList{"操作失败", "操作失败"}));
        QCOMPARE(dialogTexts, (QStringList{backendError, backendError}));
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
        if (port > 0 && httpStatus == 409) {
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
            QTRY_VERIFY2(dialogs.contains("操作完成"), qPrintable(dialogs.join(" | ")));
            QTRY_COMPARE(w.alarmTable->item(w.alarmTable->currentRow(), 5)->data(Qt::UserRole).toString(), QString("acknowledged"));
            QVERIFY(!w.alarmAck->isEnabled());
            w.sendJson("GET", "/audit", {}, "audit");
            QTRY_COMPARE(w.auditTable->rowCount(), 1);
            QCOMPARE(w.auditTable->item(0, 2)->text(), QString("2147483655"));
            QCOMPARE(w.auditTable->item(0, 3)->data(Qt::UserRole).toString(), QString("success"));
            timer.stop(); w.refreshTimer.stop();
        }

    }






    void desktopGesturesKeepControlsAndDraftsExclusive()
    {
        using namespace DesktopV1;
        QTemporaryDir dir;DesktopCanvas canvas;canvas.configureLayoutStorage(dir.filePath("layout.json"));canvas.show();QTest::qWait(20);
        QSignalSpy apps(&canvas,&DesktopCanvas::appRequested);
        QTest::mouseClick(&canvas,Qt::RightButton,Qt::NoModifier,QPoint(100,200));QCOMPARE(apps.count(),0);QCOMPARE(canvas.state,0);
        QTest::mousePress(&canvas,Qt::LeftButton,Qt::NoModifier,QPoint(400,25));QTest::qWait(610);
        QTest::mouseRelease(&canvas,Qt::LeftButton,Qt::NoModifier,QPoint(400,25));QCOMPARE(canvas.state,1);QVERIFY(!canvas.layoutEditing());
        canvas.change(0);QTest::mousePress(&canvas,Qt::LeftButton,Qt::NoModifier,QPoint(100,200));QTest::qWait(610);
        QTest::mouseRelease(&canvas,Qt::LeftButton,Qt::NoModifier,QPoint(100,200));QCOMPARE(canvas.state,3);QCOMPARE(apps.count(),0);
        const auto before=canvas.layoutSnapshot();QTest::mousePress(&canvas,Qt::LeftButton,Qt::NoModifier,QPoint(100,440));
        QTest::mouseMove(&canvas,QPoint(600,440));QEvent cancel(QEvent::TouchCancel);QApplication::sendEvent(&canvas,&cancel);
        QTest::mouseRelease(&canvas,Qt::LeftButton,Qt::NoModifier,QPoint(600,440));QCOMPARE(canvas.layoutSnapshot(),before);
        canvas.discardLayout();canvas.change(2);QTest::qWait(20);QTest::mouseClick(canvas.search,Qt::LeftButton);
        auto *keyboard=canvas.findChild<DesktopKeyboard *>("desktopKeyboard");QVERIFY(keyboard->isVisible());keyboard->append("a");
        QTest::keyClick(&canvas,Qt::Key_Escape);QCOMPARE(canvas.state,2);QVERIFY(keyboard->composition().isEmpty());QVERIFY(keyboard->isVisible());
        QTest::keyClick(&canvas,Qt::Key_Escape);QCOMPARE(canvas.state,2);QVERIFY(!keyboard->isVisible());
        QTest::keyClick(&canvas,Qt::Key_Escape);QCOMPARE(canvas.state,0);
    }

    void desktopSearchRendersConsistentlyAcrossFontContexts_data()
    {
        QTest::addColumn<QString>("query");
        QTest::newRow("placeholder") << QString();
        QTest::newRow("chinese") << QString::fromUtf8("网络");
        QTest::newRow("latin") << QString("network");
        QTest::newRow("protocol") << QString("CAN / RS485");
        QTest::newRow("mixed") << QString::fromUtf8("网络 CAN");
    }
    void desktopSearchRendersConsistentlyAcrossFontContexts()
    {
        QFETCH(QString, query);
        struct RestoreFont {
            QFont saved = QApplication::font();
            ~RestoreFont() { QApplication::setFont(saved); }
        } restore;
        QTemporaryDir state;
        QVERIFY(state.isValid());
        const auto capture = [&](bool example, const QFont &applicationFont) {
            QApplication::setFont(applicationFont);
            DesktopV1::DesktopCanvas canvas(example);
            if (!example) canvas.configureLayoutStorage(state.filePath("layout.json"));
            canvas.resize(1024, 600);
            canvas.show(); canvas.change(2);
            canvas.search->setText(query);
            canvas.search->clearFocus(); canvas.setFocus();
            QApplication::processEvents();
            QImage image(canvas.search->size(), QImage::Format_ARGB32);
            image.fill(Qt::white);
            canvas.search->render(&image);
            return image.convertToFormat(QImage::Format_RGBA8888);
        };
        const auto preview = capture(true, QFont("DejaVu Sans", 9));
        QFont productionFont("Dshan Sans", 12);
        productionFont.setStyleStrategy(QFont::PreferAntialias);
        const auto live = capture(false, productionFont);
        if (preview != live) {
            preview.save("screenshots/search-context-preview-" + QString::fromLatin1(QTest::currentDataTag()) + ".png");
            live.save("screenshots/search-context-live-" + QString::fromLatin1(QTest::currentDataTag()) + ".png");
        }
        QVERIFY2(preview == live, "Search rendering changes with the enclosing application font");
    }

    void desktopKeyboardAcceptsChineseAndRespectsOverlayLifecycle()
    {
        using namespace DesktopV1;
        DesktopCanvas canvas;canvas.show();canvas.change(2);QTest::qWait(20);
        QTest::mouseClick(canvas.search,Qt::LeftButton);QTest::qWait(20);
        auto *keyboard=canvas.findChild<DesktopKeyboard *>("desktopKeyboard");QVERIFY(keyboard);QVERIFY(keyboard->isVisible());
        QVERIFY(!keyboard->geometry().intersects(canvas.search->geometry()));
        for(const auto &letter:QString("wangluo")) {
            auto *key=keyboard->findChild<QPushButton *>("keyboardKey_"+QString(letter));QVERIFY(key);QTest::mouseClick(key,Qt::LeftButton);
        }
        QCOMPARE(keyboard->composition(),QString("wangluo"));QVERIFY2(keyboard->candidates().contains("网络"),qPrintable(keyboard->candidates().join(",")));
        QCOMPARE(canvas.search->text(),QString());
        keyboard->choose(keyboard->candidates().indexOf("网络"));QCOMPARE(canvas.search->text(),QString("网络"));
        QVERIFY(canvas.grab().save("screenshots/desktop-chinese-keyboard.png"));
        keyboard->append("a");QVERIFY(canvas.dismissKeyboardFirst());QVERIFY(keyboard->isVisible());QVERIFY(keyboard->composition().isEmpty());
        QVERIFY(canvas.dismissKeyboardFirst());QVERIFY(!keyboard->isVisible());QCOMPARE(canvas.state,2);
        QTest::mouseClick(canvas.search,Qt::LeftButton);QVERIFY(keyboard->isVisible());
        canvas.change(0);QVERIFY(!keyboard->isVisible());QVERIFY(canvas.search->text().isEmpty());
        canvas.resize(1280,720);canvas.change(2);QTest::qWait(20);QTest::mouseClick(canvas.search,Qt::LeftButton);
        QVERIFY(!keyboard->geometry().intersects(canvas.search->geometry()));QVERIFY(canvas.rect().contains(keyboard->geometry()));
        for(auto *button:keyboard->findChildren<QPushButton *>()){QVERIFY(button->width()>=44);QVERIFY(button->height()>=44);}
        keyboard->findChild<QPushButton *>("keyboardLanguage")->click();keyboard->append("a");QCOMPARE(canvas.search->text(),QString("a"));
        keyboard->findChild<QPushButton *>("keyboardNumbers")->click();keyboard->append("1");QCOMPARE(canvas.search->text(),QString("a1"));
        canvas.change(0);
    }

    void desktopShortSwipesDoNotCommitOrLaunch()
    {
        DesktopV1::DesktopCanvas canvas(false);
        QTemporaryDir dir;
        canvas.configureLayoutStorage(dir.filePath("layout.json"));
        canvas.resize(1024,600); canvas.show();
        QSignalSpy apps(&canvas,&DesktopV1::DesktopCanvas::appRequested);
        const auto swipe = [&](int state, QPoint from, QPoint to) {
            canvas.change(state); canvas.repaint();
            QTest::mousePress(&canvas,Qt::LeftButton,Qt::NoModifier,from);
            QTest::mouseMove(&canvas,to);
            QTest::mouseRelease(&canvas,Qt::LeftButton,Qt::NoModifier,to);
        };
        swipe(0,{500,25},{500,79}); QCOMPARE(canvas.state,0);
        swipe(0,{500,570},{500,516}); QCOMPARE(canvas.state,0);
        swipe(1,{500,350},{500,296}); QCOMPARE(canvas.state,1);
        swipe(2,{500,300},{500,354}); QCOMPARE(canvas.state,2);
        for (int dx : {-54,54}) {
            swipe(0,{450,300},{450+dx,300}); QCOMPARE(canvas.state,0);
            QCOMPARE(canvas.desk,0);
        }
        QCOMPARE(apps.count(),0);
        QVERIFY(!canvas.layoutEditing());
    }
    void desktopDrawerSearchAndCategoriesRejectEmptyTargets()
    {
        DesktopV1::DesktopCanvas canvas(false);
        canvas.resize(1024,600); canvas.show(); canvas.change(2); QTest::qWait(30);
        QSignalSpy apps(&canvas,&DesktopV1::DesktopCanvas::appRequested);
        canvas.search->setText("网络"); canvas.repaint();
        QTest::mouseClick(&canvas,Qt::LeftButton,Qt::NoModifier,QPoint(110,280));
        QCOMPARE(apps.count(),1);
        QCOMPARE(apps.takeFirst()[0].toString(),QString("network"));
        canvas.change(2); canvas.search->setText("网络"); canvas.repaint();
        QTest::mouseClick(&canvas,Qt::LeftButton,Qt::NoModifier,QPoint(210,195));
        QCOMPARE(canvas.group,1); canvas.repaint();
        for (int y : {280,423})
            for (int x : {110,265,420,575,730,885})
                QTest::mouseClick(&canvas,Qt::LeftButton,Qt::NoModifier,QPoint(x,y));
        QCOMPARE(apps.count(),0); QCOMPARE(canvas.state,2);
        canvas.search->clear(); canvas.repaint();
        QTest::mouseClick(&canvas,Qt::LeftButton,Qt::NoModifier,QPoint(110,280));
        QCOMPARE(apps.count(),1);
        QCOMPARE(apps.takeFirst()[0].toString(),QString("overview"));
        canvas.change(2); canvas.search->setText("不存在的应用名"); canvas.repaint();
        for (int y : {280,423})
            for (int x : {110,265,420,575,730,885})
                QTest::mouseClick(&canvas,Qt::LeftButton,Qt::NoModifier,QPoint(x,y));
        QCOMPARE(apps.count(),0); QCOMPARE(canvas.state,2);
    }

    void desktopLayoutResetBacksUpBeforeReplacing()
    {
        using namespace DesktopV1;
        QTemporaryDir dir;const auto path=dir.filePath("layout.json");QFile file(path);
        const QByteArray broken="{broken JSON preserved}";
        QVERIFY(file.open(QIODevice::WriteOnly));file.write(broken);file.close();
        DesktopLayoutStore store(path);QString error;store.load(&error);QVERIFY(!error.isEmpty());
        QVERIFY(store.restoreDefaults(&error));QFile backup(path+".before-reset");QVERIFY(backup.open(QIODevice::ReadOnly));QCOMPARE(backup.readAll(),broken);backup.close();
        DesktopLayoutStore reopened(path);QCOMPARE(reopened.load(&error),DesktopLayoutStore::defaults());
        auto layout=DesktopLayoutStore::defaults();layout["revision"]=2;QVERIFY(store.commit(layout,&error));
        // Backup failure must not touch the current layout.
        QVERIFY(backup.remove());QVERIFY(QDir().mkdir(backup.fileName()));
        QVERIFY(file.open(QIODevice::ReadOnly));const auto before=file.readAll();file.close();
        QVERIFY(!store.restoreDefaults(&error));QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),before);file.close();
        QVERIFY(QDir().rmdir(backup.fileName()));
        DesktopCanvas canvas;canvas.configureLayoutStorage(path);canvas.change(3);QVERIFY(canvas.removeWidget(0));
        QVERIFY(canvas.restoreDefaultLayout());QCOMPARE(canvas.layoutSnapshot(),DesktopLayoutStore::defaults());QVERIFY(!canvas.layoutEditing());
    }

    void desktopPointHistoryRejectsInvalidStaleAndOutOfOrderSamples()
    {
        using namespace DesktopV1;
        DesktopPointHistory history;history.setBindings({"sensor/temperature"});
        const qint64 start=1792380000000LL;
        auto sample=[&](qint64 ts,double value) {return QJsonObject{{"point_id","sensor/temperature"},{"name","环境温度"},
            {"type","temperature"},{"unit","°C"},{"source","mqtt"},{"quality","good"},{"value",value},
            {"timestamp",QDateTime::fromMSecsSinceEpoch(ts,Qt::UTC).toString(Qt::ISODateWithMs)}};};
        history.ingest({sample(start,24)},true,start,0);
        QCOMPARE(history.current("sensor/temperature",start,0)["value"].toDouble(),24.0);
        QCOMPARE(history.series("sensor/temperature",start).size(),1);
        history.ingest({sample(start,999)},true,start+1000,1000);
        QCOMPARE(history.current("sensor/temperature",start+1000,1000)["value"].toDouble(),24.0);
        history.ingest({sample(start-1000,20)},true,start+2000,2000);
        QCOMPARE(history.series("sensor/temperature",start+2000).size(),1);
        QCOMPARE(history.current("sensor/temperature",start+60000,60000)["quality"].toString(),QString("stale"));
        QVERIFY(history.current("sensor/temperature",start+60000,60000)["value"].isNull());
        qint64 t=start+3000,mono=3000;
        for(const auto &value:QList<QJsonValue>{QJsonValue(),QJsonValue("24"),QJsonValue(true)}) {
            auto bad=sample(t,24);bad["value"]=value;history.ingest({bad},true,t,mono);
            QVERIFY(history.current("sensor/temperature",t,mono)["quality"]!="good");t+=1000;mono+=1000;
        }
        auto wrong=sample(t,24);wrong["unit"]="Pa";history.ingest({wrong},true,t,mono);
        QVERIFY(history.current("sensor/temperature",t,mono)["value"].isNull());t+=1000;mono+=1000;
        auto demo=sample(t,24);demo["source"]="demo";history.ingest({demo},true,t,mono);
        QVERIFY(history.current("sensor/temperature",t,mono)["quality"]!="good");t+=1000;mono+=1000;
        history.ingest({sample(t,24)},true,t,mono);
        QVERIFY(history.series("sensor/temperature",t).last().startsSegment);
        history.ingest({},false,t+1000,mono+1000);QVERIFY(history.current("sensor/temperature",t+1000,mono+1000)["value"].isNull());
        history.ingest({sample(t,24)},true,t+2000,mono+2000);
        QVERIFY(history.current("sensor/temperature",t+2000,mono+2000)["quality"]!="good");
        history.ingest({sample(t+3000,24)},true,t+3000,mono+3000);
        QVERIFY(history.series("sensor/temperature",t+3000).last().startsSegment);
        // Independent UTC jump breaks history instead of stretching an old segment.
        history.ingest({sample(t+200000,24)},true,t+200000,mono+4000);
        QCOMPARE(history.series("sensor/temperature",t+200000).size(),1);
        DesktopPointHistory bounded;bounded.setBindings({"sensor/temperature"});
        for(int i=0;i<1900;++i)bounded.ingest({sample(start+i*500,24)},true,start+i*500,i*500);
        QCOMPARE(bounded.series("sensor/temperature",start+1899*500).size(),1800);
        QVERIFY(DesktopPointHistory::validUnit("pressure","MPa"));QVERIFY(!DesktopPointHistory::validUnit("pressure","unknown"));
    }

    void desktopTrendBindingUsesStableIdsAndPreservesDraftHistory()
    {
        using namespace DesktopV1;
        QTemporaryDir dir;DesktopCanvas canvas;canvas.configureLayoutStorage(dir.filePath("layout.json"));
        const auto now=QDateTime::currentDateTimeUtc();
        auto point=[&](QString id,QString type,QString unit,double value){return QJsonObject{{"point_id",id},{"name",id},
            {"type",type},{"unit",unit},{"source","mqtt"},{"quality","good"},{"value",value},{"timestamp",now.toString(Qt::ISODateWithMs)}};};
        const QJsonArray points={point("sensor/temperature","temperature","°C",24.3),point("sensor/humidity","humidity","%",52.6)};
        canvas.applyServices({{"service_connected",true},{"reporting",QJsonObject{{"points",points}}}});
        canvas.change(3);QVERIFY(!canvas.bindTrend(0,"arbitrary/unknown",QString()));
        QVERIFY(canvas.bindTrend(0,"sensor/temperature","sensor/humidity"));
        QCOMPARE(canvas.boundPoint(0)["value"].toDouble(),24.3);QCOMPARE(canvas.boundHistory(0).size(),1);
        QVERIFY(canvas.commitLayout());const auto saved=canvas.layoutSnapshot();
        canvas.change(3);QVERIFY(canvas.removeWidget(0));canvas.discardLayout();
        QCOMPARE(canvas.layoutSnapshot(),saved);QCOMPARE(canvas.boundHistory(0).size(),1);
        DesktopCanvas restored;restored.configureLayoutStorage(dir.filePath("layout.json"));QVERIFY(restored.boundPoint(0)["value"].isNull());
        restored.applyServices({{"service_connected",true},{"reporting",QJsonObject{{"points",points}}}});
        QCOMPARE(restored.boundPoint(0,true)["value"].toDouble(),52.6);
        restored.show();QTest::qWait(20);QVERIFY(restored.grab().save("screenshots/desktop-bound-trend.png"));
        restored.applyServices({{"service_connected",false}});QVERIFY(restored.boundPoint(0)["value"].isNull());
    }

    void desktopImagePickerPreservesDraftOnCancel_data()
    {
        QTest::addColumn<bool>("acceptBinding");
        QTest::newRow("apply") << true;
        QTest::newRow("cancel") << false;
    }
    void desktopImagePickerPreservesDraftOnCancel()
    {
        QFETCH(bool, acceptBinding);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString first = dir.filePath("first.png"), second = dir.filePath("second.jpg");
        QImage sample(32, 24, QImage::Format_RGB32);
        sample.fill(Qt::red);
        QVERIFY(sample.save(first));
        sample.fill(Qt::blue);
        QVERIFY(sample.save(second));
        DesktopV1::DesktopCanvas canvas(false);
        canvas.resize(1024, 600);
        canvas.configureLayoutStorage(dir.filePath("layout.json"));
        canvas.desk = 1;
        canvas.change(3);
        QVERIFY(canvas.bindImage(0, first));
        const auto before = canvas.layoutSnapshot();
        canvas.show();
        QTest::qWait(30);
        bool bindingSeen = false, pickerSeen = false, sized = false, fontSized = true;
        bool touchSized = true, selected = false, timedOut = false, localized = true;
        QString labelFailure;
        int step = 0;
        QElapsedTimer pickerReady;
        QTimer poll, timeout;
        timeout.setSingleShot(true);
        connect(&timeout, &QTimer::timeout, &canvas, [&]() {
            timedOut = true;
            for (auto *w : QApplication::topLevelWidgets())
                if (auto *dialog = qobject_cast<QDialog *>(w)) dialog->reject();
        });
        connect(&poll, &QTimer::timeout, &canvas, [&]() {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog) return;
            if (step == 0 && dialog->objectName() == "desktopImageBindingDialog") {
                bindingSeen = true;
                sized = dialog->width() <= 992 && dialog->height() <= 568;
                for (auto *label : dialog->findChildren<QLabel *>())
                    fontSized = fontSized && label->font().pixelSize() == 14;
                for (auto *button : dialog->findChildren<QPushButton *>())
                    touchSized = touchSized && button->height() >= 44;
                auto *button = dialog->findChild<QPushButton *>("desktopImageChoose");
                if (!button) return;
                ++step;
                QMetaObject::invokeMethod(button, "click", Qt::QueuedConnection);
            } else if (step == 1 && dialog->objectName() == "desktopLocalFilePicker") {
                auto *picker = qobject_cast<QFileDialog *>(dialog);
                if (!picker) return;
                pickerSeen = picker->testOption(QFileDialog::DontUseNativeDialog);
                localized = localized && QStringList({"选择", "打开"}).contains(picker->labelText(QFileDialog::Accept))
                            && picker->labelText(QFileDialog::Reject) == "取消"
                            && picker->labelText(QFileDialog::LookIn) == "位置";
                if (!localized) labelFailure = picker->labelText(QFileDialog::Accept) + " | "
                    + picker->labelText(QFileDialog::Reject) + " | " + picker->labelText(QFileDialog::LookIn);
                auto *parentButton = picker->findChild<QToolButton *>("toParentButton");
                localized = localized && parentButton && parentButton->text() == "上级"
                    && parentButton->toolButtonStyle() == Qt::ToolButtonTextOnly;
                auto *tree = picker->findChild<QTreeView *>("treeView");
                localized = localized && tree && tree->model()->headerData(0, Qt::Horizontal).toString() == "名称";
                sized = sized && picker->width() <= 992 && picker->height() <= 568;
                if (!pickerReady.isValid()) { pickerReady.start(); return; }
                // QFileSystemModel populates and restores its initial selection asynchronously.
                if (pickerReady.elapsed() < 200) return;
                auto *fileName = picker->findChild<QLineEdit *>("fileNameEdit");
                if (!fileName) return;
                fileName->clear();
                QTest::keyClicks(fileName, QFileInfo(second).fileName());
                if (picker->selectedFiles() != QStringList{second}) return;
                ++step;
                QMetaObject::invokeMethod(picker, "accept", Qt::QueuedConnection);
            } else if (step == 2 && dialog->objectName() == "desktopImageBindingDialog") {
                for (auto *label : dialog->findChildren<QLabel *>())
                    selected = selected || label->text() == "second.jpg";
                if (!selected) return;
                ++step;
                acceptBinding ? dialog->accept() : dialog->reject();
            }
        });
        poll.start(30);
        timeout.start(6000);
        // Exercise the public touch route into the modal, including a large inherited font.
        const auto oldFont = QApplication::font();
        QFont large = oldFont;
        large.setPointSize(24);
        QApplication::setFont(large);
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(250, 220));
        QApplication::setFont(oldFont);
        poll.stop(); timeout.stop();
        QVERIFY(!timedOut);
        QVERIFY(bindingSeen); QVERIFY(pickerSeen); QVERIFY(sized);
        QVERIFY(fontSized); QVERIFY(touchSized); QVERIFY(selected); QVERIFY2(localized, qPrintable(labelFailure));
        if (acceptBinding) {
            QTRY_VERIFY(!canvas.boundImage(0).isNull());
            QVERIFY(canvas.boundImage(0).pixelColor(0, 0).blue() >= 253);
            QVERIFY(canvas.boundImage(0).pixelColor(0, 0).red() <= 2);
            QVERIFY(canvas.layoutSnapshot() != before);
        } else {
            QCOMPARE(canvas.layoutSnapshot(), before);
            QTRY_COMPARE(canvas.boundImage(0).pixelColor(0, 0), QColor(Qt::red));
        }
        DesktopV1::DesktopCanvas disk(false);
        disk.configureLayoutStorage(dir.filePath("layout.json"));
        disk.desk = 1;
        QVERIFY(disk.boundImage(0).isNull());
    }
    void desktopImagesDecodeSafelyAndPersistBindings()
    {
        using namespace DesktopV1;
        QTemporaryDir dir; QVERIFY(dir.isValid()); const auto good=dir.filePath("equipment.png");
        QImage source(1400,900,QImage::Format_RGB32); source.fill(QColor("#1468fa"));
        {QPainter painter(&source);painter.setPen(Qt::white);painter.setFont(QFont("Dshan Sans",48));painter.drawText(source.rect(),Qt::AlignCenter,"图片组件测试 · 非现场数据");}
        QVERIFY(source.save(good));
        const auto broken=dir.filePath("broken.png");QFile f(broken);QVERIFY(f.open(QIODevice::WriteOnly));f.write("not an image");f.close();
        QImage large(4097,1,QImage::Format_RGB32);large.fill(Qt::red);const auto oversized=dir.filePath("oversized.png");QVERIFY(large.save(oversized));
        const auto bmp=dir.filePath("unsupported.bmp");QVERIFY(source.copy(0,0,8,8).save(bmp));
        DesktopImageCache cache;QSignalSpy changes(&cache,&DesktopImageCache::changed);
        cache.setPaths({good,broken,oversized,bmp});QTRY_COMPARE(changes.count(),4);
        QVERIFY(!cache.image(good).isNull());QVERIFY(cache.image(good).width()<=1024);QVERIFY(cache.image(good).height()<=600);
        for(const auto &path:{broken,oversized,bmp}) {QVERIFY(cache.image(path).isNull());QVERIFY(!cache.status(path).isEmpty());}
        QSignalSpy dispatched(&cache,&DesktopImageCache::decodeRequested);
        for(int i=0;i<30;++i)cache.reload(good);
        QTRY_VERIFY(!cache.image(good).isNull());QVERIFY(dispatched.count()<=2);
        cache.setPaths({});QVERIFY(cache.image(good).isNull());
        const auto layoutFile=dir.filePath("layout.json");DesktopCanvas canvas;canvas.configureLayoutStorage(layoutFile);
        canvas.desk=1;canvas.change(3);QVERIFY(!canvas.bindImage(0,"relative.png"));
        QVERIFY(!canvas.bindImage(0,good,"stretch"));QVERIFY(!canvas.bindImage(1,good));
        QVERIFY(canvas.bindImage(0,good,"contain"));QTRY_VERIFY(!canvas.boundImage(0).isNull());
        QVERIFY(canvas.commitLayout());DesktopCanvas restored;restored.configureLayoutStorage(layoutFile);
        QCOMPARE(restored.desk,1);QTRY_VERIFY(!restored.boundImage(0).isNull());
        restored.change(3);QVERIFY(restored.bindImage(0,broken));QTRY_VERIFY(restored.boundImage(0).isNull());
        restored.discardLayout();QTRY_VERIFY(!restored.boundImage(0).isNull());
        restored.show();QTest::qWait(20);QDir().mkpath("screenshots");QVERIFY(restored.grab().save("screenshots/desktop-image-bound.png"));
        restored.change(3);QVERIFY(restored.bindImage(0,QString()));QVERIFY(restored.commitLayout());
        DesktopCanvas cleared;cleared.configureLayoutStorage(layoutFile);QVERIFY(cleared.boundImage(0).isNull());
    }

    void desktopLayoutStorePreservesInvalidAndConflictingFiles()
    {
        using DesktopV1::DesktopLayoutStore;
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString path=dir.filePath("layout.json"); QString error;
        DesktopLayoutStore first(path); auto layout=first.load(&error);
        QVERIFY(DesktopLayoutStore::validate(layout,&error)); QVERIFY(first.commit(layout,&error));
        QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly)); const auto initial=file.readAll(); file.close();
        DesktopLayoutStore second(path); QCOMPARE(second.load(&error),layout);
        layout["revision"]=2; QVERIFY(first.commit(layout,&error));
        QVERIFY(!second.commit(layout,&error)); QVERIFY(!error.isEmpty());
        auto invalid=layout; invalid["schema_version"]=99;
        QVERIFY(!DesktopLayoutStore::validate(invalid,&error));
        invalid=layout; auto pages=invalid["pages"].toArray(); auto page=pages[0].toObject();
        auto entries=page["slots"].toArray(); auto entry=entries[1].toObject();
        entry["id"]=entries[0].toObject()["id"]; entries[1]=entry;
        page["slots"]=entries; pages[0]=page; invalid["pages"]=pages;
        QVERIFY(!DesktopLayoutStore::validate(invalid,&error));
        invalid=layout; pages=invalid["pages"].toArray(); page=pages[0].toObject(); entries=page["slots"].toArray();
        entry=entries[0].toObject(); entry["type"]="cloud"; entries[0]=entry;
        page["slots"]=entries; pages[0]=page; invalid["pages"]=pages;
        QVERIFY(!first.commit(invalid,&error));
        QVERIFY(file.open(QIODevice::ReadOnly)); const auto valid=file.readAll(); file.close();
        QVERIFY(valid!=initial);
        const auto permissions=QFile::permissions(dir.path());
        QVERIFY(QFile::setPermissions(dir.path(),QFile::ReadOwner|QFile::ExeOwner));
        const bool writeWithoutPermission=first.commit(layout,&error);
        QVERIFY(QFile::setPermissions(dir.path(),permissions)); QVERIFY(!writeWithoutPermission);
        QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(),valid); file.close();
        const QList<QByteArray> broken={QByteArray("{broken"),QByteArray("{\"schema_version\":99}"),QByteArray(140000,'x')};
        for(const auto &bytes:broken) {
            QVERIFY(file.open(QIODevice::WriteOnly|QIODevice::Truncate)); QCOMPARE(file.write(bytes),qint64(bytes.size())); file.close();
            DesktopLayoutStore corrupt(path); QCOMPARE(corrupt.load(&error),DesktopLayoutStore::defaults());
            QVERIFY(!error.isEmpty()); QVERIFY(!corrupt.commit(layout,&error));
            QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(),bytes); file.close();
        }
        const QString linked=dir.filePath("linked.json"); QVERIFY(QFile::link(path,linked));
        DesktopLayoutStore symlink(linked); symlink.load(&error); QVERIFY(!symlink.commit(layout,&error));
    }

    void overlayAnimationKeepsEventLoopResponsiveAndBlocksOldTargets()
    {
        qputenv("DSHANCONTROL_DESKTOP_ANIMATIONS","1");
        QTemporaryDir dir;DesktopV1::DesktopCanvas canvas;
        qputenv("DSHANCONTROL_DESKTOP_ANIMATIONS","0");
        canvas.configureLayoutStorage(dir.filePath("layout.json"));canvas.show();QTest::qWait(20);
        int ticks=0;QTimer timer;connect(&timer,&QTimer::timeout,this,[&](){++ticks;});timer.start(15);
        QSignalSpy covered(&canvas,&DesktopV1::DesktopCanvas::overlayChanged);
        QSignalSpy applications(&canvas,&DesktopV1::DesktopCanvas::appRequested);
        canvas.change(2);QVERIFY(canvas.overlayTransitionActive());QVERIFY(!canvas.search->isVisible());
        QTest::mouseClick(&canvas,Qt::LeftButton,Qt::NoModifier,QPoint(100,270));QCOMPARE(applications.size(),0);
        QTRY_VERIFY_WITH_TIMEOUT(!canvas.overlayTransitionActive(),1000);
        QVERIFY(ticks>=3);QVERIFY(canvas.search->isVisible());
        canvas.change(0);QVERIFY(canvas.overlayTransitionActive());QVERIFY(covered.last()[0].toBool());
        QTRY_VERIFY_WITH_TIMEOUT(!canvas.overlayTransitionActive(),1000);
        QVERIFY(!covered.last()[0].toBool());QVERIFY(!canvas.search->isVisible());
        canvas.change(1);QTest::qWait(40);canvas.change(0);
        QTRY_VERIFY_WITH_TIMEOUT(!canvas.overlayTransitionActive(),1000);QCOMPARE(canvas.state,0);
        DesktopV1::DesktopCanvas lowPower;lowPower.change(2);QVERIFY(!lowPower.overlayTransitionActive());
    }

    void incompatibleDragReturnsWithoutChangingDraftOrBlockingEvents()
    {
        qputenv("DSHANCONTROL_DESKTOP_ANIMATIONS","1");QTemporaryDir dir;DesktopV1::DesktopCanvas canvas;
        qputenv("DSHANCONTROL_DESKTOP_ANIMATIONS","0");canvas.configureLayoutStorage(dir.filePath("layout.json"));canvas.show();canvas.change(3);QTest::qWait(20);
        const auto before=canvas.layoutSnapshot();int ticks=0;QTimer timer;connect(&timer,&QTimer::timeout,this,[&](){++ticks;});timer.start(15);
        QTest::mousePress(&canvas,Qt::LeftButton,Qt::NoModifier,QPoint(100,440));
        QTest::mouseMove(&canvas,QPoint(200,220),20);QTest::mouseRelease(&canvas,Qt::LeftButton,Qt::NoModifier,QPoint(200,220));
        QVERIFY(canvas.dragReturning());QCOMPARE(canvas.layoutSnapshot(),before);
        QTRY_VERIFY_WITH_TIMEOUT(!canvas.dragReturning(),1000);QVERIFY(ticks>=3);QCOMPARE(canvas.layoutSnapshot(),before);
        QTest::mousePress(&canvas,Qt::LeftButton,Qt::NoModifier,QPoint(100,440));QTest::mouseMove(&canvas,QPoint(200,220),20);
        QEvent cancel(QEvent::TouchCancel);QApplication::sendEvent(&canvas,&cancel);
        QTest::mouseRelease(&canvas,Qt::LeftButton,Qt::NoModifier,QPoint(200,220));
        QVERIFY(!canvas.dragReturning());QCOMPARE(canvas.layoutSnapshot(),before);
    }

    void componentLibraryUsesInlineChoicesAndRejectsIncompatibleSlots()
    {
        QTemporaryDir dir;DesktopV1::DesktopCanvas canvas;canvas.configureLayoutStorage(dir.filePath("layout.json"));
        canvas.resize(1024,600);canvas.show();QTest::qWait(20);
        const auto before=canvas.layoutSnapshot();canvas.change(4);QTest::qWait(10);
        QTest::mouseClick(&canvas,Qt::LeftButton,Qt::NoModifier,QPoint(750,235));
        QCOMPARE(canvas.layoutSnapshot(),before);QVERIFY(!canvas.backLibraryLevel());
        canvas.change(3);QVERIFY(canvas.removeWidget(0));canvas.change(4);QTest::qWait(10);
        QTest::mouseClick(&canvas,Qt::LeftButton,Qt::NoModifier,QPoint(750,235));QTest::qWait(10);
        QVERIFY(canvas.backLibraryLevel());QTest::qWait(10);
        QTest::mouseClick(&canvas,Qt::LeftButton,Qt::NoModifier,QPoint(750,235));QTest::qWait(10);
        QDir().mkpath("screenshots");QVERIFY(canvas.grab().save("screenshots/desktop-library-subgroup.png"));
        // Only the large empty slot exists: image is compatible, video is disabled.
        QTest::mouseClick(&canvas,Qt::LeftButton,Qt::NoModifier,QPoint(750,245));QCOMPARE(canvas.state,4);
        QTest::keyClick(&canvas,Qt::Key_Escape);QCOMPARE(canvas.state,4);QVERIFY(!canvas.backLibraryLevel());QTest::qWait(10);
        QTest::mouseClick(&canvas,Qt::LeftButton,Qt::NoModifier,QPoint(750,235));QTest::qWait(10);
        QTest::mouseClick(&canvas,Qt::LeftButton,Qt::NoModifier,QPoint(750,185));QCOMPARE(canvas.state,3);
        QCOMPARE(canvas.layoutSnapshot()["pages"].toArray()[0].toObject()["slots"].toArray()[0].toObject()["type"].toString(),QString("image"));
        canvas.discardLayout();QCOMPARE(canvas.layoutSnapshot(),before);
    }

    void trendContextUsesSharedStablePointHistoryAndLegacyRouteStillWorks()
    {
        QTemporaryDir dir;DesktopV1::DesktopCanvas source;source.configureLayoutStorage(dir.filePath("layout.json"));
        const auto stamp=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
        QJsonObject point{{"point_id","machine/pressure"},{"name","泵出口压力"},{"type","pressure"},{"unit","MPa"},
            {"value",0.62},{"quality","good"},{"source","modbus"},{"timestamp",stamp}};
        source.applyServices({{"service_connected",true},{"reporting",QJsonObject{{"points",QJsonArray{point}}}}});
        source.change(3);QVERIFY(source.bindTrend(0,"machine/pressure",QString()));QVERIFY(source.commitLayout());
        QSignalSpy context(&source,&DesktopV1::DesktopCanvas::appContextRequested);source.openWidget(0);
        QCOMPARE(context.size(),1);QCOMPARE(context[0][0].toString(),QString("devices"));
        MainWindow window;isolate(window);window.setDesktopPointSource(&source);
        QVERIFY(window.openApplication("devices",qvariant_cast<QJsonObject>(context[0][1])));
        QCOMPARE(window.devicePageStack->currentIndex(),1);
        QCOMPARE(window.boundTrendPanel->reading()["value"].toDouble(),0.62);
        QCOMPARE(window.boundTrendPanel->reading()["point_id"].toString(),QString("machine/pressure"));
        QCOMPARE(source.boundHistory(0).size(),1);
        window.resize(1024,600);window.show();QTest::qWait(30);
        DesktopV1::DesktopShell shell;
        auto *embedded=shell.findChild<MainWindow *>();QVERIFY(embedded);isolate(*embedded);
        embedded->setDesktopPointSource(&source);
        QVERIFY(shell.openApplication("devices",qvariant_cast<QJsonObject>(context[0][1])));
        shell.resize(1024,600);shell.show();QTest::qWait(30);
        QVERIFY(embedded->boundTrendPanel->mapTo(&shell,embedded->boundTrendPanel->rect().bottomRight()).y()<=542);
        QDir().mkpath("screenshots");QVERIFY(shell.grab().save("screenshots/desktop-trend-context.png"));
        source.applyServices({});QVERIFY(window.boundTrendPanel->reading()["value"].isNull());
        QVERIFY(window.openApplication("devices"));QCOMPARE(window.devicePageStack->currentIndex(),0);
        QVERIFY(!window.openApplication("devices",{{"point_id","invalid"}}));
    }

    void boundHomeAndVideoKeepStableContextWithoutAutoplay()
    {
        using DesktopV1::DesktopCanvas;
        QTemporaryDir dir;
        const auto layout=dir.filePath("layout.json");
        const auto media=dir.filePath("not-decoded-test.mp4");
        QFile file(media);QVERIFY(file.open(QIODevice::WriteOnly));file.write("test-only invalid media bytes");file.close();
        DesktopCanvas canvas;canvas.configureLayoutStorage(layout);
        QJsonObject home{{"configured",true},{"fresh",true},{"devices",QJsonArray{
            QJsonObject{{"id","lamp-a"},{"name","台灯甲"},{"online",true},{"room","A"}},
            QJsonObject{{"id","lamp-b"},{"name","台灯乙"},{"online",true},{"room","B"}}}}};
        QJsonObject snapshot{{"service_connected",true},{"home",home}};
        canvas.applyServices(snapshot);canvas.change(3);
        QVERIFY(canvas.bindHome(5,"lamp-b"));QVERIFY(!canvas.bindHome(5,"unknown"));
        QVERIFY(canvas.bindVideo(1,media));QVERIFY(!canvas.bindVideo(1,"relative.mp4"));
        QVERIFY(canvas.commitLayout());
        DesktopCanvas restored;restored.configureLayoutStorage(layout);
        QSignalSpy context(&restored,&DesktopCanvas::appContextRequested);
        restored.openWidget(5);QCOMPARE(context.size(),1);
        QCOMPARE(context[0][0].toString(),QString("home"));
        QCOMPARE(qvariant_cast<QJsonObject>(context[0][1])["device_id"].toString(),QString("lamp-b"));
        restored.openWidget(1);QCOMPARE(context.size(),2);
        QCOMPARE(qvariant_cast<QJsonObject>(context[1][1])["path"].toString(),media);
        MainWindow window;isolate(window);window.familyPanel->apply(snapshot);
        QVERIFY(window.openApplication("home",{{"device_id","lamp-b"}}));
        auto *devices=window.familyPanel->findChild<QTableWidget *>("homeDevices");
        QVERIFY(devices->currentRow()>=0);
        QCOMPARE(devices->item(devices->currentRow(),0)->data(Qt::UserRole).toString(),QString("lamp-b"));
        QVERIFY(window.openApplication("video",{{"path",media}}));
        QCOMPARE(window.mediaPanel->selectedFile,media);
        QVERIFY(window.mediaPanel->session.queue.isEmpty());
        QVERIFY(window.openApplication("video",{{"path",dir.filePath("removed.mp4")}}));
        QVERIFY(window.mediaPanel->selectedFile.isEmpty());
        QVERIFY(!window.mediaPanel->play->isEnabled());
        restored.change(3);QVERIFY(restored.bindHome(5,"lamp-b"));
        QVERIFY(restored.bindVideo(1,QString()));restored.discardLayout();
        context.clear();restored.openWidget(1);QCOMPARE(qvariant_cast<QJsonObject>(context[0][1])["path"].toString(),media);
    }

    void desktopInitialDefaultIsSeparateFromEditingAndPreservesExistingFiles()
    {
        QTemporaryDir dir;
        const auto path = dir.filePath("layout-v1.json");
        DesktopV1::DesktopCanvas canvas;
        canvas.configureLayoutStorage(path);
        QVERIFY(QFileInfo::exists(path));
        QVERIFY(!canvas.layoutEditing());
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto before = file.readAll();
        file.close();
        QCOMPARE(QJsonDocument::fromJson(before).object(), DesktopV1::DesktopLayoutStore::defaults());
        canvas.change(3); QVERIFY(canvas.removeWidget(0)); canvas.discardLayout();
        QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), before); file.close();
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate)); file.write("FUTURE OR BROKEN USER FILE"); file.close();
        canvas.configureLayoutStorage(path);
        QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), QByteArray("FUTURE OR BROKEN USER FILE"));
        QVERIFY(!canvas.layoutEditing());
    }

    void desktopLayoutEditingPersistsAndCancels()
    {
        using DesktopV1::DesktopCanvas;
        QTemporaryDir dir; QVERIFY(dir.isValid()); const QString path=dir.filePath("layout.json");
        DesktopCanvas canvas; canvas.configureLayoutStorage(path); canvas.show();
        const auto original=canvas.layoutSnapshot();
        canvas.change(3); QVERIFY(canvas.layoutEditing()); QVERIFY(canvas.removeWidget(3));
        QVERIFY(!canvas.placeWidget("cloud",3)); QVERIFY(canvas.placeWidget("alarms",3));
        QVERIFY(canvas.swapWidgets(3,5)); QVERIFY(!canvas.swapWidgets(0,3));
        QVERIFY(canvas.commitLayout()); QVERIFY(!canvas.layoutEditing());
        const auto saved=canvas.layoutSnapshot(); QVERIFY(saved!=original);
        QCOMPARE(saved["pages"].toArray()[1],original["pages"].toArray()[1]);
        DesktopCanvas restored; restored.configureLayoutStorage(path); QCOMPARE(restored.layoutSnapshot(),saved);
        restored.change(3); QVERIFY(restored.removeWidget(0)); restored.discardLayout(); QCOMPARE(restored.layoutSnapshot(),saved);
        canvas.change(3); QVERIFY(canvas.removeWidget(0));
        QTimer::singleShot(0,[](){ auto *box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget()); if(box)box->button(QMessageBox::Cancel)->click(); });
        canvas.change(0); QVERIFY(canvas.layoutEditing());
        QTimer::singleShot(0,[](){ auto *box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget()); if(box)box->button(QMessageBox::Discard)->click(); });
        canvas.change(0); QVERIFY(!canvas.layoutEditing()); QCOMPARE(canvas.layoutSnapshot(),saved);
        DesktopCanvas denied; denied.configureLayoutStorage(dir.filePath("missing/layout.json")); denied.change(3);
        QVERIFY(denied.removeWidget(0)); QVERIFY(!denied.commitLayout()); QVERIFY(denied.layoutEditing());
        canvas.change(3); QTest::mousePress(&canvas,Qt::LeftButton,Qt::NoModifier,QPoint(100,440));
        QTest::mouseMove(&canvas,QPoint(600,440),20); QTest::mouseRelease(&canvas,Qt::LeftButton,Qt::NoModifier,QPoint(600,440));
        QVERIFY(canvas.layoutSnapshot()!=saved); QVERIFY(canvas.commitLayout());
        QDir().mkpath("screenshots"); QVERIFY(canvas.grab().save("screenshots/desktop-custom-layout.png"));
    }

    void desktopBrightnessReadsDriverAndCancelsStaleObservations()
    {
        QTemporaryDir dir;
        QLocalServer server;
        const auto path=dir.filePath("display.sock");
        QVERIFY(server.listen(path));
        QList<QJsonObject> requests;
        QList<QPointer<QLocalSocket>> held;
        bool hold=false;
        QJsonValue raw=100;
        QString source="driver-configured-brightness";
        connect(&server,&QLocalServer::newConnection,this,[&]() {
            auto *socket=server.nextPendingConnection();
            auto bytes=std::make_shared<QByteArray>();
            connect(socket,&QLocalSocket::readyRead,this,[&,socket,bytes]() {
                *bytes+=socket->readAll();
                if(!bytes->contains('\n'))return;
                requests.append(QJsonDocument::fromJson(bytes->trimmed()).object());
                if(hold) {held.append(socket);return;}
                socket->write(QJsonDocument(QJsonObject{{"protocol",1},{"ok",true},
                    {"result",QJsonObject{{"channel","display0"},{"brightness",raw},{"source",source}}}}).toJson(QJsonDocument::Compact)+"\n");
                socket->flush();
            });
        });
        QList<int> values;
        DisplayBrightnessReader reader([&](int value,const QString &) {values.append(value);});
        reader.path=path;reader.serverUid=getuid();
        QTRY_COMPARE(values.size(),1);QCOMPARE(values.last(),100);
        QCOMPARE(requests.last(),QJsonObject({{"protocol",1},{"method","display.brightness.get"},
                                             {"params",QJsonObject{{"channel","display0"}}}}));
        for(const auto &value:QList<QJsonValue>{QString("100"),true,100.5,-1,256})
        {
            raw=value;const int count=values.size();reader.refresh();
            QTRY_COMPARE(values.size(),count+1);QCOMPARE(values.last(),-1);
        }
        raw=0;reader.refresh();QTRY_COMPARE(values.last(),0);
        source="saved-preference";reader.refresh();QTRY_COMPARE(values.last(),-1);
        source="driver-configured-brightness";hold=true;reader.refresh();QTRY_COMPARE(held.size(),1);
        const int before=values.size();
        held[0]->write("{\"protocol\":1,\"ok\":true,\"result\":{\"channel\":\"display0\",\"brightness\":220,\"source\":\"driver-configured-brightness\"}}\n");
        held[0]->flush();reader.setSuspended(true);QTest::qWait(40);QCOMPARE(values.size(),before);
        reader.refresh();QTest::qWait(20);QCOMPARE(values.size(),before);
        hold=false;raw=184;reader.setSuspended(false);QTRY_COMPARE(values.size(),before+1);QCOMPARE(values.last(),184);
        for(const auto &request:requests)QCOMPARE(request["method"].toString(),QString("display.brightness.get"));
    }
    void desktopBrightnessPeerAndTimeoutFailuresStayUnknown()
    {
        QTemporaryDir dir;QLocalServer server;const auto path=dir.filePath("display.sock");QVERIFY(server.listen(path));
        QByteArray received;
        connect(&server,&QLocalServer::newConnection,this,[&]() {
            auto *socket=server.nextPendingConnection();
            connect(socket,&QLocalSocket::readyRead,this,[&,socket]() {received+=socket->readAll();});
        });
        QList<int> values;QString message;
        DisplayBrightnessReader reader([&](int v,const QString &e){values.append(v);message=e;});
        reader.path=path;reader.serverUid=getuid()+1;
        QTRY_COMPARE(values.size(),1);QCOMPARE(values.last(),-1);QVERIFY(message.contains("身份"));QVERIFY(received.isEmpty());
        reader.serverUid=getuid();reader.deadlineMs=50;reader.refresh();
        QTRY_COMPARE(values.size(),2);QCOMPARE(values.last(),-1);QVERIFY(message.contains("超时"));
        QVERIFY(received.contains("display.brightness.get"));
    }
    void desktopBrightnessNeverUsesSavedPreferenceAsHardwareState()
    {
        DesktopV1::DesktopCanvas canvas;
        auto data=snapshot();auto settings=data["settings"].toObject();settings["demo_mode"]=false;settings["brightness"]=220;data["settings"]=settings;
        canvas.applyHmi(data);QCOMPARE(canvas.brightness,-1);
        canvas.observeBrightness(100,QString());QCOMPARE(canvas.brightness,39);
        canvas.applyHmi(data);QCOMPARE(canvas.brightness,39);
        canvas.observeBrightness(-1,"读取屏幕亮度超时");QCOMPARE(canvas.brightness,-1);
        canvas.applyHmi(data);QCOMPARE(canvas.brightness,-1);
        canvas.observeBrightness(0,QString());QCOMPARE(canvas.brightness,0);
    }

    void staleSnapshotCannotReplaceSettingsAfterBrightnessTransaction_data()
    {
        QTest::addColumn<bool>("obsoleteFailure");
        QTest::newRow("late-success") << false;
        QTest::newRow("late-error") << true;
    }
    void staleSnapshotCannotReplaceSettingsAfterBrightnessTransaction()
    {
        QFETCH(bool, obsoleteFailure);
        MainWindow w; isolate(w); w.updateSnapshot(snapshot()); w.refreshTimer.stop();
        w.requestActive = false; // isolate aborted the constructor request without its completion slot.
        QTcpServer server; QVERIFY(server.listen(QHostAddress::LocalHost));
        QList<QPointer<QTcpSocket>> snapshots;
        int writes = 0;
        auto respond = [](QTcpSocket *socket, const QJsonObject &body) {
            const auto bytes = QJsonDocument(body).toJson(QJsonDocument::Compact);
            socket->write("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: " +
                QByteArray::number(bytes.size()) + "\r\n\r\n" + bytes);
            socket->disconnectFromHost();
        };
        connect(&server, &QTcpServer::newConnection, this, [&]() {
            auto *socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket]() {
                auto bytes = socket->property("request").toByteArray() + socket->readAll();
                socket->setProperty("request", bytes);
                const int split = bytes.indexOf("\r\n\r\n"); if (split < 0) return;
                int length = 0;
                for (const auto &line : bytes.left(split).split('\n'))
                    if (line.toLower().startsWith("content-length:")) length = line.mid(15).trimmed().toInt();
                if (bytes.size() < split + 4 + length || socket->property("handled").toBool()) return;
                socket->setProperty("handled", true);
                const auto path = QUrl(QString::fromUtf8(bytes.split(' ')[1])).path();
                if (path.endsWith("/snapshot")) { snapshots.append(socket); return; }
                ++writes;
                const auto body = QJsonDocument::fromJson(bytes.mid(split + 4, length)).object();
                if (path.endsWith("/settings")) respond(socket, {{"ok", true}, {"settings", body}});
                else respond(socket, {{"ok", true}, {"result", QJsonObject{{"brightness", 184}}}});
            });
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        });
        w.networkManager.setProxy(QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", server.serverPort()));
        connect(&w.networkManager, &QNetworkAccessManager::finished, &w, &MainWindow::handleReply);
        QSignalSpy snapshotsApplied(&w, &MainWindow::hmiSnapshotChanged);
        QSignalSpy finished(&w, &MainWindow::desktopBrightnessFinished);
        w.requestSnapshot(); w.requestSnapshot(); w.requestSnapshot();
        QTRY_COMPARE(snapshots.size(), 1); QVERIFY(w.requestActive);
        const int priorDisplay = w.brightnessSlider->value();
        QVERIFY(w.requestDesktopBrightness(72));
        auto duringSave = snapshot(); auto duringSettings = duringSave["settings"].toObject();
        duringSettings["brightness"] = 17; duringSave["settings"] = duringSettings;
        w.updateSnapshot(duringSave); w.refreshTimer.stop();
        QCOMPARE(w.brightnessSlider->value(), priorDisplay);
        snapshotsApplied.clear();
        QTRY_COMPARE(finished.count(), 1); QVERIFY(finished.first()[0].toBool());
        QCOMPARE(writes, 2); w.refreshTimer.stop();
        QVERIFY(snapshots.first());
        respond(snapshots.first(), obsoleteFailure ? QJsonObject{{"ok", false}, {"error", "obsolete failure"}} : snapshot());
        QTRY_COMPARE(snapshots.size(), 2);
        QCOMPARE(snapshotsApplied.count(), 0); // The pre-write reply is obsolete.
        auto current = snapshot(); auto settings = current["settings"].toObject();
        settings["brightness"] = 184; current["settings"] = settings;
        QVERIFY(snapshots.last()); respond(snapshots.last(), current);
        QTRY_COMPARE(snapshotsApplied.count(), 1);
        QCOMPARE(w.brightnessSlider->value(), 184); QVERIFY(!w.requestActive);
        QCOMPARE(writes, 2); w.refreshTimer.stop();
    }

    void reopeningUpdatePanelReattachesWithoutStartingAnotherInstall()
    {
        QTemporaryDir temp; QVERIFY(temp.isValid());
        auto write = [&](const QString &name, const QByteArray &bytes) {
            QFile file(temp.filePath(name));
            return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
        };
        QVERIFY(write("updater", "#!/bin/sh\ncd \"$(dirname \"$0\")\"\nprintf '%s\\t%s\\n' \"$2\" \"$3\" >> requests\nif [ \"$2\" = update.identity ]; then cat identity; else cat result; fi\n"));
        QVERIFY(QFile::setPermissions(temp.filePath("updater"), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        QVERIFY(write("updater.json", "{\"task_service\":true}"));
        QVERIFY(write("identity", "{\"system\":{\"board_id\":\"fixture-board\",\"contract_id\":\"fixture-contract\"},\"installed\":{\"version\":\"0.1.0-dev\"},\"check_configured\":true}"));
        const QString id(32, 'e');
        QJsonObject task{{"task_id", id}, {"operation", "install"}, {"status", "running"}, {"stage", "health"}};
        QVERIFY(write("result", QJsonDocument(task).toJson(QJsonDocument::Compact)));
        auto create = [&]() {
            const bool hadConfig = qEnvironmentVariableIsSet("DSHANCONTROL_UPDATER_CONFIG");
            const bool hadExecutable = qEnvironmentVariableIsSet("DSHANCONTROL_UPDATER_EXECUTABLE");
            const auto config = qgetenv("DSHANCONTROL_UPDATER_CONFIG");
            const auto executable = qgetenv("DSHANCONTROL_UPDATER_EXECUTABLE");
            qputenv("DSHANCONTROL_UPDATER_CONFIG", temp.filePath("updater.json").toLocal8Bit());
            qputenv("DSHANCONTROL_UPDATER_EXECUTABLE", temp.filePath("updater").toLocal8Bit());
            auto *panel = new UpdatePanel;
            if (hadConfig) qputenv("DSHANCONTROL_UPDATER_CONFIG", config); else qunsetenv("DSHANCONTROL_UPDATER_CONFIG");
            if (hadExecutable) qputenv("DSHANCONTROL_UPDATER_EXECUTABLE", executable); else qunsetenv("DSHANCONTROL_UPDATER_EXECUTABLE");
            return panel;
        };
        for (int session = 0; session < 2; ++session) {
            QScopedPointer<UpdatePanel> panel(create());
            QTRY_COMPARE(panel->taskId, id); QVERIFY(panel->taskPoll.isActive());
            QVERIFY(!panel->install->isEnabled()); QVERIFY(!panel->download->isEnabled());
            panel->taskPoll.setInterval(10);
            QTest::qWait(80);
            if (session == 1) {
                task["status"] = "success"; task["stage"] = "completed";
                QVERIFY(write("result", QJsonDocument(task).toJson(QJsonDocument::Compact)));
                QTRY_VERIFY(!panel->taskPoll.isActive());
                QTRY_COMPARE(panel->process.state(), QProcess::NotRunning);
            }
        }
        QFile requests(temp.filePath("requests")); QVERIFY(requests.open(QIODevice::ReadOnly));
        int latestQueries = 0, specificQueries = 0;
        for (const auto &line : requests.readAll().split('\n')) {
            if (line.isEmpty()) continue;
            const auto columns = line.split('\t'); QCOMPARE(columns.size(), 2);
            QVERIFY(columns[0] == "update.identity" || columns[0] == "update.status");
            if (columns[0] == "update.status") {
                const auto params = QJsonDocument::fromJson(columns[1]).object();
                if (params.isEmpty()) ++latestQueries;
                else { QCOMPARE(params["task_id"].toString(), id); ++specificQueries; }
            }
        }
        QCOMPARE(latestQueries, 2); QVERIFY(specificQueries >= 2);
    }

    void desktopBrightnessUsesExistingTransaction()
    {
        MainWindow w; isolate(w); w.updateSnapshot(snapshot()); w.refreshTimer.stop();
        QTcpServer server; QVERIFY(server.listen(QHostAddress::LocalHost));
        QList<QJsonObject> writes; int mode = 0;
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
                const auto method = bytes.split(' ')[0];
                const QString path = QUrl(QString::fromUtf8(bytes.split(' ')[1])).path();
                const auto body = QJsonDocument::fromJson(bytes.mid(split + 4, length)).object();
                QJsonObject response = snapshot();
                if (method != "GET") {
                    writes.append(QJsonObject{{"method", QString::fromUtf8(method)}, {"path", path}, {"body", body}});
                    if (path.endsWith("/settings"))
                        response = mode == 2 ? QJsonObject{{"ok", false}} : QJsonObject{{"ok", true}, {"settings", body}};
                    else {
                        response = {{"ok", true}, {"result", QJsonObject{{"brightness",
                            mode == 1 ? 20 : body["params"].toObject()["value"].toInt()}}}};
                        if (mode == 3) { socket->disconnectFromHost(); return; }
                    }
                }
                const auto output = QJsonDocument(response).toJson();
                socket->write("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: " +
                    QByteArray::number(output.size()) + "\r\n\r\n" + output);
                socket->disconnectFromHost();
            });
        });
        w.networkManager.setProxy(QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", server.serverPort()));
        connect(&w.networkManager, &QNetworkAccessManager::finished, &w, &MainWindow::handleReply);
        QSignalSpy finished(&w, &MainWindow::desktopBrightnessFinished);
        QVERIFY(w.requestDesktopBrightness(72));
        QVERIFY(!w.requestDesktopBrightness(35)); // no second transaction or early completion
        QCOMPARE(finished.count(), 0);
        QTRY_COMPARE(finished.count(), 1); QCOMPARE(writes.size(), 2);
        QVERIFY(finished.at(0).at(0).toBool()); QCOMPARE(finished.at(0).at(1).toInt(), 184);
        QCOMPARE(writes.at(0)["method"].toString(), QString("PUT"));
        QCOMPARE(writes.at(0)["body"].toObject(), QJsonObject({{"brightness", 184}}));
        QCOMPARE(writes.at(1)["body"].toObject()["confirmed"], QJsonValue(true));
        QCOMPARE(writes.at(1)["body"].toObject()["params"].toObject()["value"].toInt(), 184);
        QVERIFY(!w.settingsPending); QVERIFY(!QApplication::activeModalWidget());
        mode = 1; QVERIFY(w.requestDesktopBrightness(50));
        QTRY_COMPARE(finished.count(), 2); QCOMPARE(writes.size(), 4);
        QVERIFY(!finished.at(1).at(0).toBool()); QVERIFY(finished.at(1).at(2).toString().contains("背光应用未确认"));
        mode = 2; QVERIFY(w.requestDesktopBrightness(65));
        QTRY_COMPARE(finished.count(), 3); QCOMPARE(writes.size(), 5); // failed save never applies hardware
        QVERIFY(!finished.at(2).at(0).toBool());
        mode = 3; QVERIFY(w.requestDesktopBrightness(65));
        QTRY_COMPARE(finished.count(), 4); QCOMPARE(writes.size(), 7);
        QVERIFY(!finished.at(3).at(0).toBool()); QTest::qWait(100); QCOMPARE(writes.size(), 7); // no retry
        mode = 0; QVERIFY(w.requestDesktopBrightness(0));
        QTRY_COMPARE(finished.count(), 5); QCOMPARE(finished.at(4).at(1).toInt(), 10);
        QVERIFY(w.requestDesktopBrightness(100));
        QTRY_COMPARE(finished.count(), 6); QCOMPARE(finished.at(5).at(1).toInt(), 255);
        w.brightnessSlider->setValue(99); QVERIFY(w.settingsDirty);
        QVERIFY(!w.requestDesktopBrightness(50)); QCOMPARE(finished.count(), 7);
        QVERIFY(finished.last().at(2).toString().contains("请先保存"));
        w.markOffline(); QVERIFY(!w.requestDesktopBrightness(50));
        QCOMPARE(writes.size(), 11); w.refreshTimer.stop();
    }

    void desktopBrightnessDragAndReadback()
    {
        DesktopV1::DesktopCanvas w(false); w.resize(1024, 600); w.show();
        auto data = snapshot(); auto settings = data["settings"].toObject();
        settings["brightness"] = 220; data["settings"] = settings;
        w.applyHmi(data); w.change(1); QCoreApplication::processEvents();
        QSignalSpy sent(&w, &DesktopV1::DesktopCanvas::brightnessRequested);
        QTest::mousePress(&w, Qt::LeftButton, Qt::NoModifier, QPoint(250, 390));
        QMouseEvent move(QEvent::MouseMove, QPointF(397, 390), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&w, &move);
        QCOMPARE(w.brightness, 72); QCOMPARE(sent.count(), 0);
        w.applyHmi(data); QCOMPARE(w.brightness, 72); // periodic snapshot cannot move the drag thumb
        QTest::mouseRelease(&w, Qt::LeftButton, Qt::NoModifier, QPoint(397, 390));
        QCOMPARE(sent.count(), 1); QCOMPARE(sent.at(0).at(0).toInt(), 72); QVERIFY(w.brightnessBusy());
        w.observeBrightness(100,QString());QCOMPARE(w.brightness,72);
        QTest::mouseClick(&w, Qt::LeftButton, Qt::NoModifier, QPoint(460, 390)); QCOMPARE(sent.count(), 1);
        w.finishBrightness(true, 184, "背光已应用"); QVERIFY(!w.brightnessBusy());
        w.applyHmi(data); QCOMPARE(w.brightness, 72); // stale snapshot
        settings["brightness"] = 184; data["settings"] = settings; w.applyHmi(data);
        w.finishBrightness(false, -1, "显示设置已保存，背光应用未确认");
        w.applyHmi(data); QCOMPARE(w.brightness, -1); // settings is not a hardware readback
        QTest::mousePress(&w, Qt::LeftButton, Qt::NoModifier, QPoint(300, 390));
        QEvent cancel(QEvent::TouchCancel); QApplication::sendEvent(&w, &cancel);
        QTest::mouseRelease(&w, Qt::LeftButton, Qt::NoModifier, QPoint(400, 390));
        QCOMPARE(sent.count(), 1); QCOMPARE(w.brightness, -1);
        QTest::mousePress(&w, Qt::LeftButton, Qt::NoModifier, QPoint(300, 390));
        w.change(0); QTest::mouseRelease(&w, Qt::LeftButton, Qt::NoModifier, QPoint(400, 390));
        QCOMPARE(sent.count(), 1);
        w.applyHmi({}); w.change(1); QTest::mouseClick(&w, Qt::LeftButton, Qt::NoModifier, QPoint(300, 390));
        QCOMPARE(sent.count(), 1);
        w.applyHmi(data); w.resize(1280, 720); QCoreApplication::processEvents();
        QTest::mouseClick(&w, Qt::LeftButton, Qt::NoModifier, QPoint(496, 468));
        QCOMPARE(sent.count(), 2); QCOMPARE(sent.last().at(0).toInt(), 72);
        w.finishBrightness(true, 184, "显示设置已保存，背光已应用");
        QVERIFY(w.grab().save("screenshots/desktop-control-brightness-wide.png"));
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
