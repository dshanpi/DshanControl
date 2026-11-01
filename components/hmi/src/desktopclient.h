#pragma once
#include <QJsonObject>
#include <QObject>
#include <QQueue>
#include <QTimer>
#include <QSet>
class DesktopClient : public QObject
{
    Q_OBJECT
  public:
    explicit DesktopClient(QObject *parent = nullptr);
    void request(const QString &method, const QJsonObject &params = {});
    bool isPending(const QString &method) const;
    QJsonObject snapshot() const { return state; }
  signals:
    void snapshotChanged(const QJsonObject &state);
    void feedback(const QString &message);
    void operationFeedback(const QString &method, const QString &message);
    void requestStateChanged();

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
    QString socketPath, activeMethod;
    QSet<QString> awaitingSnapshot;
    qint64 serverUid = 0;
    void next();
};
