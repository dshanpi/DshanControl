#include "mqtt.h"
#include "model.h"
#include <QFile>
#include <QNetworkProxy>
#include <QSslCertificate>
#include <QSslConfiguration>

namespace
{
QByteArray word(int value)
{
    QByteArray out;
    out += char(value >> 8);
    out += char(value);
    return out;
}
int integer(const QByteArray &data, int offset)
{
    require(offset >= 0 && offset + 2 <= data.size(), "Truncated MQTT integer", "mqtt");
    return (static_cast<unsigned char>(data[offset]) << 8) |
           static_cast<unsigned char>(data[offset + 1]);
}
QByteArray string(const QByteArray &value)
{
    require(!value.isEmpty() && value.size() <= 65535 && !value.contains('\0'),
            "Invalid MQTT string", "mqtt");
    return word(value.size()) + value;
}
bool topicValid(const QString &topic)
{
    return !topic.isEmpty() && topic.toUtf8().size() <= 512 && !topic.contains(QChar(0)) &&
           !topic.contains('+') && !topic.contains('#') && !topic.contains('\n');
}
} // namespace
MqttConnection::MqttConnection(QObject *parent) : QObject(parent)
{
    socket.setProxy(QNetworkProxy::NoProxy);
    socket.setReadBufferSize(131072);
    connect(&socket, &QSslSocket::encrypted, this, [this]() {
        if (!active)
            return;
        socket.write(connectPacket);
        connectPacket.fill(0);
        connectPacket.clear();
        lastTx = Desktop::monotonicMs();
    });
    connect(&socket, &QSslSocket::readyRead, this, &MqttConnection::receive);
    connect(&socket, QOverload<QAbstractSocket::SocketError>::of(&QSslSocket::error), this,
            [this](QAbstractSocket::SocketError) { fail("transport-error"); });
    connect(&socket, QOverload<const QList<QSslError> &>::of(&QSslSocket::sslErrors), this,
            [this](const QList<QSslError> &) { fail("tls-verification"); });
    connect(&socket, &QSslSocket::disconnected, this, [this]() { fail("disconnected"); });
    timer.setInterval(1000);
    connect(&timer, &QTimer::timeout, this, [this]() {
        const auto now = Desktop::monotonicMs();
        if (!ready && now - began > 15000)
        {
            fail("connection-timeout");
            return;
        }
        if (pingAt && now - pingAt > 10000)
        {
            fail("keepalive-timeout");
            return;
        }
        for (auto it = pending.begin(); it != pending.end(); ++it)
            if (now - it.value() > 30000)
            {
                fail("puback-timeout");
                return;
            }
        if (ready && !pingAt && now - lastTx >= 15000)
        {
            pingAt = now;
            send(0xc0, {});
        }
    });
}
void MqttConnection::start(const QJsonObject &config, const QString &client,
                           const QString &username, const QByteArray &password,
                           const QStringList &topics)
{
    require(!active, "MQTT already active", "mqtt");
    const auto host = config["host"].toString();
    require(config["tls"] == true && !host.isEmpty() && host.size() <= 253 && !host.contains('/') &&
                !host.contains(QChar(0)) && positiveInteger(config["port"]) &&
                config["port"].toDouble() <= 65535 && topics.size() <= 32,
            "Explicit TLS MQTT endpoint required", "configuration");
    for (const auto &topic : topics)
        require(topicValid(topic), "Exact MQTT topic required", "configuration");
    auto ssl = QSslConfiguration::defaultConfiguration();
    ssl.setPeerVerifyMode(QSslSocket::VerifyPeer);
    ssl.setProtocol(QSsl::TlsV1_2OrLater);
    if (config.contains("ca_file"))
    {
        const auto certificates =
            QSslCertificate::fromData(readBytes(config["ca_file"].toString(), 1024 * 1024));
        require(!certificates.isEmpty(), "MQTT CA unavailable", "configuration");
        auto all = ssl.caCertificates();
        all.append(certificates);
        ssl.setCaCertificates(all);
    }
    require(password.size() <= 1024 && !password.isEmpty(), "MQTT credentials required",
            "configuration");
    QByteArray body = string("MQTT") + QByteArray::fromHex("04c2001e") + string(client.toUtf8()) +
                      string(username.toUtf8()) + word(password.size()) + password;
    // Clean session, user/password, no subscriptions to cloud command/OTA topics.
    QByteArray length;
    int remaining = body.size();
    do
    {
        int byte = remaining % 128;
        remaining /= 128;
        if (remaining)
            byte |= 128;
        length += char(byte);
    } while (remaining);
    connectPacket = QByteArray(1, char(0x10)) + length + body;
    body.fill(0);
    subscriptions = topics;
    ready = false;
    active = true;
    buffer.clear();
    pending.clear();
    subscribing.clear();
    pingAt = 0;
    began = lastTx = Desktop::monotonicMs();
    timer.start();
    socket.setSslConfiguration(ssl);
    socket.connectToHostEncrypted(host, quint16(config["port"].toInt()));
}
void MqttConnection::stop()
{
    if (ready)
        send(0xe0, {});
    fail("stopped");
}
void MqttConnection::fail(const QString &code)
{
    if (!active)
        return;
    active = false;
    ready = false;
    timer.stop();
    connectPacket.fill(0);
    connectPacket.clear();
    buffer.clear();
    const auto outstanding = pending.keys();
    pending.clear();
    subscribing.clear();
    socket.abort();
    for (int id : outstanding)
        emit unconfirmed(id);
    emit failed(code);
}
int MqttConnection::allocateId()
{
    require(pending.size() + subscribing.size() < 64, "MQTT inflight limit", "mqtt");
    do
    {
        nextId = nextId % 65535 + 1;
    } while (pending.contains(nextId) || subscribing.contains(nextId));
    return nextId;
}
void MqttConnection::send(unsigned char header, const QByteArray &body)
{
    require(body.size() <= 65536, "MQTT packet too large", "mqtt");
    QByteArray length;
    int count = body.size();
    do
    {
        int byte = count % 128;
        count /= 128;
        if (count)
            byte |= 128;
        length += char(byte);
    } while (count);
    socket.write(QByteArray(1, char(header)) + length + body);
    lastTx = Desktop::monotonicMs();
}
int MqttConnection::publish(const QString &topic, const QByteArray &payload)
{
    require(ready && topicValid(topic) && payload.size() <= 60000 && pending.size() < 16,
            "MQTT publish unavailable", "mqtt", "not-connected");
    const int id = allocateId();
    pending[id] = Desktop::monotonicMs();
    send(0x32, string(topic.toUtf8()) + word(id) + payload);
    return id;
}
void MqttConnection::receive()
{
    if (!active)
    {
        socket.readAll();
        return;
    }
    buffer += socket.readAll();
    try
    {
        require(buffer.size() <= 131072, "MQTT receive limit", "mqtt");
        while (buffer.size() >= 2)
        {
            int count = 0, multiplier = 1, offset = 1, digits = 0;
            bool complete = false;
            while (offset < buffer.size() && digits < 4)
            {
                const auto byte = static_cast<unsigned char>(buffer[offset++]);
                ++digits;
                count += (byte & 127) * multiplier;
                if (!(byte & 128))
                {
                    complete = true;
                    break;
                }
                multiplier *= 128;
            }
            require(complete || digits < 4, "Invalid MQTT remaining length", "mqtt");
            require(count <= 65536, "MQTT frame limit", "mqtt");
            if (!complete || buffer.size() < offset + count)
                return;
            const auto header = static_cast<unsigned char>(buffer[0]);
            auto payload = buffer.mid(offset, count);
            buffer.remove(0, offset + count);
            frame(header, payload);
            if (!active)
                return;
        }
    }
    catch (const Failure &)
    {
        fail("protocol-error");
    }
}
void MqttConnection::frame(unsigned char header, const QByteArray &body)
{
    if (!ready)
    {
        require(header == 0x20 && body.size() == 2 && body[0] == 0 && body[1] == 0,
                "MQTT connection refused or invalid", "mqtt");
        ready = true;
        for (const auto &topic : subscriptions)
        {
            const int id = allocateId();
            subscribing[id] = topic;
            send(0x82, word(id) + string(topic.toUtf8()) + QByteArray(1, char(1)));
        }
        emit connected();
        return;
    }
    if (header == 0x40)
    {
        require(body.size() == 2 && pending.contains(integer(body, 0)), "Unknown MQTT PUBACK",
                "mqtt");
        int id = integer(body, 0);
        pending.remove(id);
        emit acknowledged(id);
    }
    else if (header == 0x90)
    {
        const int id = integer(body, 0);
        require(body.size() == 3 && subscribing.contains(id) && (body[2] == 0 || body[2] == 1),
                "MQTT subscription refused", "mqtt");
        subscribing.remove(id);
    }
    else if (header == 0xd0)
    {
        require(body.isEmpty() && pingAt, "Unexpected MQTT PINGRESP", "mqtt");
        pingAt = 0;
    }
    else if ((header >> 4) == 3)
    {
        const int qos = (header >> 1) & 3;
        require(qos <= 1 && !(qos == 0 && (header & 8)), "Unsupported MQTT publish", "mqtt");
        const int length = integer(body, 0), offset = 2 + length + (qos ? 2 : 0);
        require(length > 0 && length <= 512 && offset <= body.size(), "Invalid MQTT topic length",
                "mqtt");
        const auto raw = body.mid(2, length);
        const auto topic = QString::fromUtf8(raw);
        require(topic.toUtf8() == raw && subscriptions.contains(topic), "Unsubscribed MQTT topic",
                "mqtt");
        const int id = qos ? integer(body, 2 + length) : 0;
        require(!qos || id != 0, "Invalid MQTT identifier", "mqtt");
        emit message(topic, body.mid(offset), bool(header & 1));
        if (qos)
            send(0x40, word(id));
    }
    else
        throw Failure("unexpected-packet", "mqtt", "Unexpected MQTT packet");
}
