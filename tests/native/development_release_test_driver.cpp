// Test-only TLS/proxy injection. Never installed in the system image.
#include "../../components/runtime/src/updater.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QNetworkProxy>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QTextStream>
int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  try {
    require(argc == 2, "Test specification required");
    const auto spec = readJson(QString::fromLocal8Bit(argv[1]));
    QNetworkProxy::setApplicationProxy(
        QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1",
                      quint16(spec["proxy_port"].toInt())));
    if (spec["trust_fixture_ca"].toBool(true)) {
      auto tls = QSslConfiguration::defaultConfiguration();
      tls.setCaCertificates(
          tls.caCertificates() +
          QSslCertificate::fromPath(spec["ca_file"].toString()));
      QSslConfiguration::setDefaultConfiguration(tls);
    }
    NativeUpdater updater(readJson(spec["configuration"].toString()));
    const auto operation = spec["operation"].toString();
    QJsonObject value;
    if (operation == "check")
      value = updater.check();
    else if (operation == "download")
      value = updater.download(spec["candidate"].toObject(),
                               spec["destination"].toString());
    else
      throw Failure("test-operation", "test", "Unknown test operation");
    QTextStream(stdout) << QJsonDocument(value).toJson(QJsonDocument::Compact)
                        << '\n';
    return 0;
  } catch (const Failure &failure) {
    QTextStream(stderr) << QJsonDocument(
                               QJsonObject{{"code", failure.code},
                                           {"stage", failure.stage},
                                           {"message", failure.detail}})
                               .toJson(QJsonDocument::Compact)
                        << '\n';
    return 1;
  }
}
