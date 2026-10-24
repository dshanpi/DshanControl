#include "mediasession.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QPointer>
#include <QTimer>
#include <QUuid>
#include <QList>

namespace { QList<QPointer<QProcess>> retiringWorkers; }
bool MediaSession::hasRetiringWorkers()
{
    for (int i = retiringWorkers.size() - 1; i >= 0; --i)
        if (!retiringWorkers[i] || retiringWorkers[i]->state() == QProcess::NotRunning)
            retiringWorkers.removeAt(i);
    return !retiringWorkers.isEmpty();
}


MediaSession::MediaSession(QObject *parent) : QObject(parent), process(new QProcess(this))
{
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
        emit changed(state);
    });
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int, QProcess::ExitStatus) {
                startupDeadline.stop();
                if (state["state"] != "error")
                    state = {{"available", false}, {"state", "error"}, {"error", "worker-exited"}};
                queue.clear();
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
    hasRetiringWorkers();
    retiringWorkers.append(retired);
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
