#pragma once
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QPointer>
#include <QTimer>
#include <cmath>
#include <functional>
#include <memory>
#include <sys/socket.h>

// Read-only native platform observation. A saved preference is not evidence of
// the current backlight. Writes continue through the existing confirmed action.
class DisplayBrightnessReader : public QObject
{
    friend class HmiUiTest;
    QTimer poll;
    QPointer<QLocalSocket> pending;
    QString path;
    quint64 generation = 0;
    bool suspended = false, active = false;
    uid_t serverUid = 0;
    int deadlineMs = 1500;
    std::function<void(int, const QString &)> observed;

    void cancel()
    {
        ++generation;
        auto *socket = pending.data();
        pending = nullptr;
        if (socket)
        {
            disconnect(socket, nullptr, this, nullptr);
            socket->abort();
            socket->deleteLater();
        }
    }

  public:
    DisplayBrightnessReader(std::function<void(int, const QString &)> callback,
                            QObject *parent = nullptr)
        : QObject(parent), poll(this),
          path(qEnvironmentVariable("DSHANCONTROL_NATIVE_PLATFORM_SOCKET")),
          observed(std::move(callback))
    {
        poll.setInterval(2000);
        connect(&poll, &QTimer::timeout, this, &DisplayBrightnessReader::refresh);
        QTimer::singleShot(0, this, &DisplayBrightnessReader::refresh);
    }
    ~DisplayBrightnessReader() override { cancel(); }
    void setActive(bool value)
    {
        active = value;
        if (active)
        {
            poll.start();
            refresh();
        }
        else
            poll.stop();
    }
    void setSuspended(bool value)
    {
        suspended = value;
        if (value)
            cancel();
        else
            refresh();
    }
    void refresh()
    {
        if (suspended || pending)
            return;
        if (path.isEmpty() || !QDir::isAbsolutePath(path) || QDir::cleanPath(path) != path ||
            path.contains(QChar(0)))
        {
            observed(-1, "当前屏幕亮度不可用");
            return;
        }
        auto *socket = new QLocalSocket(this);
        pending = socket;
        socket->setReadBufferSize(16385);
        const auto epoch = generation;
        auto bytes = std::make_shared<QByteArray>();
        auto finish = [this, socket, epoch](int raw, const QString &error) {
            if (generation != epoch || pending != socket)
                return;
            pending = nullptr;
            disconnect(socket, nullptr, this, nullptr);
            socket->abort();
            socket->deleteLater();
            observed(raw, error);
        };
        connect(socket, &QLocalSocket::connected, this, [this, socket, finish]() {
            struct ucred peer;
            socklen_t size = sizeof(peer);
            if (::getsockopt(int(socket->socketDescriptor()), SOL_SOCKET, SO_PEERCRED, &peer,
                             &size) != 0 ||
                peer.uid != serverUid)
            {
                finish(-1, "亮度服务身份不匹配");
                return;
            }
            socket->write("{\"protocol\":1,\"method\":\"display.brightness.get\","
                          "\"params\":{\"channel\":\"display0\"}}\n");
        });
        connect(socket, &QLocalSocket::readyRead, this, [socket, bytes, finish]() {
            *bytes += socket->readAll();
            if (bytes->size() > 16384)
            {
                finish(-1, "亮度服务响应无效");
                return;
            }
            if (!bytes->contains('\n'))
                return;
            QJsonParseError error;
            const auto document = QJsonDocument::fromJson(bytes->trimmed(), &error);
            const auto envelope = document.object();
            const auto result = envelope["result"].toObject();
            const auto value = result["brightness"];
            if (error.error != QJsonParseError::NoError || bytes->count('\n') != 1 ||
                envelope["protocol"] != 1 || envelope["ok"] != true ||
                result["channel"] != "display0" ||
                result["source"] != "driver-configured-brightness" || !value.isDouble() ||
                !std::isfinite(value.toDouble()) ||
                value.toDouble() != std::floor(value.toDouble()) || value.toDouble() < 0 ||
                value.toDouble() > 255)
            {
                finish(-1, "未获得驱动确认的屏幕亮度");
                return;
            }
            finish(value.toInt(), QString());
        });
        connect(socket, QOverload<QLocalSocket::LocalSocketError>::of(&QLocalSocket::error), this,
                [finish](QLocalSocket::LocalSocketError) { finish(-1, "当前屏幕亮度不可用"); });
        connect(socket, &QLocalSocket::disconnected, this,
                [finish]() { finish(-1, "亮度服务连接已断开"); });
        QTimer::singleShot(deadlineMs, socket, [finish]() { finish(-1, "读取屏幕亮度超时"); });
        socket->connectToServer(path);
    }
};
