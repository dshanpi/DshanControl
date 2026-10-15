#include "applicationheartbeat.h"
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSaveFile>
#include <time.h>

ApplicationHeartbeat::ApplicationHeartbeat(const QString &destination, const QString &identity,
        const QUrl &healthUrl, QObject *parent)
    : QObject(parent), file(destination), commit(identity), url(healthUrl), network(this), timer(this)
{
    network.setProxy(QNetworkProxy::NoProxy);
    QFile boot("/proc/sys/kernel/random/boot_id");
    if (!QFileInfo(file).isAbsolute() || !QFileInfo(QFileInfo(file).absolutePath()).isDir() ||
        !QRegularExpression("^[a-f0-9]{40}$").match(commit).hasMatch() ||
        !boot.open(QIODevice::ReadOnly) || url.scheme()!="http" || url.host()!="127.0.0.1" ||
        url.path()!="/api/health" || !url.userInfo().isEmpty() || url.hasQuery() || url.hasFragment())
        return;
    bootId=QString::fromUtf8(boot.read(128)).trimmed();
    if (bootId.isEmpty()) return;
    timer.setInterval(1000);
    connect(&timer,&QTimer::timeout,this,[this]{poll();});
    timer.start();
    QTimer::singleShot(0,this,[this]{poll();});
}
void ApplicationHeartbeat::poll()
{
    if(pending) return;
    pending=true;
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    auto *reply=network.get(request);
    reply->setReadBufferSize(65537);
    auto *timeout=new QTimer(reply);timeout->setSingleShot(true);
    connect(timeout,&QTimer::timeout,reply,&QNetworkReply::abort);
    connect(reply,&QNetworkReply::readyRead,this,[reply]{
        if(reply->bytesAvailable()>65536) reply->abort();
    });
    connect(reply,&QNetworkReply::finished,this,[this,reply,timeout]{
        pending=false;timeout->stop();reply->deleteLater();
        if(reply->error()!=QNetworkReply::NoError ||
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()!=200)
            return;
        const auto bytes=reply->read(65537);
        if(bytes.size()>65536) return;
        QJsonParseError error;auto doc=QJsonDocument::fromJson(bytes,&error);
        if(error.error!=QJsonParseError::NoError || !doc.isObject()) return;
        auto health=doc.object();
        if(!health["ok"].isBool() || !health["ok"].toBool() || health["service"]!="omnigate-web" ||
            health["application_commit"]!=commit) return;
        if(QFileInfo(file).isSymLink() || QFileInfo(QFileInfo(file).absolutePath()).isSymLink()) return;
        struct timespec now;
        if(::clock_gettime(CLOCK_BOOTTIME,&now)!=0) return;
        QJsonObject value{{"schema_version",1},{"pid",double(QCoreApplication::applicationPid())},
            {"commit",commit},{"boot_id",bootId},{"gui_event_loop",true},
            {"backend_identity_verified",true},{"monotonic_seconds",double(now.tv_sec)+now.tv_nsec/1e9}};
        QSaveFile output(file);output.setDirectWriteFallback(false);
        if(!output.open(QIODevice::WriteOnly)) return;
        output.setPermissions(QFile::ReadOwner|QFile::WriteOwner);
        auto data=QJsonDocument(value).toJson(QJsonDocument::Compact);
        if(output.write(data)!=data.size()) {output.cancelWriting();return;}
        output.commit();
    });
    timeout->start(3000);
}
