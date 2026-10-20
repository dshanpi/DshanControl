#pragma once
#include "model.h"
#include "mqtt.h"
#include <QLocalSocket>
#include <QNetworkAccessManager>
#include <QSet>
#include <functional>

class DesktopProviders : public QObject
{
    Q_OBJECT
  public:
    explicit DesktopProviders(const QJsonObject &config, QObject *parent = nullptr);
    QJsonObject snapshot(bool privileged) const;
    QJsonObject dispatch(const QString &method, const QJsonObject &params);
  signals:
    void changed();

  private:
    QJsonObject config, reporting, cloud, home, voice;
    Desktop::Pairing pairing;
    Desktop::Points points;
    QNetworkAccessManager network;
    MqttConnection cloudMqtt, sensorMqtt;
    QLocalSocket voiceSocket;
    QByteArray voiceBuffer, cloudSecret, sensorSecret, homeToken;
    QTimer tick;
    QSet<QString> jobs, commandIds;
    QStringList commandOrder;
    QJsonArray devices, history;
    QMap<QString, QJsonArray> functions;
    QMap<QString, qint64> functionTimes;
    QMap<QString, QJsonObject> pending;
    qint64 nextCloudConnect = 0, nextSensorConnect = 0, nextLocalPoll = 0;
    qint64 lastReportAttempt = 0, lastHomeSync = 0, voiceHeartbeat = 0, voiceStopAt = 0;
    qint64 revision = 0, localSequence = 0, lastLocalTimestamp = 0;
    int reportPacket = 0;
    QString stateDirectory;
    void request(const QString &job, const QUrl &url, const QByteArray &method,
                 const QByteArray &body, const QByteArray &contentType, const QByteArray &token,
                 std::function<void(const QJsonObject &, const QString &)> done);
    void refreshPairing();
    void refreshHome();
    void fetchFunctions(const QString &id);
    void command(const QJsonObject &params);
    void report();
    void pollLocal();
    void update();
    void handleVoice();
    void voiceCommand(const QString &command);
    void voiceUnavailable();
};
