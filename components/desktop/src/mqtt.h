#pragma once
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QSslSocket>
#include <QTimer>

class MqttConnection : public QObject
{
    Q_OBJECT
  public:
    explicit MqttConnection(QObject *parent = nullptr);
    void start(const QJsonObject &configuration, const QString &clientId, const QString &username,
               const QByteArray &password, const QStringList &topics);
    int publish(const QString &topic, const QByteArray &payload);
    bool online() const { return ready; }
    bool connecting() const { return active && !ready; }
    void stop();
  signals:
    void connected();
    void failed(const QString &code);
    void message(const QString &topic, const QByteArray &payload, bool retained);
    void acknowledged(int packetId);
    void unconfirmed(int packetId);

  private:
    QSslSocket socket;
    QTimer timer;
    QByteArray buffer, connectPacket;
    QStringList subscriptions;
    QMap<int, qint64> pending;
    QMap<int, QString> subscribing;
    int nextId = 0;
    qint64 began = 0, pingAt = 0, lastTx = 0;
    bool ready = false, active = false;
    int allocateId();
    void receive();
    void frame(unsigned char header, const QByteArray &payload);
    void send(unsigned char header, const QByteArray &payload);
    void fail(const QString &code);
};
