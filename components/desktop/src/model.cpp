#include "model.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <cmath>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

namespace Desktop
{
qint64 monotonicMs()
{
    timespec now;
    require(::clock_gettime(CLOCK_BOOTTIME, &now) == 0, "Monotonic clock unavailable");
    return qint64(now.tv_sec) * 1000 + now.tv_nsec / 1000000;
}
QByteArray privateBytes(const QString &path, int limit)
{
    require(QDir::isAbsolutePath(path) && QDir::cleanPath(path) == path && !path.contains(QChar(0)),
            "Invalid private configuration path", "configuration");
    int fd =
        ::open(QFile::encodeName(path).constData(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
    require(fd >= 0, "Private configuration unavailable", "configuration", "not-configured");
    struct stat st;
    const bool safe = ::fstat(fd, &st) == 0 && S_ISREG(st.st_mode) && st.st_uid == ::geteuid() &&
                      !(st.st_mode & 0077) && st.st_size > 0 && st.st_size <= limit;
    if (!safe)
    {
        ::close(fd);
        throw Failure("private-config", "configuration",
                      "Private owner-only regular file required");
    }
    QByteArray bytes(int(st.st_size), Qt::Uninitialized);
    qint64 read = 0;
    while (read < st.st_size)
    {
        ssize_t n = ::read(fd, bytes.data() + read, size_t(st.st_size - read));
        if (n <= 0)
            break;
        read += n;
    }
    ::close(fd);
    require(read == st.st_size, "Incomplete private file", "configuration");
    return bytes;
}
QJsonObject privateConfig(const QString &path)
{
    auto value = strictJson(privateBytes(path, 32768));
    require(value.isObject(), "Configuration object required", "configuration");
    return value.toObject();
}
bool validId(const QString &value)
{
    return QRegularExpression("^[A-Za-z0-9_-]{1,128}$").match(value).hasMatch();
}
void exactFields(const QJsonObject &object, const QStringList &fields)
{
    auto a = object.keys(), b = fields;
    a.sort();
    b.sort();
    require(a == b, "Unknown or missing request fields", "request", "invalid-request");
}
void Pairing::apply(const QJsonObject &root, qint64 elapsed, qint64 now)
{
    fresh = false;
    const auto data = root["data"].toObject();
    require(elapsed >= 0 && root["success"] == true && data["deviceId"] == deviceId &&
                validId(deviceId),
            "Pairing response identity mismatch", "pairing", "invalid-response");
    if (!data.contains("bound") && !data.contains("bindTokenExpiresAt") &&
        !data.contains("serverTime"))
    {
        ownership = "unknown";
        lastCode.clear();
        deadline = 0;
        fresh = true;
        return;
    }
    require(data["bound"].isBool() && data["bindToken"].isString() &&
                positiveInteger(data["serverTime"]) &&
                positiveInteger(data["bindTokenExpiresAt"], true),
            "Invalid pairing response", "pairing", "invalid-response");
    const auto token = data["bindToken"].toString();
    const auto expiry = qint64(data["bindTokenExpiresAt"].toDouble());
    const auto server = qint64(data["serverTime"].toDouble());
    if (data["bound"].toBool())
    {
        require(token.isEmpty() && expiry == 0, "Contradictory bound response", "pairing");
        ownership = "bound";
        lastCode.clear();
        deadline = 0;
    }
    else
    {
        require(expiry > server && expiry - server <= 600 &&
                    QRegularExpression("^[0-9]{6}$").match(token).hasMatch(),
                "Invalid pairing expiry or code", "pairing");
        qint64 remaining = qMax(qint64(0), (expiry - server) * 1000 - elapsed);
        const qint64 next = now + remaining;
        // Even an expired code cannot be revived by polling the same token.
        deadline = token == lastCode ? qMin(deadline, next) : next;
        lastCode = token;
        ownership = "unbound";
    }
    fresh = true;
}
QJsonObject Pairing::snapshot(qint64 now) const
{
    const int remaining = int(qMax(qint64(0), deadline - now) / 1000);
    return {{"device_id", deviceId},
            {"ownership", ownership},
            {"fresh", fresh},
            {"code", ownership == "unbound" && remaining ? lastCode : QString()},
            {"expires_in", remaining}};
}
void Points::ingest(const QString &device, const QJsonObject &message, const QJsonObject &mapping,
                    qint64 now, qint64 utcSeconds, const QString &source)
{
    const QSet<QString> allowed{"schema_version", "device_id", "timestamp",
                                "sequence",       "quality",   "values"};
    for (auto it = message.begin(); it != message.end(); ++it)
        require(allowed.contains(it.key()), "Unknown telemetry field", "telemetry");
    require(validId(device) && message["schema_version"] == 1 && message["device_id"] == device &&
                message["timestamp"].isString() && message["quality"] == "good" &&
                positiveInteger(message["sequence"], true) && message["values"].isObject(),
            "Invalid or uncertain telemetry", "telemetry");
    const auto timestamp = message["timestamp"].toString();
    require(timestamp.endsWith('Z'), "UTC telemetry time required", "telemetry");
    auto sampled = QDateTime::fromString(timestamp, Qt::ISODateWithMs);
    if (!sampled.isValid())
        sampled = QDateTime::fromString(timestamp, Qt::ISODate);
    require(sampled.isValid() && utcSeconds >= 1704067200 &&
                sampled.toSecsSinceEpoch() <= utcSeconds + 5 &&
                utcSeconds - sampled.toSecsSinceEpoch() <= 60,
            "Telemetry sample expired or clock unready", "telemetry");
    const double sequence = message["sequence"].toDouble();
    require(!sequences.contains(device) || sequence > sequences[device],
            "Duplicate or old telemetry", "telemetry");
    auto values = message["values"].toObject();
    require(!values.isEmpty() && values.size() <= 64, "Bounded telemetry values required",
            "telemetry");
    QList<QPair<QString, Point>> staged;
    for (auto it = values.begin(); it != values.end(); ++it)
    {
        require(mapping.contains(it.key()), "Unauthorized telemetry point", "telemetry");
        const auto spec = mapping[it.key()].toObject();
        require(it.value().isDouble() && std::isfinite(it.value().toDouble()) &&
                    spec["scale"].isDouble() && std::isfinite(spec["scale"].toDouble()) &&
                    spec["scale"].toDouble() != 0 && validId(spec["type"].toString()) &&
                    spec["unit"].isString() && spec["unit"].toString().size() <= 16,
                "Invalid sensor value or unit mapping", "telemetry");
        const double value = it.value().toDouble() * spec["scale"].toDouble();
        require(std::isfinite(value), "Sensor conversion overflow", "telemetry");
        const QString key = device + '/' + it.key();
        staged.append(qMakePair(
            key,
            Point{{{"point_id", key},
                   {"name", spec["name"].toString(it.key())},
                   {"type", spec["type"]},
                   {"value", value},
                   {"unit", spec["unit"]},
                   {"timestamp", timestamp},
                   {"source", source},
                   {"quality", "good"}},
                  now + qMax(qint64(0), qint64(60) - (utcSeconds - sampled.toSecsSinceEpoch())) *
                            1000}));
    }
    int additions = 0;
    for (const auto &entry : staged)
        if (!points.contains(entry.first))
            ++additions;
    require(points.size() + additions <= 128 &&
                (sequences.contains(device) || sequences.size() < 32),
            "Telemetry cache full", "telemetry");
    for (const auto &entry : staged)
        points[entry.first] = entry.second;
    sequences[device] = sequence;
}
QJsonArray Points::snapshot(qint64 now) const
{
    QJsonArray out;
    for (auto it = points.begin(); it != points.end(); ++it)
    {
        auto value = it->data;
        if (it->expires <= now)
        {
            value["quality"] = "stale";
            value["value"] = QJsonValue();
        }
        out.append(value);
    }
    return out;
}
QJsonObject Points::cloudPayload(qint64 now) const
{
    QJsonArray out;
    for (const auto &entry : snapshot(now))
    {
        const auto item = entry.toObject();
        if (item["quality"] != "good")
            continue;
        out.append(
            QJsonObject{{"type", item["type"]}, {"value", item["value"]}, {"unit", item["unit"]}});
    }
    return {{"data", out}};
}
QJsonArray validateDevices(const QJsonObject &root)
{
    require(root["devices"].isArray() && root["devices"].toArray().size() <= 100,
            "Invalid device list", "home");
    QJsonArray out;
    QSet<QString> ids;
    for (const auto &entry : root["devices"].toArray())
    {
        const auto device = entry.toObject();
        const auto id = device["id"].toString();
        require(validId(id) && !ids.contains(id) && device["online"].isBool() &&
                    device["name"].isString() && device["name"].toString().size() <= 200 &&
                    device["status"].isArray() && device["status"].toArray().size() <= 128,
                "Invalid device", "home");
        ids.insert(id);
        out.append(QJsonObject{{"id", id},
                               {"name", device["name"]},
                               {"online", device["online"]},
                               {"room", device["room"].toString()},
                               {"status", device["status"]}});
    }
    return out;
}
QJsonArray validateFunctions(const QJsonObject &root)
{
    require(root["functions"].isArray() && root["functions"].toArray().size() <= 24 &&
                root["control_allowed"].isBool(),
            "Invalid control capabilities", "home");
    QJsonArray out;
    QSet<QString> codes;
    for (const auto &entry : root["functions"].toArray())
    {
        auto item = entry.toObject();
        const auto code = item["code"].toString();
        require(validId(code) && !codes.contains(code), "Invalid capability code", "home");
        codes.insert(code);
        if (item["values"].isString())
        {
            auto schema = strictJson(item["values"].toString().toUtf8());
            require(schema.isObject(), "Invalid capability schema", "home");
            item["values"] = schema;
        }
        require(item["values"].isObject(), "Capability schema required", "home");
        const auto type = item["type"].toString();
        require(type == "Boolean" || type == "Integer" || type == "Enum", "Unsupported capability",
                "home");
        item["writable"] = root["control_allowed"].toBool();
        out.append(item);
    }
    return out;
}
void validateCommand(const QJsonObject &function, const QJsonValue &value)
{
    require(function["writable"] == true, "Read-only capability", "home", "read-only");
    const auto type = function["type"].toString();
    const auto schema = function["values"].toObject();
    if (type == "Boolean")
        require(value.isBool(), "Boolean command required", "home");
    else if (type == "Integer")
    {
        const auto min = schema["min"], max = schema["max"], step = schema["step"];
        require(value.isDouble() && std::isfinite(value.toDouble()) &&
                    std::floor(value.toDouble()) == value.toDouble() && min.isDouble() &&
                    max.isDouble() && positiveInteger(step) &&
                    std::floor(min.toDouble()) == min.toDouble() &&
                    std::floor(max.toDouble()) == max.toDouble() &&
                    min.toDouble() >= -2147483648.0 && max.toDouble() <= 2147483647.0 &&
                    min.toDouble() <= max.toDouble() && value.toDouble() >= min.toDouble() &&
                    value.toDouble() <= max.toDouble() &&
                    std::fmod(value.toDouble() - min.toDouble(), step.toDouble()) == 0,
                "Integer command outside capability range", "home");
    }
    else if (type == "Enum")
    {
        require(value.isString() && schema["range"].isArray() &&
                    schema["range"].toArray().size() <= 64 &&
                    schema["range"].toArray().contains(value),
                "Unsupported enum value", "home");
    }
    else
        throw Failure("unsupported", "home", "Unsupported control capability");
}
QJsonValue reportedValue(const QJsonObject &device, const QString &code)
{
    for (const auto &entry : device["status"].toArray())
    {
        auto state = entry.toObject();
        if (state["code"] == code)
            return state["value"];
    }
    return QJsonValue(QJsonValue::Undefined);
}
} // namespace Desktop
