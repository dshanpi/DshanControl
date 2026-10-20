#pragma once
#include <QJsonObject>
#include <QObject>
#include <QQueue>
#include <QTimer>
class DesktopClient : public QObject
{
    Q_OBJECT
  public:
    explicit DesktopClient(QObject *parent = nullptr);
    void request(const QString &method, const QJsonObject &params = {});
    QJsonObject snapshot() const { return state; }
  signals:
    void snapshotChanged(const QJsonObject &state);
    void feedback(const QString &message);

  private:
    friend class HmiUiTest;
    struct Request
    {
        QString method;
        QJsonObject params;
    };
    QQueue<Request> queue;
    QTimer poll;
    QJsonObject state;
    bool active = false;
    QString socketPath;
    qint64 serverUid = 0;
    void next();
};
