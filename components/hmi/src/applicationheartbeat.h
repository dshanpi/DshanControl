#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QUrl>

// Written only by the GUI event loop after a matching backend health response.
class ApplicationHeartbeat : public QObject {
public:
    ApplicationHeartbeat(const QString &file, const QString &commit,
                         const QUrl &healthUrl, QObject *parent = nullptr);
private:
    void poll();
    QString file, commit, bootId;
    QUrl url;
    QNetworkAccessManager network;
    QTimer timer;
    bool pending = false;
};
