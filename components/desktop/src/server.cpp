#include "server.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

DesktopServer::DesktopServer(const QString &path, const QJsonObject &configuration)
    : providers(configuration, this), socketPath(path)
{
    require(::geteuid() != 400 && ::geteuid() != 401,
            "Desktop provider must run separately from Web and HMI identities", "authorization");
    require(QDir::isAbsolutePath(path) && QDir::cleanPath(path) == path &&
                !path.contains(QChar(0)) && !QFileInfo(path).isSymLink(),
            "Invalid desktop socket", "configuration");
    const QString parent = QFileInfo(path).absolutePath();
    const gid_t group = ::geteuid() == 0 ? 400 : ::getegid();
    const bool exists = QFileInfo::exists(parent);
    require(!QFileInfo(parent).isSymLink() && QDir().mkpath(parent),
            "Cannot create desktop socket directory", "configuration");
    struct stat st;
    require(::lstat(QFile::encodeName(parent).constData(), &st) == 0 && S_ISDIR(st.st_mode) &&
                st.st_uid == ::geteuid(),
            "Owned desktop socket directory required", "authorization");
    if (!exists)
    {
        require(::chmod(QFile::encodeName(parent).constData(), 0750) == 0 &&
                    ::chown(QFile::encodeName(parent).constData(), ::geteuid(), group) == 0,
                "Cannot protect desktop IPC directory");
    }
    else
        require(!(st.st_mode & 0022) && st.st_gid == group,
                "Desktop IPC directory is writable or wrong group", "authorization");
    ownership.reset(new FileLock(parent + "/.desktop.lock"));
    if (QFileInfo::exists(path))
    {
        require(::lstat(QFile::encodeName(path).constData(), &st) == 0 && S_ISSOCK(st.st_mode) &&
                    st.st_uid == ::geteuid(),
                "Existing desktop path is not an owned socket", "resource-lock");
        QLocalSocket probe;
        probe.connectToServer(path);
        require(!probe.waitForConnected(300) &&
                    probe.error() == QLocalSocket::ConnectionRefusedError,
                "Desktop socket is live or unidentifiable", "resource-lock");
        require(QFile::remove(path), "Cannot remove stale owned desktop socket");
    }
    require(listen(path), "Cannot listen on desktop socket", "service");
    require(::chmod(QFile::encodeName(path).constData(), 0660) == 0 &&
                ::chown(QFile::encodeName(path).constData(), ::geteuid(), group) == 0,
            "Cannot protect desktop socket", "service");
    setMaxPendingConnections(16);
    connect(this, &QLocalServer::newConnection, this, [this]() {
        while (hasPendingConnections())
        {
            auto *socket = nextPendingConnection();
            if (clients >= 16)
            {
                socket->abort();
                socket->deleteLater();
                continue;
            }
            ++clients;
            socket->setReadBufferSize(65537);
            auto bytes = std::make_shared<QByteArray>();
            auto finished = std::make_shared<bool>(false);
            QTimer::singleShot(5000, socket, [socket]() { socket->abort(); });
            connect(socket, &QLocalSocket::disconnected, this, [this, socket]() {
                --clients;
                socket->deleteLater();
            });
            auto ready = [this, socket, bytes, finished]() {
                if (*finished)
                    return;
                *bytes += socket->readAll();
                if (bytes->size() > 65536)
                {
                    *finished = true;
                    socket->abort();
                    return;
                }
                if (!bytes->contains('\n'))
                    return;
                *finished = true;
                QJsonObject response;
                try
                {
                    struct ucred peer;
                    socklen_t size = sizeof(peer);
                    require(::getsockopt(int(socket->socketDescriptor()), SOL_SOCKET, SO_PEERCRED,
                                         &peer, &size) == 0,
                            "Cannot verify desktop caller", "authorization");
                    require(peer.uid == ::geteuid() || peer.uid == 401 || peer.uid == 400 || peer.uid == 0,
                            "Desktop caller denied", "authorization", "peer-denied");
                    require(bytes->endsWith('\n') && bytes->count('\n') == 1,
                            "One bounded request required", "request");
                    auto value = strictJson(bytes->trimmed());
                    require(value.isObject(), "RPC object required", "request");
                    const auto request = value.toObject();
                    Desktop::exactFields(request, {"protocol", "method", "params"});
                    require(request["protocol"] == 1 && request["method"].isString() &&
                                request["params"].isObject(),
                            "Invalid RPC envelope", "request");
                    const auto method = request["method"].toString();
                    const bool privileged = peer.uid == ::geteuid() || peer.uid == 401;
                    require(privileged || method == "desktop.snapshot",
                            "Method unavailable to Web role", "authorization", "method-denied");
                    QJsonObject result;
                    if (method == "desktop.snapshot")
                    {
                        Desktop::exactFields(request["params"].toObject(), {});
                        result = providers.snapshot(privileged);
                    }
                    else
                        result = providers.dispatch(method, request["params"].toObject());
                    response = {{"protocol", 1}, {"ok", true}, {"result", result}};
                }
                catch (const Failure &error)
                {
                    response = {{"protocol", 1},
                                {"ok", false},
                                {"error", QJsonObject{{"code", error.code},
                                                      {"stage", error.stage},
                                                      {"message", error.detail}}}};
                }
                socket->write(QJsonDocument(response).toJson(QJsonDocument::Compact) + '\n');
                socket->disconnectFromServer();
            };
            connect(socket, &QLocalSocket::readyRead, socket, ready);
            if (socket->bytesAvailable())
                ready();
        }
    });
}
DesktopServer::~DesktopServer()
{
    close();
    QFile::remove(socketPath);
}
