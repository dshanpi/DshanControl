#include "../../components/desktop/src/model.h"
#include <QDateTime>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace Desktop;
class DesktopModelTest : public QObject {
    Q_OBJECT
    QJsonObject response(bool bound=false,const QString &code="482719",int ttl=300) {
        return {{"success",true},{"data",QJsonObject{{"deviceId","unit-s4"},{"bound",bound},
            {"bindToken",bound?QString():code},{"serverTime",1800000000},{"bindTokenExpiresAt",bound?0:1800000000+ttl}}}};
    }
    QJsonObject sample(int sequence=1,const QString &quality="good") {
        return {{"schema_version",1},{"device_id","sensor-1"},{"sequence",sequence},{"quality",quality},
            {"timestamp","2027-01-15T08:00:00Z"},{"values",QJsonObject{{"temperature",24300},{"humidity",52600}}}};
    }
    QJsonObject mapping() {
        return {{"temperature",QJsonObject{{"name","环境温度"},{"type","temperature"},{"scale",0.001},{"unit","°C"}}},
                {"humidity",QJsonObject{{"name","环境湿度"},{"type","humidity"},{"scale",0.001},{"unit","%"}}}};
    }
    template<class F>bool refused(F f){try{f();return false;}catch(const Failure &){return true;}}
private slots:
    void pairingUsesExplicitOwnershipAndConservativeMonotonicExpiry(){
        Pairing p;p.deviceId="unit-s4";p.apply(response(),1500,10000);
        QCOMPARE(p.ownership,QString("unbound"));QCOMPARE(p.snapshot(10000)["expires_in"].toInt(),298);
        auto legacy=QJsonObject{{"success",true},{"data",QJsonObject{{"deviceId","unit-s4"},{"bindToken","123456"}}}};
        p.apply(legacy,0,10001);QCOMPARE(p.ownership,QString("unknown"));QVERIFY(p.snapshot(10001)["code"].toString().isEmpty());
        p.apply(response(true),0,20000);QCOMPARE(p.ownership,QString("bound"));QVERIFY(p.lastCode.isEmpty());
    }
    void repeatedCodeCannotExtendOrReviveExpiry(){
        Pairing p;p.deviceId="unit-s4";p.apply(response(false,"482719",10),0,1000);
        p.apply(response(false,"482719",300),0,4000);QCOMPARE(p.deadline,qint64(11000));
        p.apply(response(false,"482719",300),0,12000);QVERIFY(p.snapshot(12000)["code"].toString().isEmpty());
        p.apply(response(false,"837209",300),0,12001);QCOMPARE(p.snapshot(12001)["expires_in"].toInt(),300);
    }
    void pairingRejectsMixedIdentityInvalidCodeAndExcessiveTtl(){
        Pairing p;p.deviceId="unit-s4";
        QVERIFY(refused([&]{p.apply(response(false,"12345x"),0,0);}));
        QVERIFY(refused([&]{p.apply(response(false,"123456",601),0,0);}));
        auto value=response();auto data=value["data"].toObject();data["deviceId"]="other-unit";value["data"]=data;
        QVERIFY(refused([&]{p.apply(value,0,0);}));QCOMPARE(p.ownership,QString("unknown"));
        QVERIFY(refused([&]{strictJson("{\"bound\":true,\"bound\":false}");}));
    }
    void failedBoundResponseNeverTurnsIntoUnbound(){
        Pairing p;p.deviceId="unit-s4";p.apply(response(true),0,10);
        auto bad=response();auto data=bad["data"].toObject();data["bound"]=true;bad["data"]=data;
        QVERIFY(refused([&]{p.apply(bad,0,11);}));QCOMPARE(p.ownership,QString("bound"));QVERIFY(!p.fresh);
    }
    void sensorQualitySequenceTimeAndAtomicMapping(){
        Points p;const auto utc=QDateTime::fromString("2027-01-15T08:00:00Z",Qt::ISODate).toSecsSinceEpoch();
        p.ingest("sensor-1",sample(),mapping(),1000,utc,"MQTT");QCOMPARE(p.points.size(),2);
        const auto items=p.cloudPayload(1000)["data"].toArray();QCOMPARE(items.size(),2);
        QCOMPARE(p.points["sensor-1/temperature"].data["value"].toDouble(),24.3);
        QVERIFY(refused([&]{p.ingest("sensor-1",sample(),mapping(),2000,utc,"MQTT");}));
        QVERIFY(refused([&]{p.ingest("sensor-1",sample(2,"uncertain"),mapping(),2000,utc,"MQTT");}));
        auto bad=sample(2);auto values=bad["values"].toObject();values["unauthorized"]=1;bad["values"]=values;
        QVERIFY(refused([&]{p.ingest("sensor-1",bad,mapping(),2000,utc,"MQTT");}));QCOMPARE(p.sequences["sensor-1"],1.0);
        QVERIFY(refused([&]{p.ingest("sensor-1",sample(2),mapping(),2000,utc+61,"MQTT");}));
        QVERIFY(p.cloudPayload(61000)["data"].toArray().isEmpty());
        for(const auto &entry:p.snapshot(61000)){QVERIFY(entry.toObject()["value"].isNull());QCOMPARE(entry.toObject()["quality"].toString(),QString("stale"));}
    }
    void capabilitiesEnforceActualTypesRangesAndReadOnly(){
        auto root=QJsonObject{{"control_allowed",true},{"functions",QJsonArray{QJsonObject{{"code","brightness"},{"type","Integer"},{"values","{\"min\":10,\"max\":100,\"step\":5}"}}}}};
        auto f=validateFunctions(root).first().toObject();validateCommand(f,60);
        QVERIFY(refused([&]{validateCommand(f,61);}));QVERIFY(refused([&]{validateCommand(f,101);}));
        f["writable"]=false;QVERIFY(refused([&]{validateCommand(f,60);}));
        f={{"type","Enum"},{"writable",true},{"values",QJsonObject{{"range",QJsonArray{"auto","sleep"}}}}};
        validateCommand(f,"sleep");QVERIFY(refused([&]{validateCommand(f,"unsupported");}));
    }
    void unknownDeviceStateStaysUnknown(){
        auto root=QJsonObject{{"devices",QJsonArray{QJsonObject{{"id","lamp"},{"name","书房台灯"},{"online",false},{"status",QJsonArray{}}}}}};
        auto d=validateDevices(root).first().toObject();QVERIFY(reportedValue(d,"switch").isUndefined());
        root["devices"]=QJsonArray{d,d};QVERIFY(refused([&]{validateDevices(root);}));
    }
    void privateCredentialsRejectSymlinksAndGroupReadableFiles(){
        QTemporaryDir temp;QVERIFY(temp.isValid());const auto path=temp.filePath("secret");QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write("fixture-only");file.close();
        QVERIFY(QFile::setPermissions(path,QFile::ReadOwner|QFile::WriteOwner));QCOMPARE(privateBytes(path,32),QByteArray("fixture-only"));
        QVERIFY(QFile::link(path,temp.filePath("link")));QVERIFY(refused([&]{privateBytes(temp.filePath("link"),32);}));
        QVERIFY(QFile::setPermissions(path,QFile::ReadOwner|QFile::ReadGroup));QVERIFY(refused([&]{privateBytes(path,32);}));
    }
};
QTEST_GUILESS_MAIN(DesktopModelTest)
#include "model_test.moc"
