#include "../../components/hmi/src/desktop/applicationregistry.h"
#include <QtTest>
using namespace DesktopV1;

class ApplicationRegistryTest : public QObject
{
    Q_OBJECT
    QJsonObject manifest()
    {
        QFile file(":/desktop/assets/app-registry.json");
        if (!file.open(QIODevice::ReadOnly)) return {};
        return QJsonDocument::fromJson(file.readAll()).object();
    }
  private slots:
    void registeredApplicationsHaveStableRoutesAndCategories()
    {
        const QStringList ids{"overview", "fieldbus", "network", "devices", "alarms", "settings",
            "cloud_bind", "reporting", "voice", "home", "video", "update"};
        const QList<int> pages{0, 1, 2, 3, 4, 5, 6, 6, 7, 7, 8, 5};
        const auto apps = ApplicationRegistry::apps();
        QCOMPARE(apps.size(), ids.size());
        QMap<QString, int> groups;
        for (int i = 0; i < ids.size(); ++i) {
            QCOMPARE(apps[i].id, ids[i]);
            QCOMPARE(ApplicationRegistry::indexForId(ids[i]), i);
            ApplicationRoute route;
            QVERIFY(ApplicationRegistry::resolve(ids[i], {}, &route));
            QCOMPARE(route.id, ids[i]);
            QCOMPARE(int(route.page), pages[i]);
            QVERIFY(!route.title.isEmpty());
            QVERIFY(route.context.isEmpty());
            groups[apps[i].category]++;
            QVERIFY(ApplicationRegistry::matchesCategory(apps[i], 0));
            int matched = 0;
            for (int group = 1; group <= 3; ++group)
                matched += ApplicationRegistry::matchesCategory(apps[i], group);
            QCOMPARE(matched, 1);
            QVERIFY(!ApplicationRegistry::matchesCategory(apps[i], -1));
            QVERIFY(!ApplicationRegistry::matchesCategory(apps[i], 4));
        }
        QCOMPARE(groups["industrial"], 4);
        QCOMPARE(groups["network_cloud"], 5);
        QCOMPARE(groups["system_media"], 3);
    }
    void metadataOrderDoesNotDependOnJsonArrayPosition()
    {
        auto value = manifest();
        auto entries = value["apps"].toArray();
        const auto first = entries.first();
        entries.removeFirst();
        entries.append(first);
        value["apps"] = entries;
        QList<AppItem> parsed;
        QVERIFY(ApplicationRegistry::parse(QJsonDocument(value).toJson(), &parsed));
        QCOMPARE(parsed.first().id, QString("overview"));
        QCOMPARE(parsed.last().id, QString("update"));
    }
    void invalidRegistryDoesNotPartiallyReplaceKnownEntries()
    {
        const auto good = manifest();
        for (int fault = 0; fault < 8; ++fault) {
            auto value = good;
            auto entries = value["apps"].toArray();
            auto item = entries[1].toObject();
            if (fault == 0) value["schema_version"] = true;
            if (fault == 1) item["id"] = "overview";
            if (fault == 2) item["id"] = "not_a_registered_handler";
            if (fault == 3) item["order"] = 0;
            if (fault == 4) item["order"] = 1.5;
            if (fault == 5) item["title"] = QJsonArray{"wrong type"};
            if (fault == 6) item["category"] = "industrial_typo";
            if (fault == 7) item["color"] = "not-a-color";
            entries[1] = item;
            value["apps"] = entries;
            auto output = ApplicationRegistry::apps();
            QString error;
            QVERIFY(!ApplicationRegistry::parse(QJsonDocument(value).toJson(), &output, &error));
            QVERIFY(!error.isEmpty());
            QCOMPARE(output.size(), 12);
            QCOMPARE(output[1].id, QString("fieldbus"));
        }
    }
    void invalidRequestsCannotFallBackToAnotherApplication()
    {
        ApplicationRoute route;
        route.id = "unchanged";
        QString error;
        QVERIFY(!ApplicationRegistry::resolve("unknown", {}, &route, &error));
        QCOMPARE(route.id, QString("unchanged"));
        QVERIFY(!ApplicationRegistry::resolve("overview", {{"path", "/tmp/test.mp4"}}, &route));
        QVERIFY(!ApplicationRegistry::resolve("devices", {{"point_id", 42}}, &route));
        QVERIFY(!ApplicationRegistry::resolve("devices", {{"point_id", "device/point\n"}}, &route));
        QVERIFY(!ApplicationRegistry::resolve("home", {{"device_id", "device\n"}}, &route));
        QVERIFY(!ApplicationRegistry::resolve("home", {{"device_id", "device"}, {"action", "on"}}, &route));
        QCOMPARE(route.id, QString("unchanged"));
    }
    void contextsRetainExplicitBindingsWithoutResolvingDevicesOrStartingMedia()
    {
        ApplicationRoute route;
        const QJsonObject points{{"point_id", "device/temperature"}, {"secondary_point_id", "device/humidity"}};
        QVERIFY(ApplicationRegistry::resolve("devices", points, &route));
        QCOMPARE(route.context, points);
        QVERIFY(ApplicationRegistry::resolve("home", {{"device_id", "lamp.room:1"}}, &route));
        QCOMPARE(route.context["device_id"].toString(), QString("lamp.room:1"));
        const QString media = "/media/not-present-yet/测试 video.mp4";
        QVERIFY(ApplicationRegistry::resolve("video", {{"path", media}}, &route));
        QCOMPARE(route.context["path"].toString(), media);
        QVERIFY(!ApplicationRegistry::resolve("video", {{"path", "../video.mp4"}}, &route));
        QVERIFY(!ApplicationRegistry::resolve("video", {{"path", "/media/../video.mp4"}}, &route));
        QVERIFY(!ApplicationRegistry::resolve("video", {{"path", "https://example.invalid/video"}}, &route));
        QVERIFY(!ApplicationRegistry::resolve("video", {{"path", QString("/media/a") + QChar(0) + "b"}}, &route));
        QVERIFY(ApplicationRegistry::resolve("reporting", {}, &route));
        QCOMPARE(route.section, 1);
        QVERIFY(ApplicationRegistry::resolve("cloud_bind", {}, &route));
        QCOMPARE(route.section, 0);
    }
};
QTEST_GUILESS_MAIN(ApplicationRegistryTest)
#include "application_registry_test.moc"
