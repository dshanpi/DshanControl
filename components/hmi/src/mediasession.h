#pragma once
#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QQueue>
#include <QTimer>
#include <QImage>
#include <QSharedMemory>
class MediaSession : public QObject
{
    Q_OBJECT
  public:
    explicit MediaSession(QObject *parent = nullptr);
    ~MediaSession() override;
    void start(bool explicitRequest = false);
    void command(const QString &name, const QJsonObject &params = {});
    QJsonObject snapshot() const { return state; }
  signals:
    void changed(const QJsonObject &state);
    void feedback(const QString &message);
    void frameReady(const QImage &frame);

  private:
    friend class HmiUiTest;
    QProcess *process;
    QTimer startupDeadline;
    void abortWorker(const QString &reason);
    QJsonObject state;
    QByteArray received;
    QQueue<QJsonObject> queue;
    bool startedOnce = false, failedRun = false;
    QString executable;
    QSharedMemory frameMemory;
    void read();
    void flush();
};
