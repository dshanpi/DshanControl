#include "applicationsupervision.h"
#include "applicationhealth.h"
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSet>
#include <limits>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <time.h>
#include <unistd.h>

namespace {
double monotonic() {
  struct timespec now;
  require(::clock_gettime(CLOCK_BOOTTIME, &now) == 0,
          "Supervision monotonic clock unavailable", "supervision");
  return double(now.tv_sec) + now.tv_nsec / 1e9;
}
int bounded(const QJsonObject &policy, const QString &key, int fallback,
            int minimum, int maximum) {
  if (!policy.contains(key))
    return fallback;
  const auto value = policy[key];
  require(positiveInteger(value) && value.toDouble() >= minimum &&
              value.toDouble() <= maximum,
          "Invalid supervision interval/threshold", "supervision");
  return int(value.toDouble());
}
QString safeField(const QJsonObject &error, const QString &name,
                  const QString &fallback) {
  const auto value = error[name].toString();
  return QRegularExpression("^[A-Za-z0-9._-]{1,96}$").match(value).hasMatch()
             ? value
             : fallback;
}
} // namespace

ApplicationSupervision::ApplicationSupervision(const QJsonObject &service,
                                               const QJsonObject &updater,
                                               QObject *parent)
    : QObject(parent), configuration(updater),
      updaterConfig(service["updater_config"].toString()) {
  require(!service.contains("supervision") || service["supervision"].isObject(),
          "Invalid native supervision policy", "supervision");
  const auto policy = service["supervision"].toObject();
  const QSet<QString> allowed{"enabled", "interval_seconds",
                              "failure_threshold", "timeout_seconds"};
  for (const auto &key : policy.keys())
    require(allowed.contains(key), "Unknown supervision field", "supervision");
  require(!policy.contains("enabled") || policy["enabled"].isBool(),
          "Supervision enabled must be boolean", "supervision");
  const bool enabled = policy["enabled"].toBool(false);
  const auto seconds = bounded(policy, "interval_seconds", 10, 2, 300);
  threshold = bounded(policy, "failure_threshold", 3, 2, 100);
  timeout = bounded(policy, "timeout_seconds", 10, 5, 30) * 1000;
  observation = {
      {"enabled", enabled},        {"status", enabled ? "pending" : "disabled"},
      {"consecutive_failures", 0}, {"alarm", false},
      {"restart_count", 0},        {"hardware_watchdog", false}};
  if (!enabled)
    return;
  const auto health = updater["application_health"].toObject();
  require(updater["development_mode"].toBool(false) ||
              (health["launch_receipts_required"] == true &&
               health["native_supervisor"] == true),
          "Native supervision requires managed production roles",
          "supervision");
  QProcessEnvironment environment;
  environment.insert("PATH", "/usr/bin:/bin");
  environment.insert("HOME", "/nonexistent");
  environment.insert("LANG", "C");
  helper.setProcessEnvironment(environment);
  deadline.setSingleShot(true);
  connect(&deadline, &QTimer::timeout, this, [this]() {
    if (running) {
      timedOut = true;
      helper.kill();
    }
  });
  connect(&helper, &QProcess::readyReadStandardOutput, this,
          [this]() { readOutput(); });
  connect(&helper, &QProcess::readyReadStandardError, this,
          [this]() { readOutput(); });
  connect(&helper,
          QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
          [this](int code, QProcess::ExitStatus status) {
            complete(code, status);
          });
  connect(&helper, &QProcess::errorOccurred, this,
          [this](QProcess::ProcessError error) {
            if (running && error == QProcess::FailedToStart) {
              running = false;
              deadline.stop();
              failed("supervision", "probe-start-failed");
            }
          });
  connect(&interval, &QTimer::timeout, this, [this]() { probe(); });
  interval.start(seconds * 1000);
  QTimer::singleShot(0, this, [this]() { probe(); });
}
ApplicationSupervision::~ApplicationSupervision() {
  interval.stop();
  deadline.stop();
  running = false;
  if (helper.state() != QProcess::NotRunning) {
    helper.kill();
    helper.waitForFinished(1000);
  }
}
bool ApplicationSupervision::idle() {
  // Observe kernel flock records without ever acquiring an application lease:
  // even a brief monitoring lock could make an explicit update fail as busy.
  // Linux5.4 fs/locks.c prints major/minor in hex and inode in decimal.
  QSet<QString> expected;
  const auto root = configuration["application_root"].toString();
  for (const auto &path :
       {root + "/.update.lock",
        configuration["control_lock"].toString(root + "/.control.lock")}) {
    struct stat info;
    require(::lstat(QFile::encodeName(path).constData(), &info) == 0 &&
                S_ISREG(info.st_mode) && info.st_nlink == 1 &&
                info.st_uid == ::geteuid() && !(info.st_mode & 0077),
            "Supervision lease identity unavailable", "supervision");
    expected.insert(QString::number(major(info.st_dev), 16) + ':' +
                    QString::number(minor(info.st_dev), 16) + ':' +
                    QString::number(qulonglong(info.st_ino)));
  }
  for (const auto &line : readBytes("/proc/locks", 1024 * 1024).split('\n')) {
    const auto fields = line.simplified().split(' ');
    const auto kind = fields.indexOf("FLOCK");
    if (kind < 0)
      continue;
    require(fields.size() > kind + 4, "Invalid kernel flock observation",
            "supervision");
    const auto identity = fields[kind + 4].split(':');
    require(identity.size() == 3, "Invalid kernel flock identity",
            "supervision");
    QStringList parts;
    for (int index = 0; index < 3; ++index) {
      bool valid = false;
      const auto value =
          identity[index].toULongLong(&valid, index < 2 ? 16 : 10);
      require(valid, "Invalid kernel flock number", "supervision");
      parts << QString::number(value, index < 2 ? 16 : 10);
    }
    if (expected.contains(parts.join(':'))) {
      observation["status"] = "busy";
      observation["observed_monotonic_seconds"] = monotonic();
      return false;
    }
  }
  return true;
}
void ApplicationSupervision::probe() {
  if (running)
    return;
  try {
    if (!idle())
      return;
    expectedCommit = applicationIdentity(configuration)["commit"].toString();
    expectedCurrent =
        QFileInfo(configuration["application_root"].toString() + "/current")
            .canonicalFilePath();
    output.clear();
    errorOutput.clear();
    timedOut = oversized = false;
    running = true;
    helper.start(QCoreApplication::applicationFilePath(),
                 {"--config", updaterConfig, "application-health"});
    deadline.start(timeout);
  } catch (const Failure &error) {
    failed(error.stage, error.code);
  }
}
void ApplicationSupervision::readOutput() {
  output += helper.readAllStandardOutput();
  errorOutput += helper.readAllStandardError();
  if (output.size() > 65536 || errorOutput.size() > 65536) {
    output = output.left(65536);
    errorOutput = errorOutput.left(65536);
    oversized = true;
    helper.kill();
  }
}
void ApplicationSupervision::failed(const QString &stage, const QString &code) {
  const auto previous = observation["consecutive_failures"].toInt();
  const auto count =
      previous == std::numeric_limits<int>::max() ? previous : previous + 1;
  observation["status"] = "unhealthy";
  observation["consecutive_failures"] = count;
  observation["alarm"] = count >= threshold;
  observation["error"] = QJsonObject{{"stage", stage}, {"code", code}};
  observation["observed_monotonic_seconds"] = monotonic();
}
void ApplicationSupervision::complete(int exitCode,
                                      QProcess::ExitStatus status) {
  if (!running)
    return;
  running = false;
  deadline.stop();
  readOutput();
  try {
    if (!idle())
      return;
    if (applicationIdentity(configuration)["commit"].toString() !=
            expectedCommit ||
        QFileInfo(configuration["application_root"].toString() + "/current")
                .canonicalFilePath() != expectedCurrent) {
      observation["status"] = "changed";
      observation["observed_monotonic_seconds"] = monotonic();
      return;
    }
    if (timedOut || oversized || status != QProcess::NormalExit) {
      failed("supervision",
             timedOut ? "probe-timeout"
                      : oversized ? "probe-output-limit" : "probe-crashed");
      return;
    }
    if (exitCode != 0) {
      const auto error = QJsonDocument::fromJson(errorOutput).object();
      failed(safeField(error, "stage", "supervision"),
             safeField(error, "code", "probe-failed"));
      return;
    }
    const auto result = QJsonDocument::fromJson(output).object();
    if (result["ok"] != true ||
        result["application_commit"] != expectedCommit) {
      failed("supervision", "probe-identity");
      return;
    }
    observation["status"] = "healthy";
    observation["consecutive_failures"] = 0;
    observation["alarm"] = false;
    observation["application_commit"] = expectedCommit;
    observation.remove("error");
    observation["observed_monotonic_seconds"] = monotonic();
  } catch (const Failure &error) {
    failed(error.stage, error.code);
  }
}
