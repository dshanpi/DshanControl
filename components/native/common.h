#pragma once
#include "../runtime/src/storage.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSet>
#include <cmath>
#include <unistd.h>

namespace Native {
inline void check(bool value, const QString &message,
                  const QString &code = "ValueError") {
  require(value, message, "native", code);
}
inline QString env(const char *key, const QString &fallback) {
  const auto value = qEnvironmentVariable(key);
  return value.isEmpty() ? fallback : value;
}
inline QByteArray optional(const QString &path, qint64 limit = 1024 * 1024) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
    return {};
  auto data = file.read(limit + 1);
  check(data.size() <= limit, "File exceeds limit");
  return data;
}
inline QJsonObject object(const QByteArray &data) {
  const auto value = strictJson(data);
  check(value.isObject(), "JSON object required");
  return value.toObject();
}
inline void fields(const QJsonObject &value, const QSet<QString> &allowed,
                   const QSet<QString> &required = {}) {
  for (auto i = value.begin(); i != value.end(); ++i)
    check(allowed.contains(i.key()), "Unexpected parameter: " + i.key());
  for (const auto &key : required)
    check(value.contains(key), "Missing parameter: " + key);
}
inline qint64 integer(const QJsonValue &value, const QString &name, qint64 low,
                      qint64 high) {
  qint64 result = 0;
  bool ok = false;
  if (value.isDouble()) {
    const auto n = value.toDouble();
    ok = std::isfinite(n) && n >= double(low) && n <= double(high) &&
         std::floor(n) == n;
    if (ok)
      result = qint64(n);
  } else if (value.isString()) {
    auto s = value.toString();
    ok = QRegularExpression("^-?(?:0[xX][0-9a-fA-F]+|[0-9]+)$")
             .match(s)
             .hasMatch();
    if (ok)
      result =
          s.toLongLong(&ok, s.contains('x', Qt::CaseInsensitive) ? 16 : 10);
  }
  check(ok && result >= low && result <= high,
        name + ": invalid integer or outside range");
  return result;
}
inline bool boolean(const QJsonObject &p, const QString &key,
                    bool fallback = false) {
  if (!p.contains(key))
    return fallback;
  check(p[key].isBool(), key + ": boolean required");
  return p[key].toBool();
}
struct ProcessResult {
  int code;
  QByteArray output, error;
};
inline ProcessResult run(const QString &program, const QStringList &args,
                         int timeout = 5000) {
  QProcess process;
  QProcessEnvironment e;
  e.insert("PATH", "/usr/sbin:/usr/bin:/sbin:/bin");
  e.insert("LANG", "C.UTF-8");
  process.setProcessEnvironment(e);
  process.start(program, args);
  check(process.waitForStarted(2000), "Executable unavailable: " + program,
        "ToolUnavailable");
  process.closeWriteChannel();
  QElapsedTimer timer;
  timer.start();
  QByteArray output, error;
  while (process.state() != QProcess::NotRunning) {
    process.waitForFinished(20);
    output += process.readAllStandardOutput();
    error += process.readAllStandardError();
    if (timer.elapsed() > timeout || output.size() > 65536 ||
        error.size() > 8192) {
      process.kill();
      process.waitForFinished(1000);
      throw Failure("TimeoutError", "native",
                    "Tool budget exceeded; outcome may be unknown");
    }
  }
  output += process.readAllStandardOutput();
  error += process.readAllStandardError();
  check(output.size() <= 65536 && error.size() <= 8192,
        "Tool response exceeds limit");
  return {process.exitStatus() == QProcess::NormalExit ? process.exitCode()
                                                       : -1,
          output, error};
}
inline QByteArray command(const QString &program, const QStringList &args,
                          int timeout = 5000) {
  auto r = run(program, args, timeout);
  check(r.code == 0,
        QString::fromUtf8(r.error.isEmpty() ? r.output : r.error).left(2000),
        "ToolFailed");
  return r.output;
}
inline QJsonObject rpc(const QString &path, const QString &method,
                       const QJsonObject &params, int timeout = 20000) {
  QLocalSocket socket;
  socket.connectToServer(path);
  check(socket.waitForConnected(3000), "Native service unavailable",
        "Unavailable");
  socket.write(
      QJsonDocument(
          QJsonObject{{"protocol", 1}, {"method", method}, {"params", params}})
          .toJson(QJsonDocument::Compact) +
      '\n');
  check(socket.waitForBytesWritten(3000), "RPC write failed", "Unavailable");
  QElapsedTimer timer;
  timer.start();
  QByteArray data;
  while (!data.contains('\n')) {
    check(timer.elapsed() < timeout, "RPC timeout; outcome may be unknown",
          "TimeoutError");
    socket.waitForReadyRead(qMin(100, timeout - int(timer.elapsed())));
    data += socket.readAll();
    check(data.size() <= 1024 * 1024, "RPC response too large");
    check(socket.state() != QLocalSocket::UnconnectedState ||
              data.contains('\n'),
          "RPC closed without response");
  }
  const auto response = object(data.left(data.indexOf('\n')));
  check(response["protocol"] == 1 && response["ok"].isBool(),
        "Invalid RPC envelope");
  if (response["ok"] != true) {
    auto e = response["error"].toObject();
    throw Failure(e["code"].toString(), e["stage"].toString(),
                  e["message"].toString());
  }
  check(response["result"].isObject(), "Invalid RPC result");
  return response["result"].toObject();
}
inline void output(const QJsonObject &value) {
  const auto bytes = QJsonDocument(value).toJson(QJsonDocument::Compact) + '\n';
  QFile out;
  out.open(stdout, QIODevice::WriteOnly);
  out.write(bytes);
  out.flush();
}
} // namespace Native
