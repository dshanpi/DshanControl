#include "desktoppoints.h"
#include <QDateTime>
#include <QRegularExpression>
#include <QSet>
#include <cmath>
namespace DesktopV1
{
bool DesktopPointHistory::validId(const QString &id)
{
    return id.size() <= 256 &&
           QRegularExpression("^[A-Za-z0-9_-]{1,128}/[A-Za-z0-9_-]{1,128}$").match(id).hasMatch();
}
bool DesktopPointHistory::validUnit(const QString &type, const QString &unit)
{
    if (type == "temperature")
        return unit == "°C" || unit == "C";
    if (type == "humidity")
        return unit == "%";
    if (type == "pressure")
        return QStringList{"Pa", "kPa", "MPa", "bar"}.contains(unit);
    return !type.isEmpty() &&
           QStringList{"°C", "C",  "%",   "Pa", "kPa", "MPa", "bar", "V",     "A",    "mA", "W",
                       "kW", "Hz", "rpm", "mm", "cm",  "m",   "s",   "L/min", "m3/h", ""}
               .contains(unit);
}
bool DesktopPointHistory::validSample(const QJsonObject &p, qint64 utcMs)
{
    const auto stamp = p["timestamp"].toString();
    const auto time = QDateTime::fromString(stamp, Qt::ISODateWithMs);
    const auto sampled = time.toMSecsSinceEpoch();
    return validId(p["point_id"].toString()) && p["quality"] == "good" && p["source"].isString() &&
           !p["source"].toString().isEmpty() && p["source"] != "demo" && p["source"] != "fixture" &&
           p["value"].isDouble() && std::isfinite(p["value"].toDouble()) && p["unit"].isString() &&
           validUnit(p["type"].toString(), p["unit"].toString()) && stamp.endsWith('Z') &&
           time.isValid() && utcMs >= 1704067200000LL && sampled <= utcMs + 5000 &&
           sampled > utcMs - 60000;
}
void DesktopPointHistory::setBindings(const QStringList &ids)
{
    QSet<QString> desired;
    for (const auto &id : ids)
        if (validId(id) && desired.size() < 8)
            desired.insert(id);
    for (const auto &id : entries.keys())
        if (!desired.contains(id))
            entries.remove(id);
    for (const auto &id : desired)
        if (!entries.contains(id))
            entries.insert(id, Entry());
}
void DesktopPointHistory::ingest(const QJsonArray &points, bool connected, qint64 utcMs,
                                 qint64 monoMs)
{
    if (lastUtc && qAbs((utcMs - lastUtc) - (monoMs - lastMono)) > 5000)
        for (auto it = entries.begin(); it != entries.end(); ++it)
        {
            it->samples.clear();
            it->gap = true;
            it->latest = 0;
        }
    lastUtc = utcMs;
    lastMono = monoMs;
    QMap<QString, QJsonObject> indexed;
    QSet<QString> duplicate;
    if (connected && points.size() <= 128)
        for (const auto &value : points)
        {
            const auto p = value.toObject();
            const auto id = p["point_id"].toString();
            if (indexed.contains(id))
                duplicate.insert(id);
            else
                indexed[id] = p;
        }
    for (auto it = entries.begin(); it != entries.end(); ++it)
    {
        auto &e = it.value();
        const auto p = indexed.value(it.key());
        const QString stamp = p["timestamp"].toString();
        const auto time = QDateTime::fromString(stamp, Qt::ISODateWithMs);
        const qint64 sampled = time.toMSecsSinceEpoch();
        const bool valid = connected && !duplicate.contains(it.key()) && validSample(p, utcMs);
        if (!valid)
        {
            e.gap = true;
            e.deadline = 0;
            e.point = {{"point_id", it.key()},
                       {"name", p["name"].toString(it.key()).left(128)},
                       {"unit", p["unit"].toString().left(16)},
                       {"quality", !connected || p.isEmpty()
                                       ? "unavailable"
                                       : p["quality"] == "stale" ||
                                                 (time.isValid() && sampled <= utcMs - 60000)
                                             ? "stale"
                                             : "error"}};
            continue;
        }
        if (sampled <= e.latest)
            continue; // Old/repeated snapshots cannot rewrite or extend a sample.
        const bool changedUnit =
            !e.point["unit"].toString().isEmpty() && e.point["unit"] != p["unit"];
        if (changedUnit)
        {
            e.samples.clear();
            e.gap = true;
        }
        TrendSample sample;
        sample.utc = sampled;
        sample.value = p["value"].toDouble();
        sample.startsSegment = e.gap || e.samples.isEmpty() || sampled - e.latest > 60000;
        e.samples.append(sample);
        e.latest = sampled;
        e.gap = false;
        e.deadline = monoMs + qMin(qint64(60000), sampled + 60000 - utcMs);
        e.point = p;
        e.point["name"] = p["name"].toString(it.key()).left(128);
        while (!e.samples.isEmpty() &&
               (e.samples.size() > 1800 || e.samples.first().utc < utcMs - 1800000))
            e.samples.removeFirst();
    }
}
QJsonObject DesktopPointHistory::current(const QString &id, qint64 utcMs, qint64 monoMs) const
{
    if (id.isEmpty())
        return {{"quality", "unbound"}};
    if (!entries.contains(id))
        return {{"point_id", id}, {"quality", "unavailable"}};
    const auto &e = entries.constFind(id).value();
    auto p = e.point;
    if (p.isEmpty())
        p = {{"point_id", id}, {"quality", "loading"}};
    if (p["quality"] == "good" &&
        (monoMs >= e.deadline || utcMs - e.latest >= 60000 || e.latest > utcMs + 5000))
        p["quality"] = "stale";
    if (p["quality"] != "good")
        p["value"] = QJsonValue();
    return p;
}
QVector<TrendSample> DesktopPointHistory::series(const QString &id, qint64 utcMs) const
{
    QVector<TrendSample> result;
    for (const auto &sample : entries.value(id).samples)
        if (sample.utc >= utcMs - 1800000 && sample.utc <= utcMs + 5000)
            result.append(sample);
    return result;
}
} // namespace DesktopV1
