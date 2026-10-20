#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QStringList>
#include <QVector>
namespace DesktopV1
{
struct TrendSample
{
    qint64 utc = 0;
    double value = 0;
    bool startsSegment = true;
};
class DesktopPointHistory
{
  public:
    void setBindings(const QStringList &ids);
    void ingest(const QJsonArray &points, bool connected, qint64 utcMs, qint64 monotonicMs);
    QJsonObject current(const QString &id, qint64 utcMs, qint64 monotonicMs) const;
    QVector<TrendSample> series(const QString &id, qint64 utcMs) const;
    static bool validSample(const QJsonObject &point, qint64 utcMs);
    static bool validId(const QString &id);
    static bool validUnit(const QString &type, const QString &unit);

  private:
    struct Entry
    {
        QJsonObject point;
        QVector<TrendSample> samples;
        qint64 latest = 0, deadline = 0;
        bool gap = true;
    };
    QMap<QString, Entry> entries;
    qint64 lastUtc = 0, lastMono = 0;
};
} // namespace DesktopV1
