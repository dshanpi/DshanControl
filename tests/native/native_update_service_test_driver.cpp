// Only this test entry point adds a private proxy/CA to the real service.
// Never install this driver or its synthetic CA in a system image.
#include "../../components/runtime/src/service.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QNetworkProxy>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QTextStream>
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    try {
        require(argc == 2, "Private fixture specification required");
        const auto spec = readJson(QString::fromLocal8Bit(argv[1]));
        require(spec["test_only"].isBool() && spec["test_only"].toBool(), "Explicit test fixture required");
        const auto service = readJson(spec["service_configuration"].toString());
        const auto updater = readJson(service["updater_config"].toString());
        require(updater["development_mode"] == true && updater["automatic_install"] == false,
                "Driver is restricted to private development fixtures");
        const QString root = spec["fixture_root"].toString();
        require(QDir::isAbsolutePath(root) && root != "/" && !QFileInfo(root).isSymLink(), "Invalid private root");
        for (const auto &path : {service["socket"].toString(), service["task_root"].toString(),
                                updater["application_root"].toString(), updater["system_contract"].toString()})
            require(QDir::cleanPath(path).startsWith(root + '/'), "Fixture path escaped private root");
        const auto proxy = spec["proxy_host"].toString();
        require((proxy == "127.0.0.1" || proxy == "192.168.1.199") &&
                positiveInteger(spec["proxy_port"]) && spec["proxy_port"].toInt() < 65536,
                "Private proxy endpoint required");
        QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,
            proxy, quint16(spec["proxy_port"].toInt())));
        auto tls = QSslConfiguration::defaultConfiguration();
        const auto certificates = QSslCertificate::fromPath(spec["ca_file"].toString());
        require(!certificates.isEmpty(), "Fixture CA missing");
        tls.setCaCertificates(tls.caCertificates() + certificates);
        QSslConfiguration::setDefaultConfiguration(tls);
        RuntimeService runtime(service);
        return app.exec();
    } catch (const Failure &e) {
        QTextStream(stderr) << QJsonDocument(QJsonObject{{"code",e.code}, {"stage",e.stage},
            {"message",e.detail}}).toJson(QJsonDocument::Compact) << '\n';
        return 1;
    }
}
