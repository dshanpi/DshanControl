#include "desktopclient.h"
#include <QJsonDocument>
#include <QLocalSocket>
#include <QVariant>
#include <memory>
#include <sys/socket.h>

DesktopClient::DesktopClient(QObject *parent) : QObject(parent)
{
    socketPath = qEnvironmentVariable("DSHANCONTROL_DESKTOP_SOCKET",
                                      "/run/dshancontrol/desktop/desktop.sock");
    bool valid = true;
    if (qEnvironmentVariableIsSet("DSHANCONTROL_DESKTOP_UID"))
        serverUid = qEnvironmentVariable("DSHANCONTROL_DESKTOP_UID").toLongLong(&valid);
    if (!valid || serverUid < 0 || serverUid > 2147483647)
        serverUid = -1;
    poll.setInterval(2000);
    connect(&poll, &QTimer::timeout, this, [this]() { request("desktop.snapshot"); });
    poll.start();
    QTimer::singleShot(0, this, [this]() { request("desktop.snapshot"); });
}
void DesktopClient::request(const QString &method, const QJsonObject &params)
{
    if (method == "desktop.snapshot")
    {
        for (const auto &entry : queue)
            if (entry.method == method)
                return;
        if (active || !queue.isEmpty())
            return;
    }
    if (queue.size() >= 16)
    {
        emit feedback("操作队列已满，请等待当前请求结束。");
        return;
    }
    queue.enqueue(Request{method, params});
    next();
}
void DesktopClient::next()
{
    if (active || queue.isEmpty())
        return;
    active = true;
    const auto request = queue.dequeue();
    auto *socket = new QLocalSocket(this);
    socket->setReadBufferSize(524289);
    auto bytes = std::make_shared<QByteArray>();
    auto completed = std::make_shared<bool>(false);
    auto finish = [this, socket, request, completed](const QJsonObject &result,
                                                     const QString &error) {
        if (*completed)
            return;
        *completed = true;
        if (!error.isEmpty())
        {
            if (request.method == "desktop.snapshot")
            {
                state = {{"version", 1}, {"service_connected", false}};
                emit snapshotChanged(state);
            }
            else
                emit feedback(error);
        }
        else if (request.method == "desktop.snapshot")
        {
            state = result;
            state["service_connected"] = true;
            emit snapshotChanged(state);
        }
        else
            emit feedback("请求已受理，正在等待服务结果。");
        socket->abort();
        socket->deleteLater();
        active = false;
        if (!queue.isEmpty())
            QTimer::singleShot(0, this, &DesktopClient::next);
        else if (request.method != "desktop.snapshot")
            QTimer::singleShot(0, this, [this]() { this->request("desktop.snapshot"); });
    };
    connect(socket, &QLocalSocket::connected, this, [this, socket, request, finish]() {
        struct ucred peer;
        socklen_t size = sizeof(peer);
        if (serverUid < 0 ||
            ::getsockopt(int(socket->socketDescriptor()), SOL_SOCKET, SO_PEERCRED, &peer, &size) ||
            peer.uid != uid_t(serverUid))
        {
            finish({}, "服务身份不匹配，操作未发送。");
            return;
        }
        const auto body = QJsonDocument(QJsonObject{{"protocol", 1},
                                                    {"method", request.method},
                                                    {"params", request.params}})
                              .toJson(QJsonDocument::Compact);
        socket->write(body + '\n');
    });
    connect(socket, &QLocalSocket::readyRead, this, [socket, bytes, finish]() {
        *bytes += socket->readAll();
        if (bytes->size() > 524288)
        {
            finish({}, "服务响应过大，状态未更新。");
            return;
        }
        if (!bytes->contains('\n'))
            return;
        QJsonParseError error;
        auto doc = QJsonDocument::fromJson(bytes->trimmed(), &error);
        const auto root = doc.object();
        if (error.error != QJsonParseError::NoError || root["protocol"] != 1 ||
            !root["ok"].isBool() || bytes->count('\n') != 1)
        {
            finish({}, "服务返回的数据无效。");
            return;
        }
        if (root["ok"] == true && root["result"].isObject())
            finish(root["result"].toObject(), {});
        else
        {
            const auto code = root["error"].toObject()["code"].toString();
            static const QMap<QString, QString> messages = {
                {"not-configured", "服务尚未配置，请先完成设备登记或账号授权。"},
                {"not-connected", "云服务尚未连接，请检查网络和服务状态。"},
                {"no-data", "没有可上报的有效传感器读数。"},
                {"not-ready", "语音激活或录放音尚未就绪。"},
                {"stale-or-denied", "设备状态或授权已变化，请同步后再操作。"},
                {"read-only", "当前授权只能查看设备。"},
                {"busy", "上一请求正在处理，请稍后再试。"}};
            finish({}, messages.value(code, "操作未完成，请刷新状态查看原因。"));
        }
    });
    connect(socket, QOverload<QLocalSocket::LocalSocketError>::of(&QLocalSocket::error), this,
            [finish](QLocalSocket::LocalSocketError) {
                finish({}, "无法连接接入服务；本次操作未确认，请刷新状态，勿重复发送控制。");
            });
    connect(socket, &QLocalSocket::disconnected, this, [completed, finish]() {
        if (!*completed)
            finish({}, "连接已中断，本次操作结果待确认。");
    });
    QTimer::singleShot(4000, socket,
                       [finish]() { finish({}, "服务响应超时；控制结果待确认，请刷新状态。"); });
    socket->connectToServer(socketPath);
}
