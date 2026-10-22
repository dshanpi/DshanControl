#pragma once
#include <QColor>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QRegularExpression>
#include <QSet>

namespace DesktopV1
{
struct AppItem
{
    QString id, name, icon, sub;
    QColor color;
    bool planned;
    QString category;
};

struct ApplicationRoute
{
    enum Page { Overview, Fieldbus, Network, Devices, Alarms, Settings, Cloud, Family, Video };
    QString id, title;
    Page page = Overview;
    int section = 0;
    QJsonObject context;
};

// Metadata never executes a command or bypasses a service capability check.
class ApplicationRegistry
{
    static bool fail(QString *error, const QString &message)
    {
        if (error) *error = message;
        return false;
    }
    static bool target(const QString &id, ApplicationRoute *route)
    {
        static const QMap<QString, QPair<int, int>> routes = {
            {"overview", {ApplicationRoute::Overview, 0}},
            {"fieldbus", {ApplicationRoute::Fieldbus, 0}},
            {"network", {ApplicationRoute::Network, 0}},
            {"devices", {ApplicationRoute::Devices, 0}},
            {"alarms", {ApplicationRoute::Alarms, 0}},
            {"settings", {ApplicationRoute::Settings, 0}},
            {"cloud_bind", {ApplicationRoute::Cloud, 0}},
            {"reporting", {ApplicationRoute::Cloud, 1}},
            {"voice", {ApplicationRoute::Family, 0}},
            {"home", {ApplicationRoute::Family, 1}},
            {"video", {ApplicationRoute::Video, 0}},
            {"update", {ApplicationRoute::Settings, 3}}
        };
        const auto found = routes.constFind(id);
        if (found == routes.constEnd()) return false;
        route->page = ApplicationRoute::Page(found.value().first);
        route->section = found.value().second;
        return true;
    }

  public:
    static bool parse(const QByteArray &bytes, QList<AppItem> *output, QString *error = nullptr)
    {
        if (!output) return fail(error, "Missing application registry destination");
        QJsonParseError jsonError;
        const auto doc = QJsonDocument::fromJson(bytes, &jsonError);
        if (bytes.size() > 65536 || jsonError.error != QJsonParseError::NoError ||
            !doc.isObject() || doc.object()["schema_version"] != 1 ||
            !doc.object()["apps"].isArray())
            return fail(error, "Invalid application registry schema");
        const auto entries = doc.object()["apps"].toArray();
        if (entries.isEmpty() || entries.size() > 64)
            return fail(error, "Invalid application registry size");
        QMap<int, AppItem> ordered;
        QSet<QString> ids;
        for (const auto &value : entries) {
            if (!value.isObject()) return fail(error, "Invalid application entry");
            const auto item = value.toObject();
            const QString id = item["id"].toString();
            ApplicationRoute route;
            if (!QRegularExpression("^[a-z][a-z0-9_]{0,63}$").match(id).hasMatch() ||
                ids.contains(id) || !target(id, &route))
                return fail(error, "Unknown or duplicate application route");
            for (const auto &key : {"title", "subtitle", "icon", "category", "implementation_status"}) {
                const auto field = item[QLatin1String(key)];
                if (!field.isString() || field.toString().isEmpty() ||
                    field.toString().size() > 128 || field.toString().contains(QChar(0)))
                    return fail(error, "Invalid application metadata");
            }
            const QString category = item["category"].toString();
            const QString status = item["implementation_status"].toString();
            if (!QStringList{"industrial", "network_cloud", "system_media"}.contains(category) ||
                !QStringList{"existing", "source_integrated_unverified", "planned"}.contains(status))
                return fail(error, "Invalid application category or status");
            const auto order = item["order"];
            if (!order.isDouble() || order.toDouble() != order.toInt(-1) ||
                order.toInt(-1) < 0 || order.toInt() >= 64 || ordered.contains(order.toInt()))
                return fail(error, "Invalid or duplicate application order");
            const QString color = item["color"].toString();
            if (!QRegularExpression("^#[0-9A-Fa-f]{6}$").match(color).hasMatch())
                return fail(error, "Invalid application color");
            ids.insert(id);
            ordered.insert(order.toInt(), {id, item["title"].toString(), item["icon"].toString(),
                item["subtitle"].toString(), QColor(color), status != "existing", category});
        }
        // A malformed replacement never partially changes an existing registry.
        *output = ordered.values();
        if (error) error->clear();
        return true;
    }

    static QList<AppItem> apps()
    {
        static const QList<AppItem> registered = []() {
            QFile file(":/desktop/assets/app-registry.json");
            QList<AppItem> items;
            QString error;
            if (!file.open(QIODevice::ReadOnly) || !parse(file.readAll(), &items, &error))
                qFatal("Invalid compiled application registry: %s", qPrintable(error));
            return items;
        }();
        return registered;
    }

    static int indexForId(const QString &id)
    {
        const auto entries = apps();
        for (int i = 0; i < entries.size(); ++i)
            if (entries[i].id == id) return i;
        return -1;
    }

    static bool matchesCategory(const AppItem &item, int group)
    {
        if (group == 0) return true;
        const QStringList groups{"industrial", "network_cloud", "system_media"};
        return group >= 1 && group <= groups.size() && item.category == groups[group - 1];
    }

    static bool resolve(const QString &id, const QJsonObject &context, ApplicationRoute *output,
                        QString *error = nullptr)
    {
        const auto entries = apps();
        const int index = indexForId(id);
        if (!output || index < 0) return fail(error, "Application is not registered");
        ApplicationRoute route;
        route.id = id;
        route.title = entries[index].name;
        if (!target(id, &route)) return fail(error, "Application route is unavailable");
        QStringList allowed;
        if (id == "devices") allowed = QStringList{"point_id", "secondary_point_id"};
        if (id == "home") allowed = QStringList{"device_id"};
        if (id == "video") allowed = QStringList{"path"};
        for (auto it = context.begin(); it != context.end(); ++it) {
            const auto value = it.value();
            const QString text = value.toString();
            if (!allowed.contains(it.key()) || !value.isString() || text.isEmpty() ||
                (text.contains(QChar(0)) || text.contains('\n') || text.contains('\r')))
                return fail(error, "Invalid application context");
            if (it.key() == "path") {
                if (text.size() > 4096 || !QDir::isAbsolutePath(text) ||
                    QDir::cleanPath(text) != text)
                    return fail(error, "Expected a canonical local media path");
            } else if (it.key() == "device_id") {
                if (!QRegularExpression("^[A-Za-z0-9_.:-]{1,128}$").match(text).hasMatch())
                    return fail(error, "Invalid home device identifier");
            } else if (text.size() > 256 ||
                       !QRegularExpression("^[A-Za-z0-9_-]{1,128}/[A-Za-z0-9_-]{1,128}$")
                            .match(text).hasMatch())
                return fail(error, "Invalid point identifier");
            route.context.insert(it.key(), value);
        }
        *output = route;
        if (error) error->clear();
        return true;
    }
};
} // namespace DesktopV1
