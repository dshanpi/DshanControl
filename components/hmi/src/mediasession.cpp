#include "mediasession.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QPointer>
#include <QTimer>
#include <QUuid>
#include <QProcessEnvironment>
#include <QSaveFile>
#include <QtEndian>
#include <sys/socket.h>
#include <unistd.h>

MediaSession::MediaSession(QObject *parent) : QObject(parent), process(new QProcess(this))
{
    connect(&frameServer, &QLocalServer::newConnection, this, &MediaSession::acceptFrames);
    executable = QCoreApplication::applicationDirPath() + "/dshancontrol-media-worker";
    state = {{"available", false}, {"state", "not_installed"}};
    startupDeadline.setSingleShot(true);
    startupDeadline.setInterval(4000);
    connect(&startupDeadline, &QTimer::timeout, this,
            [this]() { abortWorker("worker-start-timeout"); });
    process->setProcessChannelMode(QProcess::SeparateChannels);
    connect(process, &QProcess::readyReadStandardOutput, this, &MediaSession::read);
    connect(process, &QProcess::readyReadStandardError, this,
            [this]() { process->readAllStandardError(); });
    connect(process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        startupDeadline.stop();
        failedRun = true;
        if (state["state"] != "error")
            state = {{"available", false}, {"state", "error"}, {"error", "worker-unavailable"}};
        queue.clear();
        resetFrames();
        emit changed(state);
    });
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int, QProcess::ExitStatus) {
                startupDeadline.stop();
                if (state["state"] != "error")
                    state = {{"available", false}, {"state", "error"}, {"error", "worker-exited"}};
                queue.clear();
                resetFrames();
                emit changed(state);
            });
}
MediaSession::~MediaSession()
{
    // The vendor shutdown may block. Retire the child asynchronously so closing
    // a GUI page never waits on the decoder. All callbacks belong to the child.
    startupDeadline.stop();
    process->disconnect(this);
    if (process->state() == QProcess::NotRunning)
        return;
    process->setParent(nullptr);
    auto *retired = process;
    connect(retired, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), retired,
            &QObject::deleteLater);
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, retired,
            [retired]() { retired->kill(); });
    const auto request = QJsonDocument(QJsonObject{{"protocol", 1},
                                                   {"request_id", "closing"},
                                                   {"command", "close"},
                                                   {"params", QJsonObject()}})
                             .toJson(QJsonDocument::Compact);
    retired->write(request + '\n');
    retired->closeWriteChannel();
    QTimer::singleShot(1500, retired, [retired]() { retired->terminate(); });
    QTimer::singleShot(2500, retired, [retired]() { retired->kill(); });
}
void MediaSession::abortWorker(const QString &reason)
{
    startupDeadline.stop();
    failedRun = true;
    queue.clear();
    received.clear();
    resetFrames();
    state = {{"available", false}, {"state", "error"}, {"error", reason}};
    emit changed(state);
    // Capture this specific process/run; never kill a later explicit restart.
    QPointer<QProcess> target(process);
    const auto pid = process->processId();
    process->terminate();
    QTimer::singleShot(1000, process, [target, pid]() {
        if (target && target->processId() == pid && target->state() != QProcess::NotRunning)
            target->kill();
    });
}
void MediaSession::start(bool explicitRequest)
{
    if (process->state() != QProcess::NotRunning || (startedOnce && !explicitRequest))
        return;
    startedOnce = true;
    failedRun = false;
    received.clear();
    queue.clear();
    if (!QFileInfo(executable).isExecutable())
    {
        state = {{"available", false}, {"state", "not_installed"}};
        emit changed(state);
        return;
    }
    resetFrames();
    frameDirectory.reset(new QTemporaryDir("/tmp/dshancontrol-media-XXXXXX"));
    if (!frameDirectory->isValid() || !frameServer.listen(frameDirectory->filePath("frames.sock")))
    {
        state = {{"available", false}, {"state", "error"}, {"error", "frame-channel-failed"}};
        emit changed(state); return;
    }
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("DSHANCONTROL_MEDIA_FRAME_SOCKET", frameServer.fullServerName());
    process->setProcessEnvironment(environment);
    startupDeadline.setInterval(qEnvironmentVariable("DSHANCONTROL_MEDIA_BACKEND") == "gstreamer-software" ? 20000 : 4000);
    state = {{"available", false}, {"state", "starting_service"}};
    emit changed(state);
    process->setProgram(executable);
    process->setArguments({});
    startupDeadline.start();
    process->start();
}
void MediaSession::command(const QString &name, const QJsonObject &params)
{
    if (process->state() == QProcess::NotRunning)
    {
        emit feedback("播放器组件未运行，请重新选择视频。");
        return;
    }
    if (queue.size() >= 16)
    {
        emit feedback("播放器正在处理操作，请稍后。");
        return;
    }
    QJsonObject request{{"protocol", 1},
                        {"request_id", QUuid::createUuid().toString(QUuid::Id128)},
                        {"command", name},
                        {"params", params}};
    // A rectangle before open is a prerequisite, not an obsolete resize.
    // Merge only adjacent trailing resizes; never cross a control command.
    if (name == "rect" && !queue.isEmpty() && queue.last()["command"] == "rect")
        queue.removeLast();
    queue.enqueue(request);
    flush();
}
void MediaSession::flush()
{
    if (process->state() != QProcess::Running || state["available"] != true)
        return;
    while (!queue.isEmpty())
        process->write(QJsonDocument(queue.dequeue()).toJson(QJsonDocument::Compact) + '\n');
}
void MediaSession::read()
{
    if (failedRun) { process->readAllStandardOutput(); return; }
    received += process->readAllStandardOutput();
    if (received.size() > 131072)
    {
        abortWorker("worker-response-too-large");
        emit feedback("播放器响应异常，已停止接收。");
        return;
    }
    int end;
    while ((end = received.indexOf('\n')) >= 0)
    {
        QJsonParseError error;
        const auto bytes = received.left(end);
        received.remove(0, end + 1);
        const auto doc = QJsonDocument::fromJson(bytes, &error);
        const auto root = doc.object();
        if (error.error != QJsonParseError::NoError || !doc.isObject() || root["protocol"] != 1)
        {
            abortWorker("worker-invalid-response");
            emit feedback("播放器返回无效数据。");
            return;
        }
        if (root["event"] == "status")
        {
            const QStringList states = {"idle",   "preparing", "prepared", "starting",   "playing",
                                        "paused", "completed", "error",    "unavailable"};
            if (!root["available"].isBool() || !root["state"].isString() ||
                !states.contains(root["state"].toString()))
            {
                abortWorker("worker-invalid-status");
                return;
            }
            if(root["available"]==true &&
                (!root["capabilities"].toArray().contains("suspend-during-prepare") ||
                 !root["capabilities"].toArray().contains("visibility-state"))) {
                abortWorker("worker-incompatible-capabilities");return;
            }
            startupDeadline.stop();
            state = root;
            const auto diagnostics=qEnvironmentVariable("DSHANCONTROL_MEDIA_STATUS_FILE");
            if(!diagnostics.isEmpty())
            {
                QSaveFile file(diagnostics);
                if(file.open(QIODevice::WriteOnly))
                {
                    file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner);
                    auto record=state;
                    record["worker_pid"]=double(process->processId());
                    record["hmi_pid"]=double(QCoreApplication::applicationPid());
                    record["frames_received"]=double(framesReceived);
                    file.write(QJsonDocument(record).toJson(QJsonDocument::Compact));file.commit();
                }
            }
            emit changed(state);
            if (root["available"] == false && !queue.isEmpty())
            {
                queue.clear();
                emit feedback("当前播放器组件未包含板端解码支持。");
            }
            flush();
        }
        else if (root["event"] == "reply" && root["accepted"] == false)
            emit feedback("播放操作未完成，请查看播放器状态。");
    }
}

