#pragma once
#include "../../runtime/src/storage.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>

namespace Desktop
{
qint64 monotonicMs();
QByteArray privateBytes(const QString &path, int limit);
QJsonObject privateConfig(const QString &path);
bool validId(const QString &value);
void exactFields(const QJsonObject &object, const QStringList &fields);
class Pairing
{
  public:
    QString deviceId, ownership = "unknown", lastCode;
    qint64 deadline = 0;
    bool fresh = false;
    void apply(const QJsonObject &root, qint64 elapsed, qint64 now);
    QJsonObject snapshot(qint64 now) const;
};
// A single bounded latest-value cache, not a claimed durable delivery queue.
class Points
{
  public:
    struct Point
    {
        QJsonObject data;
        qint64 expires;
    };
    QMap<QString, Point> points;
    QMap<QString, double> sequences;
    void ingest(const QString &device, const QJsonObject &message, const QJsonObject &mapping,
                qint64 now, qint64 utcSeconds, const QString &source);
    QJsonArray snapshot(qint64 now) const;
    QJsonObject cloudPayload(qint64 now) const;
};
QJsonArray validateDevices(const QJsonObject &root);
QJsonArray validateFunctions(const QJsonObject &root);
void validateCommand(const QJsonObject &function, const QJsonValue &value);
QJsonValue reportedValue(const QJsonObject &device, const QString &code);
} // namespace Desktop
