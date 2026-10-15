#include "applicationhealth.h"
#include "applicationlaunch.h"
#include "applicationpermissions.h"
#include "applicationprocess.h"
#include "service.h"
#include "systemstorage.h"
#include "updater.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QTextStream>
int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  QStringList args = app.arguments();
  args.removeFirst();
  QString config = "/etc/dshancontrol/updater.json";
  try {
    if (args.value(0) == "serve" && args.size() == 2) {
      RuntimeService service(readJson(args[1]));
      return app.exec();
    }
    if (args.value(0) == "rpc" && args.size() == 3) {
      auto params = strictJson(args[2].toUtf8());
      require(params.isObject(), "RPC parameters must be an object");
      auto socket = qEnvironmentVariable("DSHANCONTROL_PLATFORM_SOCKET",
                                         "/var/run/dshancontrol/platform.sock");
      auto value = platformRequest(socket, args[1], params.toObject());
      if (value.isObject())
        QTextStream(stdout)
            << QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact)
            << '\n';
      else
        QTextStream(stdout) << "null\n";
      return 0;
    }
    if (args.value(0) == "--config") {
      require(args.size() >= 3, "Configuration and command required");
      config = args[1];
      args.removeFirst();
      args.removeFirst();
    }
    auto configuration = readJson(config);
    auto command = args.value(0);
    if ((command == "permissions-plan" || command == "permissions-apply") &&
        args.size() == 1) {
      QTextStream(stdout) << QJsonDocument(applicationPermissions(
                                               configuration,
                                               command == "permissions-apply"))
                                 .toJson(QJsonDocument::Compact)
                          << '\n';
      return 0;
    }
    if (command == "application-start" && args.size() == 1) {
      QTextStream(stdout) << QJsonDocument(startApplication(configuration))
                                 .toJson(QJsonDocument::Compact)
                          << '\n';
      return 0;
    }
    if (command == "application-stop" && args.size() <= 2) {
      auto result =
          ApplicationProcesses(configuration, "stop").stop(args.value(1));
      QTextStream(stdout) << QJsonDocument(result).toJson(
                                 QJsonDocument::Compact)
                          << '\n';
      return 0;
    }
    if ((command == "storage-check" || command == "storage-mount" ||
         command == "initialize") &&
        args.size() == 1) {
      auto result =
          command == "initialize"
              ? initializeApplication(configuration)
              : ensureSystemStorage(configuration, command == "storage-mount");
      QTextStream(stdout) << QJsonDocument(result).toJson(
                                 QJsonDocument::Compact)
                          << '\n';
      return 0;
    }
    if (command == "application-commit" && args.size() == 1) {
      QTextStream(stdout)
          << applicationIdentity(configuration)["commit"].toString() << '\n';
      return 0;
    }
    if (command == "application-health" && args.size() == 1) {
      QTextStream(stdout) << QJsonDocument(applicationHealth(configuration))
                                 .toJson(QJsonDocument::Compact)
                          << '\n';
      return 0;
    }
    NativeUpdater updater(configuration);
    QJsonValue result;
    if (command == "recover" && args.size() == 1) {
      updater.recover();
      result = QJsonObject{{"status", "recovered"}};
    } else if (command == "install" && args.size() == 4)
      result = updater.install(args[1], args[2], args[3]);
    else if (command == "check" && args.size() == 1) {
      auto candidate = updater.check();
      result = candidate.isEmpty() ? QJsonValue(QJsonValue::Null)
                                   : QJsonValue(candidate);
    } else if (command == "download" && args.size() == 1) {
      auto candidate = updater.check();
      require(!candidate.isEmpty(), "No compatible qualified update");
      auto destination = updater.applicationRoot() + "/downloads";
      updater.download(candidate, destination);
      result = destination;
    } else
      throw Failure(
          "invalid-command", "preparing",
          "Usage: dshancontrol-runtime [--config FILE] "
          "storage-check|storage-mount|initialize|application-stop "
          "[ROLE]|application-commit|application-health|check|download|recover|"
          "install MANIFEST SIGNATURE PACKAGE");
    if (result.isObject())
      QTextStream(stdout)
          << QJsonDocument(result.toObject()).toJson(QJsonDocument::Compact)
          << '\n';
    else if (result.isNull())
      QTextStream(stdout) << "null\n";
    else
      QTextStream(stdout) << result.toString() << '\n';
    return 0;
  } catch (const Failure &failure) {
    QTextStream(stderr) << QJsonDocument(
                               QJsonObject{{"ok", false},
                                           {"stage", failure.stage},
                                           {"code", failure.code},
                                           {"message", failure.detail}})
                               .toJson(QJsonDocument::Compact)
                        << '\n';
    return 1;
  } catch (const std::exception &) {
    QTextStream(stderr) << "{\"ok\":false,\"stage\":\"internal\",\"code\":"
                           "\"internal-error\"}\n";
    return 1;
  }
}
