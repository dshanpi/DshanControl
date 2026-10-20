#include <QtWidgets>
#include <QtNetwork>
#include <QtTest>
#include <memory>
#include <unistd.h>
#include "../../components/hmi/src/cloudpanel.h"
#include "../../components/hmi/src/desktopclient.h"

class HmiUiTest : public QObject
{
    Q_OBJECT
    QTemporaryDir temporary;
    QLocalServer server;
    QJsonObject state;
    QList<QJsonObject> requests;
    std::unique_ptr<DesktopClient> client;
    std::unique_ptr<CloudPanel> panel;
    bool dropSend = false;

    QJsonObject point() const {
        return {{"point_id","sensor/temperature"},{"name","温度"},{"type","temperature"},
                {"unit","°C"},{"value",24.3},{"source","MQTT"},{"quality","good"},
                {"timestamp",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
    }
    QJsonObject snapshot() const {
        return {{"version",1},{"service_connected",true},
                {"cloud",QJsonObject{{"configured",true},{"mqtt","connected"}}},
                {"reporting",QJsonObject{{"persistent",true},{"enabled",false},
                 {"period_seconds",30},{"inflight",false},{"points",QJsonArray{point()}}}}};
    }
    QPushButton *sendButton() const {
        for (auto *b : panel->findChildren<QPushButton *>())
            if (b->text()==QString::fromUtf8("立即上报一次")) return b;
        return nullptr;
    }
    int count(const QString &method) const {
        int n=0;for(const auto &r:requests)if(r["method"]==method)++n;return n;
    }
    void apply() { panel->apply(state);QCoreApplication::processEvents(); }
  private slots:
    void init() {
        QVERIFY(temporary.isValid());requests.clear();dropSend=false;state=snapshot();
        QVERIFY(server.listen(temporary.filePath("reporting.sock")));
        connect(&server,&QLocalServer::newConnection,this,[this]() {
            auto *s=server.nextPendingConnection();
            connect(s,&QLocalSocket::readyRead,s,[this,s]() {
                auto bytes=s->property("bytes").toByteArray()+s->readAll();
                s->setProperty("bytes",bytes);if(!bytes.contains('\n'))return;
                const auto request=QJsonDocument::fromJson(bytes.trimmed()).object();requests<<request;
                if(dropSend && request["method"]=="reporting.send") {s->abort();return;}
                s->write(QJsonDocument(QJsonObject{{"protocol",1},{"ok",true},{"result",state}})
                             .toJson(QJsonDocument::Compact)+'\n');s->disconnectFromServer();
            });
            connect(s,&QLocalSocket::disconnected,s,&QObject::deleteLater);
        });
        client.reset(new DesktopClient);client->poll.stop();
        client->socketPath=temporary.filePath("reporting.sock");client->serverUid=geteuid();
        panel.reset(new CloudPanel(client.get()));panel->resize(1024,600);panel->selectSection(1);panel->show();
        QTRY_VERIFY(count("desktop.snapshot")>0 && !client->active);
        QVERIFY(sendButton());QVERIFY(sendButton()->isEnabled());requests.clear();
    }
    void cleanup() {
        panel.reset();client.reset();server.close();disconnect(&server,nullptr,this,nullptr);
        for(auto *s:server.findChildren<QLocalSocket *>())delete s;
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
    }
    void unavailableInputsCannotSend_data() {
        QTest::addColumn<int>("reason");
        const char *names[]={"service-offline","broker-offline","inflight","empty","stale-quality",
            "expired-time","missing-value","duplicate-id","untrusted-source","bad-unit","too-many-points"};
        for(int i=0;i<11;++i)QTest::newRow(names[i])<<i;
    }
    void unavailableInputsCannotSend() {
        QFETCH(int,reason);auto r=state["reporting"].toObject();auto p=point();
        if(reason==0)state["service_connected"]=false;
        else if(reason==1)state["cloud"]=QJsonObject{{"configured",true},{"mqtt","offline"}};
        else if(reason==2)r["inflight"]=true;
        else if(reason==3)r["points"]=QJsonArray{};
        else {
            if(reason==4)p["quality"]="stale";
            if(reason==5)p["timestamp"]=QDateTime::currentDateTimeUtc().addSecs(-61).toString(Qt::ISODateWithMs);
            if(reason==6)p["value"]=QJsonValue();
            if(reason==8)p["source"]="fixture";
            if(reason==9)p["unit"]="unknown-unit";
            QJsonArray points{p};if(reason==7)points.append(p);
            if(reason==10)for(int i=0;i<128;++i)points.append(p);
            r["points"]=points;
        }
        state["reporting"]=r;apply();QVERIFY(!sendButton()->isEnabled());
        QTest::mouseClick(sendButton(),Qt::LeftButton);QTest::qWait(40);
        QCOMPARE(count("reporting.send"),0);QCOMPARE(count("reporting.configure"),0);
        auto *table=panel->findChild<QTableWidget *>("cloudPoints");QVERIFY(table);
        if(reason==0 || reason==3 || reason==10)QCOMPARE(table->rowCount(),0);
        if(reason>=4 && reason<10)for(int i=0;i<table->rowCount();++i)QCOMPARE(table->item(i,1)->text(),QString("--"));
        state=snapshot();apply();QVERIFY(sendButton()->isEnabled());
        QTest::mouseClick(sendButton(),Qt::LeftButton);QTRY_COMPARE(count("reporting.send"),1);
        QTest::qWait(60);QCOMPARE(count("reporting.send"),1);
    }
    void settingsRefreshDoesNotWriteAndActivationSendsOnce() {
        auto r=state["reporting"].toObject();r["enabled"]=true;r["period_seconds"]=60;state["reporting"]=r;
        for(int i=0;i<4;++i)apply();QTest::qWait(50);QCOMPARE(count("reporting.configure"),0);
        auto *enabled=panel->findChild<QCheckBox *>("reportingEnabled");
        auto *period=panel->findChild<QComboBox *>("reportingPeriod");QVERIFY(enabled);QVERIFY(period);
        QVERIFY(enabled->isChecked());QCOMPARE(period->currentData().toInt(),60);
        QTest::mouseClick(enabled,Qt::LeftButton);QTRY_COMPARE(count("reporting.configure"),1);
        QJsonObject params;for(const auto &r:requests)if(r["method"]=="reporting.configure")params=r["params"].toObject();
        QCOMPARE(params,QJsonObject({{"enabled",false},{"period_seconds",60}}));
        QTRY_VERIFY(!client->active);QTest::qWait(50);QCOMPARE(count("reporting.configure"),1);
        r["persistent"]=false;state["reporting"]=r;apply();QVERIFY(!enabled->isEnabled());QVERIFY(!period->isEnabled());
        QTest::mouseClick(enabled,Qt::LeftButton);QTest::qWait(50);QCOMPARE(count("reporting.configure"),1);
    }
    void lostSendResponseIsNotReplayed() {
        dropSend=true;QSignalSpy feedback(client.get(),&DesktopClient::feedback);
        QTest::mouseClick(sendButton(),Qt::LeftButton);QTRY_COMPARE(count("reporting.send"),1);
        QTRY_VERIFY(!feedback.isEmpty());QTest::qWait(250);QCOMPARE(count("reporting.send"),1);
        QCOMPARE(count("reporting.configure"),0);
    }
};
QTEST_MAIN(HmiUiTest)
#include "cloud_reporting_boundaries_test.moc"
