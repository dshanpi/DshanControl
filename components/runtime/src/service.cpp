#include "service.h"
#include "display.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QRegularExpression>
#include <QTimer>
#include <QUuid>
#include <memory>
#include <sys/socket.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

namespace {
QString taskPath(const QString &root, const QString &id) {
  require(QRegularExpression("^[0-9a-f]{32}$").match(id).hasMatch(),
          "Invalid task ID", "identity");
  return root + '/' + id + ".json";
}
QJsonObject identity(const QJsonObject &contract) {
  QJsonObject value;
  for (const auto &field : TargetFields)
    value[field] = contract[field];
  return value;
}
QJsonObject errorObject(const Failure &error) {
  return {
      {"code", error.code}, {"stage", error.stage}, {"message", error.detail}};
}
} // namespace
RuntimeService::RuntimeService(const QJsonObject &configuration)
    : config(configuration) {
  require(config["allowed_uids"].isArray() &&
              positiveInteger(config["client_gid"], true) &&
              config["client_gid"].toDouble() <= 2147483647,
          "Explicit platform peer identities required", "authorization");
  for (const auto &uid : config["allowed_uids"].toArray())
    require(positiveInteger(uid, true) && uid.toDouble() <= 2147483647,
            "Invalid allowed platform UID", "authorization");
  updaterConfig = config["updater_config"].toString();
  taskRoot = config["task_root"].toString();
  // Verify the persistent volume before creating even the task directory.
  NativeUpdater updater(readJson(updaterConfig));
  require(QDir::isAbsolutePath(taskRoot) && taskRoot != "/" &&
              QDir::cleanPath(taskRoot) == taskRoot &&
              !QFileInfo(taskRoot).isSymLink() && QDir().mkpath(taskRoot),
          "Invalid system task directory", "storage");
  require(::chmod(QFile::encodeName(taskRoot).constData(), 0700) == 0,
          "Cannot protect task directory", "storage");
  serviceOwnership.reset(
      new FileLock(updater.applicationRoot() + "/.runtime-service.lock"));
  updater.recover();
  for (const auto &name : QDir(taskRoot).entryList({"*.json"}, QDir::Files)) {
    if (name == "latest.json")
      continue;
    auto value = readJson(taskRoot + '/' + name);
    if (value["status"] == "running") {
      value["status"] = "failed";
      value["error"] =
          QJsonObject{{"code", "interrupted"},
                      {"stage", value["stage"]},
                      {"message", "System task interrupted; transaction "
                                  "recovery checked, no automatic reinstall"}};
      durableJson(taskRoot + '/' + name, value);
    }
  }
  auto socketPath = config["socket"].toString();
  require(QDir::isAbsolutePath(socketPath) &&
              !QFileInfo(socketPath).isSymLink(),
          "Invalid platform socket path", "resource-lock");
  if (QFileInfo::exists(socketPath)) {
    struct stat info;
    require(::lstat(QFile::encodeName(socketPath).constData(), &info) == 0 &&
                S_ISSOCK(info.st_mode) && info.st_uid == ::getuid(),
            "Existing platform path is not an owned socket", "resource-lock");
    QLocalSocket probe;
    probe.connectToServer(socketPath);
    require(!probe.waitForConnected(500) &&
                probe.error() == QLocalSocket::ConnectionRefusedError,
            "Platform socket is live or its owner cannot be determined",
            "resource-lock");
    require(QFile::remove(socketPath), "Cannot remove owned stale socket",
            "storage");
  }
  require(QDir().mkpath(QFileInfo(socketPath).absolutePath()) &&
              listen(socketPath),
          "Cannot open platform socket", "service");
  require(::chmod(QFile::encodeName(socketPath).constData(), 0660) == 0 &&
              ::chown(QFile::encodeName(socketPath).constData(), ::getuid(),
                      config["client_gid"].toInt()) == 0,
          "Cannot protect platform socket", "service");
  connect(this, &QLocalServer::newConnection, this, [this]() {
    while (hasPendingConnections()) {
      auto socket = nextPendingConnection();
      auto received = std::make_shared<QByteArray>();
      auto timer = new QTimer(socket);
      timer->setSingleShot(true);
      timer->start(5000);
      connect(timer, &QTimer::timeout, socket, &QLocalSocket::abort);
      connect(socket, &QLocalSocket::disconnected, socket,
              &QObject::deleteLater);
      auto finish = [this, socket, received, timer]() {
        *received += socket->readAll();
        if (received->size() > 1024 * 1024) {
          socket->abort();
          return;
        }
        if (!received->contains('\n'))
          return;
        timer->stop();
        QJsonObject response;
        try {
          struct ucred peer;
          socklen_t size = sizeof(peer);
          require(::getsockopt(static_cast<int>(socket->socketDescriptor()),
                               SOL_SOCKET, SO_PEERCRED, &peer, &size) == 0,
                  "Cannot verify local caller", "authorization");
          require(config["allowed_uids"].toArray().contains(
                      static_cast<int>(peer.uid)),
                  "Platform caller not authorized", "authorization",
                  "peer-denied");
          require(received->endsWith('\n') && received->count('\n') == 1,
                  "One bounded request required");
          auto json = strictJson(received->trimmed());
          require(json.isObject(), "RPC object required");
          if (json.toObject()["method"] == "service.shutdown")
            require(peer.uid == ::getuid(),
                    "Only the system service owner may stop the service",
                    "authorization", "peer-denied");
          response = {{"protocol", 1},
                      {"ok", true},
                      {"result", dispatch(json.toObject())}};
        } catch (const Failure &error) {
          response = {
              {"protocol", 1}, {"ok", false}, {"error", errorObject(error)}};
        } catch (...) {
          response = {
              {"protocol", 1},
              {"ok", false},
              {"error", QJsonObject{{"code", "internal-error"},
                                    {"message", "Platform internal failure"}}}};
        }
        socket->write(QJsonDocument(response).toJson(QJsonDocument::Compact) +
                      '\n');
        socket->disconnectFromServer();
      };
      connect(socket, &QLocalSocket::readyRead, socket, finish);
      if (socket->bytesAvailable())
        finish();
    }
  });
}
RuntimeService::~RuntimeService() {
  close();
  QFile::remove(config["socket"].toString());
}
QJsonObject RuntimeService::task(const QString &requested) const {
  QString id = requested;
  if (id.isEmpty()) {
    if (!QFileInfo::exists(taskRoot + "/latest.json"))
      return {};
    id = readJson(taskRoot + "/latest.json")["task_id"].toString();
  }
  auto value = readJson(taskPath(taskRoot, id));
  NativeUpdater updater(readJson(updaterConfig));
  require(value["task_id"] == id &&
              value["identity"] == identity(updater.systemContract()),
          "Task belongs to another system", "identity");
  return value;
}
QJsonObject RuntimeService::startTask(const QJsonObject &params) {
  auto operation = params["operation"].toString();
  require(operation == "check" || operation == "download" ||
              operation == "install",
          "Unknown update operation");
  QStringList fields{"operation"};
  QJsonObject source;
  if (operation != "check") {
    fields << "source_task_id";
    source = task(params["source_task_id"].toString());
    require(source["status"] == "success" &&
                source["operation"] ==
                    (operation == "download" ? "check" : "download") &&
                source["candidate"].isObject(),
            "Invalid source task");
  }
  if (operation == "install") {
    fields << "confirmed"
           << "version"
           << "package_sha256";
    auto candidate = source["candidate"].toObject();
    require(params["confirmed"].isBool() && params["confirmed"].toBool() &&
                params["version"] == candidate["version"] &&
                params["package_sha256"] == candidate["package_sha256"],
            "Confirmation does not match downloaded release", "authorization");
  }
  auto actualFields = params.keys();
  actualFields.sort();
  fields.sort();
  require(actualFields == fields, "Unknown/missing task request fields");
  auto ownership = std::make_shared<FileLock>(taskRoot + "/.task.lock");
  auto configuration = readJson(updaterConfig);
  NativeUpdater updater(configuration);
  auto contractHash = hashFile(configuration["system_contract"].toString());
  if (!source.isEmpty())
    require(source["system_contract_sha256"] == contractHash,
            "System contract changed", "identity");
  auto id = QUuid::createUuid().toString(QUuid::Id128);
  auto path = taskPath(taskRoot, id);
  QJsonObject value{{"task_id", id},
                    {"operation", operation},
                    {"status", "running"},
                    {"stage", "preparing"},
                    {"identity", identity(updater.systemContract())},
                    {"system_contract_sha256", contractHash},
                    {"source_task_id", params.contains("source_task_id")
                                           ? params["source_task_id"]
                                           : QJsonValue(QJsonValue::Null)},
                    {"created_at_ms",
                     static_cast<double>(QDateTime::currentMSecsSinceEpoch())},
                    {"stages", QJsonArray{}},
                    {"candidate", source.isEmpty()
                                      ? QJsonValue(QJsonValue::Null)
                                      : source["candidate"]}};
  durableJson(path, value);
  durableJson(taskRoot + "/latest.json", {{"task_id", id}});
  auto root = taskRoot;
  std::thread([value, source, configuration, path, root, ownership]() mutable {
    QElapsedTimer elapsed, stageTime;
    elapsed.start();
    stageTime.start();
    auto progress = [&](const QString &stage) {
      auto stages = value["stages"].toArray();
      stages.append(QJsonObject{
          {"stage", value["stage"]},
          {"elapsed_ms", static_cast<double>(stageTime.restart())}});
      value["stages"] = stages;
      value["stage"] = stage;
      durableJson(path, value);
    };
    try {
      NativeUpdater updater(configuration);
      updater.progress = progress;
      require(hashFile(configuration["system_contract"].toString()) ==
                  value["system_contract_sha256"].toString(),
              "System identity changed", "identity");
      if (value["operation"] == "check") {
        auto candidate = updater.check();
        value["candidate"] = candidate.isEmpty() ? QJsonValue(QJsonValue::Null)
                                                 : QJsonValue(candidate);
      } else if (value["operation"] == "download") {
        auto target =
            updater.download(value["candidate"].toObject(),
                             root + '/' + value["task_id"].toString());
        value["package_filename"] = target["filename"];
      } else {
        auto dir = root + '/' + source["task_id"].toString();
        auto candidate = value["candidate"].toObject();
        require(
            hashFile(dir + "/application-release.json") ==
                    candidate["manifest_sha256"].toString() &&
                hashFile(dir + '/' + source["package_filename"].toString()) ==
                    candidate["package_sha256"].toString(),
            "Downloaded artifacts changed");
        value["release_directory"] =
            updater.install(dir + "/application-release.json",
                            dir + "/application-release.json.sig",
                            dir + '/' + source["package_filename"].toString());
      }
      progress("completed");
      value["status"] = "success";
    } catch (const Failure &error) {
      value["status"] = "failed";
      value["error"] = errorObject(error);
    } catch (...) {
      value["status"] = "failed";
      value["error"] = QJsonObject{{"code", "internal-error"},
                                   {"stage", value["stage"]},
                                   {"message", "System update task failed"}};
    }
    value["elapsed_ms"] = static_cast<double>(elapsed.elapsed());
    value["finished_at_ms"] =
        static_cast<double>(QDateTime::currentMSecsSinceEpoch());
    try {
      durableJson(path, value);
    } catch (...) { /* Existing running state is recovered as interrupted on
                       restart. */
    }
  }).detach();
  return value;
}
QJsonValue RuntimeService::dispatch(const QJsonObject &request) {
  require(!shutdownOwnership, "System service is stopping", "service",
          "service-stopping");
  require(request.size() == 3 && request["protocol"].isDouble() &&
              request["protocol"].toDouble() == 1 &&
              request["params"].isObject(),
          "Invalid platform request");
  auto method = request["method"].toString();
  auto params = request["params"].toObject();
  if (method == "service.shutdown") {
    require(params.isEmpty(), "Shutdown has no request fields");
    // Hold the same lock as every independent task until process exit. Never
    // terminate an installation merely because init was asked to stop us.
    NativeUpdater updater(readJson(updaterConfig));
    std::unique_ptr<FileLock> taskLock(new FileLock(taskRoot + "/.task.lock"));
    std::unique_ptr<FileLock> updateLock(
        new FileLock(updater.applicationRoot() + "/.update.lock"));
    std::unique_ptr<FileLock> controlLock(new FileLock(updater.controlLock()));
    shutdownOwnership = std::move(taskLock);
    shutdownUpdateOwnership = std::move(updateLock);
    shutdownControlOwnership = std::move(controlLock);
    QTimer::singleShot(50, QCoreApplication::instance(),
                       &QCoreApplication::quit);
    return QJsonObject{{"status", "stopping"}};
  }
  if (method == "display.brightness.get" ||
      method == "display.brightness.set") {
    NativeUpdater updater(readJson(updaterConfig));
    return displayBrightness(config, updater, params,
                             method == "display.brightness.set");
  }
  if (method == "update.start")
    return startTask(params);
  if (method == "update.status") {
    require(params.isEmpty() ||
                (params.size() == 1 && params.contains("task_id")),
            "Invalid status fields");
    auto value = task(params["task_id"].toString());
    return value.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(value);
  }
  if (method == "update.identity") {
    require(params.isEmpty(), "Identity has no request fields");
    NativeUpdater updater(readJson(updaterConfig));
    return QJsonObject{{"system", updater.systemContract()},
                       {"installed", updater.installed()}};
  }
  throw Failure("method-denied", "authorization",
                "Platform method not available");
}
QJsonValue platformRequest(const QString &path, const QString &method,
                           const QJsonObject &params) {
  QLocalSocket socket;
  socket.connectToServer(path);
  require(socket.waitForConnected(3000), "System platform service unavailable",
          "service", "service-unavailable");
  auto body =
      QJsonDocument(
          QJsonObject{{"protocol", 1}, {"method", method}, {"params", params}})
          .toJson(QJsonDocument::Compact) +
      '\n';
  require(body.size() <= 1024 * 1024 && socket.write(body) == body.size() &&
              socket.waitForBytesWritten(3000),
          "Platform request write failed", "service");
  QByteArray received;
  QElapsedTimer clock;
  clock.start();
  while (!received.contains('\n')) {
    require(clock.elapsed() < 5000 &&
                socket.state() == QLocalSocket::ConnectedState,
            "Platform response timed out", "service");
    socket.waitForReadyRead(1000);
    received += socket.readAll();
    require(received.size() <= 1024 * 1024, "Platform response too large",
            "service");
  }
  auto value = strictJson(received.trimmed()).toObject();
  require(value["protocol"].toDouble() == 1 && value["ok"].isBool(),
          "Invalid platform response", "service");
  if (!value["ok"].toBool()) {
    auto error = value["error"].toObject();
    throw Failure(error["code"].toString(), error["stage"].toString(),
                  error["message"].toString());
  }
  return value["result"];
}