void MediaSession::resetFrames()
{
    if (frameSocket) { frameSocket->disconnect(this); frameSocket->abort(); frameSocket->deleteLater(); frameSocket = nullptr; }
    frameServer.close(); frameInput.clear(); frameDirectory.reset(); framesReceived=0;
    emit frameReady(QImage());
}
void MediaSession::acceptFrames()
{
    while (frameServer.hasPendingConnections())
    {
        auto *connection = frameServer.nextPendingConnection();
        struct ucred peer{}; socklen_t size=sizeof(peer);
        if (frameSocket || process->processId()<=0 ||
            ::getsockopt(int(connection->socketDescriptor()),SOL_SOCKET,SO_PEERCRED,&peer,&size)!=0 ||
            peer.pid!=process->processId() || peer.uid!=::geteuid())
        { connection->abort(); connection->deleteLater(); continue; }
        frameSocket=connection; frameInput.clear();
        connection->setReadBufferSize(2*(20+640*480*2));
        connect(connection,&QLocalSocket::readyRead,this,&MediaSession::readFrames);
        connect(connection,&QLocalSocket::disconnected,this,[this,connection]() {
            if(frameSocket==connection) {
                frameSocket=nullptr; frameInput.clear(); emit frameReady(QImage());
                if(!failedRun && process->state()==QProcess::Running && state["capabilities"].toArray().contains("qt-rgb565-frames-v1"))
                    abortWorker("worker-frame-disconnected");
            }
            connection->deleteLater();
        });
        readFrames();
    }
}
void MediaSession::readFrames()
{
    if(!frameSocket)return;
    frameInput+=frameSocket->readAll();
    if(frameInput.size()>2*(20+640*480*2)) { abortWorker("worker-frame-too-large"); return; }
    while(frameInput.size()>=20)
    {
        const auto *h=reinterpret_cast<const uchar *>(frameInput.constData());
        const quint32 magic=qFromLittleEndian<quint32>(h),w=qFromLittleEndian<quint32>(h+4),
                      height=qFromLittleEndian<quint32>(h+8),bytes=qFromLittleEndian<quint32>(h+12), generation=qFromLittleEndian<quint32>(h+16);
        if(magic!=0x31465244 || !w || w>640 || !height || height>480 || bytes!=w*height*2)
        { abortWorker("worker-invalid-frame"); return; }
        if(frameInput.size()<int(20+bytes))return;
        const QImage image(reinterpret_cast<const uchar *>(frameInput.constData()+20),int(w),int(height),int(w*2),QImage::Format_RGB16);
        const QImage copy=image.copy(); frameInput.remove(0,int(20+bytes));
        if (state["available"]==true && state["frame_generation"].toDouble()==generation) { ++framesReceived; emit frameReady(copy); }
    }
}
