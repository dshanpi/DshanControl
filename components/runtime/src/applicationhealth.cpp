#include "applicationhealth.h"
#include "applicationprocess.h"
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QLocalSocket>
#include <QMap>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>
#include <fcntl.h>
#include <memory>
#include <sqlite3.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

namespace {
void healthRequire(bool ok, const QString &code, const QString &detail) {
  require(ok, detail, "health", code);
}
struct Scope {
  QJsonObject contract, installed, policy;
  QString root, current, data;
  bool development;
  explicit Scope(const QJsonObject &config) {
    healthRequire(!config.contains("development_mode") ||
                      config["development_mode"].isBool(),
                  "health-contract", "Invalid development mode");
    development = config["development_mode"].toBool(false);
    contract = readJson(config["system_contract"].toString());
    root = config["application_root"].toString();
    healthRequire(QDir::isAbsolutePath(root) && root != "/" &&
                      QDir::cleanPath(root) == root &&
                      root == contract["application_root"].toString(),
                  "health-contract", "Application root mismatch");
    if (!development)
      verifyPersistentStorage(root, contract);
    auto releases = QFileInfo(root + "/releases").canonicalFilePath();
    current = QFileInfo(root + "/current").canonicalFilePath();
    healthRequire(
        !QFileInfo(root).isSymLink() &&
            !QFileInfo(root + "/releases").isSymLink() && !releases.isEmpty() &&
            QFileInfo(root + "/current").isSymLink() && !current.isEmpty() &&
            QFileInfo(current).absolutePath() == releases,
        "health-identity", "Current application outside managed releases");
    healthRequire(!QFileInfo(current + "/installed.json").isSymLink(),
                  "health-identity", "Application identity must not be linked");
    installed = readJson(current + "/installed.json");
    healthRequire(
        QRegularExpression("^[a-f0-9]{40}$")
                .match(installed["commit"].toString())
                .hasMatch() &&
            installed["data_schema"] == 1 && contract["data_schema"] == 1,
        "health-identity", "Application identity/data schema missing");
    policy = config["application_health"].toObject();
    healthRequire(!config.contains("application_health") ||
                      config["application_health"].isObject(),
                  "health-contract", "Invalid health policy");
    data = contract["data_root"].toString();
  }
};
void desktopHealth(const Scope &scope, qint64 expectedPid) {
  const auto override = scope.policy.value("fixture_desktop_socket");
  healthRequire(
      override.isUndefined() || (scope.development && override.isString()),
      "health-contract", "Desktop socket override requires development mode");
  const auto path = override.toString("/run/dshancontrol/desktop/desktop.sock");
  healthRequire(QDir::isAbsolutePath(path) && QDir::cleanPath(path) == path &&
                    !QFileInfo(path).isSymLink(),
                "health-contract", "Invalid desktop socket");
  QLocalSocket socket;
  socket.setReadBufferSize(524289);
  QElapsedTimer elapsed;
  elapsed.start();
  socket.connectToServer(path);
  healthRequire(socket.waitForConnected(500), "health-desktop",
                "Desktop IPC unavailable");
  struct ucred peer;
  socklen_t size = sizeof(peer);
  healthRequire(::getsockopt(int(socket.socketDescriptor()), SOL_SOCKET,
                             SO_PEERCRED, &peer, &size) == 0 &&
                    size == sizeof(peer) && peer.pid == expectedPid,
                "health-desktop-identity",
                "Desktop IPC differs from managed process");
  const QByteArray request =
      "{\"protocol\":1,\"method\":\"desktop.snapshot\",\"params\":{}}\n";
  healthRequire(socket.write(request) == request.size(), "health-desktop",
                "Desktop health write failed");
  while (socket.bytesToWrite() && elapsed.elapsed() < 1500)
    if (!socket.waitForBytesWritten(
            int(qMax<qint64>(1, 1500 - elapsed.elapsed()))))
      break;
  healthRequire(!socket.bytesToWrite(), "health-desktop",
                "Desktop health write timed out");
  QByteArray bytes;
  while (!bytes.contains('\n') && elapsed.elapsed() < 1500) {
    bytes += socket.readAll();
    healthRequire(bytes.size() <= 524288, "health-desktop",
                  "Desktop response exceeds bound");
    if (bytes.contains('\n'))
      break;
    if (!socket.waitForReadyRead(
            int(qMax<qint64>(1, 1500 - elapsed.elapsed()))))
      break;
  }
  bytes += socket.readAll();
  healthRequire(
      bytes.size() <= 524288 && bytes.endsWith('\n') && bytes.count('\n') == 1,
      "health-desktop", "Desktop health response missing or malformed");
  const auto value = strictJson(bytes.trimmed());
  const auto result = value.toObject();
  healthRequire(value.isObject() && result["protocol"] == 1 &&
                    result["ok"] == true &&
                    result["result"].toObject()["version"] == 1,
                "health-desktop",
                "Desktop provider rejected health or protocol mismatched");
}
QJsonObject webHealth(const QUrl &url) {
  healthRequire(
      url.isValid() && url.scheme() == "http" && url.host() == "127.0.0.1" &&
          url.userInfo().isEmpty() && url.path() == "/api/health" &&
          !url.hasQuery() && !url.hasFragment(),
      "health-contract", "Health endpoint must be fixed loopback API");
  QNetworkAccessManager manager;
  manager.setProxy(QNetworkProxy::NoProxy);
  QNetworkRequest request(url);
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::ManualRedirectPolicy);
  QNetworkReply *reply = manager.get(request);
  reply->setReadBufferSize(65537);
  QEventLoop loop;
  QTimer timer;
  timer.setSingleShot(true);
  bool timeout = false, large = false;
  QByteArray bytes;
  auto read = [&] {
    if (large || !reply->isOpen())
      return;
    if (reply->bytesAvailable() > 0)
      bytes += reply->read(65537 - bytes.size());
    if (bytes.size() > 65536) {
      large = true;
      reply->abort();
    }
  };
  QObject::connect(reply, &QNetworkReply::readyRead, &loop, read);
  QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  QObject::connect(&timer, &QTimer::timeout, &loop, [&] {
    timeout = true;
    reply->abort();
  });
  timer.start(3000);
  loop.exec();
  timer.stop();
  read();
  auto code =
      reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  auto error = reply->error();
  healthRequire(!timeout, "health-web-timeout",
                "Application health response timed out");
  healthRequire(!large, "health-web-size",
                "Application health response exceeds limit");
  healthRequire(code == 200 && error == QNetworkReply::NoError,
                "health-web-http", "Application health HTTP failed");
  try {
    auto result = strictJson(bytes);
    healthRequire(result.isObject(), "health-web-identity",
                  "Invalid health response");
    return result.toObject();
  } catch (const Failure &) {
    throw Failure("health-web-identity", "health", "Invalid health response");
  }
}
void database(const QString &path, const QStringList &tables, bool core) {
  healthRequire(QFileInfo(path).isFile() && !QFileInfo(path).isSymLink(),
                "health-database", "Database is missing or linked");
  sqlite3 *raw = nullptr;
  auto encoded = QFile::encodeName(path);
  const int rc =
      sqlite3_open_v2(encoded.constData(), &raw,
                      SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX, nullptr);
  std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(raw, sqlite3_close);
  healthRequire(rc == SQLITE_OK, "health-database",
                "Database cannot be opened read-only");
  sqlite3_busy_timeout(raw, 2000);
  QElapsedTimer elapsed;
  elapsed.start();
  sqlite3_progress_handler(
      raw, 1000,
      [](void *p) -> int {
        return static_cast<QElapsedTimer *>(p)->elapsed() > 2000 ? 1 : 0;
      },
      &elapsed);
  sqlite3_stmt *stmt = nullptr;
  auto query = [&](const QByteArray &sql) {
    if (stmt) {
      sqlite3_finalize(stmt);
      stmt = nullptr;
    }
    healthRequire(sqlite3_prepare_v2(raw, sql.constData(), -1, &stmt,
                                     nullptr) == SQLITE_OK,
                  "health-database", "Database schema/query rejected");
  };
  try {
    query("PRAGMA quick_check");
    healthRequire(sqlite3_step(stmt) == SQLITE_ROW &&
                      sqlite3_column_text(stmt, 0) &&
                      QByteArray(reinterpret_cast<const char *>(
                          sqlite3_column_text(stmt, 0))) == "ok" &&
                      sqlite3_step(stmt) == SQLITE_DONE,
                  "health-database", "Database consistency check failed");
    for (const auto &table : tables) {
      query("SELECT count(*) FROM sqlite_master WHERE type='table' AND name=?");
      auto name = table.toUtf8();
      sqlite3_bind_text(stmt, 1, name.constData(), name.size(),
                        SQLITE_TRANSIENT);
      healthRequire(
          sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_int(stmt, 0) == 1,
          "health-database-schema", "Expected application table missing");
    }
    if (core) {
      query("SELECT value FROM metadata WHERE key='schema_version'");
      healthRequire(
          sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_text(stmt, 0) &&
              QByteArray(reinterpret_cast<const char *>(
                  sqlite3_column_text(stmt, 0))) == "1",
          "health-database-schema", "Unsupported application database schema");
    }
    sqlite3_finalize(stmt);
    stmt = nullptr;
    sqlite3_progress_handler(raw, 0, nullptr, nullptr);
  } catch (...) {
    if (stmt)
      sqlite3_finalize(stmt);
    sqlite3_progress_handler(raw, 0, nullptr, nullptr);
    throw;
  }
}
double uptime() {
  struct timespec now;
  healthRequire(::clock_gettime(CLOCK_BOOTTIME, &now) == 0, "health-clock",
                "Monotonic boot clock unavailable");
  return double(now.tv_sec) + now.tv_nsec / 1e9;
}
void heartbeat(const QString &path, qint64 pid, const QString &commit) {
  struct stat st, process;
  healthRequire(
      ::stat(QFile::encodeName("/proc/" + QString::number(pid)).constData(),
             &process) == 0 &&
          ::lstat(QFile::encodeName(path).constData(), &st) == 0 &&
          S_ISREG(st.st_mode) && st.st_uid == process.st_uid &&
          !(st.st_mode & 0022),
      "health-hmi-heartbeat", "HMI heartbeat missing or unsafe");
  auto value = readJson(path);
  auto now = uptime();
  auto stamp = value["monotonic_seconds"].toDouble(-1);
  healthRequire(value["schema_version"] == 1 &&
                    value["pid"].toDouble() == pid &&
                    value["commit"] == commit &&
                    value["boot_id"] ==
                        QString::fromUtf8(
                            readBytes("/proc/sys/kernel/random/boot_id", 128))
                            .trimmed() &&
                    value["gui_event_loop"].isBool() &&
                    value["gui_event_loop"].toBool() &&
                    value["backend_identity_verified"].isBool() &&
                    value["backend_identity_verified"].toBool() && stamp >= 0 &&
                    stamp <= now + 0.05 && now - stamp < 6,
                "health-hmi-heartbeat",
                "HMI event-loop/backend heartbeat is stale or mismatched");
}
} // namespace
QJsonObject applicationIdentity(const QJsonObject &config) {
  return Scope(config).installed;
}
QJsonObject applicationHealth(const QJsonObject &config) {
  Scope scope(config);
  auto pids = ApplicationProcesses(config, "health")
                  .health(scope.installed["commit"].toString());
  if (scope.policy["desktop_provider"].toBool(false))
    desktopHealth(scope, qint64(pids["desktop"].toDouble()));
  auto web = webHealth(
      QUrl(scope.policy["http_url"].toString("http://127.0.0.1/api/health")));
  auto commit = scope.installed["commit"].toString();
  healthRequire(web["ok"].isBool() && web["ok"].toBool() &&
                    web["service"] == "omnigate-web" &&
                    web["application_commit"] == commit,
                "health-web-identity",
                "Backend/current application identity mismatch");
  healthRequire(QDir::isAbsolutePath(scope.data) && scope.data != "/" &&
                    QDir::cleanPath(scope.data) == scope.data &&
                    !QFileInfo(scope.data).isSymLink(),
                "health-contract", "Invalid managed data root");
  if (!scope.development)
    verifyPersistentStorage(scope.data, scope.contract);
  database(scope.data + "/edge.db", {"metadata", "devices", "points", "audit"},
           true);
  database(scope.data + "/hmi.db", {"alarms", "audit", "telemetry"}, false);
  auto required = scope.policy.value("require_hmi_heartbeat");
  healthRequire(required.isUndefined() || required.isBool(), "health-contract",
                "Heartbeat policy must be boolean");
  healthRequire(scope.contract["contract_id"] != "dshancontrol-platform-2" ||
                    required.toBool(false),
                "health-contract",
                "Platform 2 requires a functional HMI heartbeat");
  if (required.toBool(false))
    heartbeat(scope.policy["hmi_heartbeat"].toString(
                  "/var/run/dshancontrol/hmi-heartbeat.json"),
              qint64(pids["hmi"].toDouble()), commit);
  return {{"ok", true},
          {"application_commit", commit},
          {"processes", pids},
          {"databases", 2},
          {"desktop_functional_rpc",
           scope.policy["desktop_provider"].toBool(false)},
          {"hmi_functional_heartbeat", required.toBool(false)}};
}
