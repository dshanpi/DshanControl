#include "applicationprocess.h"
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QRegularExpression>
#include <errno.h>
#include <fcntl.h>
#include <memory>
#include <poll.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <vector>

namespace {
#if defined(SYS_pidfd_open) && defined(SYS_pidfd_send_signal)
constexpr long PidfdOpen = SYS_pidfd_open, PidfdSignal = SYS_pidfd_send_signal;
#elif defined(__arm__)
// Selected vendor Linux5.4 arch/arm/tools/syscall.tbl entries424 and434.
// The old cross-toolchain headers predate their libc names. No kill(PID)
// fallback.
constexpr long PidfdOpen = 434, PidfdSignal = 424;
#else
#error                                                                         \
    "Target pidfd syscall numbers require an audited kernel/toolchain mapping"
#endif
void processRequire(bool condition, const QString &stage, const QString &detail,
                    const QString &suffix = "process") {
  require(condition, detail, stage, stage + '-' + suffix);
}
class Owned {
  QString stage, name;
  int directory = -1, file = -1, handle = -1;
  struct stat original;
  QByteArray content;

public:
  qint64 pid = 0;
  bool absent = false, exited = false;
  Owned(const QString &root, const QString &role, const QStringList &expected,
        const QString &phase, bool missing)
      : stage(phase), name(role + ".pid") {
    try {
      directory = ::open(QFile::encodeName(root).constData(),
                         O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
      struct stat parent;
      processRequire(directory >= 0 && ::fstat(directory, &parent) == 0 &&
                         parent.st_uid == ::geteuid() &&
                         !(parent.st_mode & 0022),
                     stage, "Untrusted application PID directory");
      file = ::openat(directory, QFile::encodeName(name).constData(),
                      O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
      if (file < 0 && errno == ENOENT && missing) {
        absent = true;
        return;
      }
      processRequire(
          file >= 0 && ::fstat(file, &original) == 0 &&
              S_ISREG(original.st_mode) && original.st_nlink == 1 &&
              original.st_uid == ::geteuid() && !(original.st_mode & 0022),
          stage, "Required application PID file is missing or unsafe");
      char raw[65];
      auto count = ::read(file, raw, sizeof(raw));
      processRequire(count > 0 && count < ssize_t(sizeof(raw)), stage,
                     "Invalid application PID file size");
      content = QByteArray(raw, int(count));
      auto text = QString::fromLatin1(content).trimmed();
      bool valid = false;
      pid = text.toLongLong(&valid);
      processRequire(
          valid && QRegularExpression("^[0-9]+$").match(text).hasMatch() &&
              pid > 1 && pid <= 2147483647,
          stage, "Invalid application PID");
      // pid_t must stay32-bit in the variadic syscall ABI on the ARM target.
      handle = int(::syscall(PidfdOpen, static_cast<pid_t>(pid), 0));
      const auto openError = errno;
      if (handle < 0 && openError == ESRCH && missing) {
        exited = true;
        return;
      }
      processRequire(handle >= 0, stage,
                     "Kernel application process handle unavailable; errno=" +
                         QString::number(openError),
                     "process-handle");
      exited = isExited();
      if (exited && missing)
        return;
      processRequire(!exited, stage, "Required application process has exited");
      const auto base = "/proc/" + QString::number(pid);
      struct stat process;
      processRequire(::stat(QFile::encodeName(base).constData(), &process) ==
                             0 &&
                         process.st_uid == ::geteuid(),
                     stage, "Application process user mismatch");
      const auto executable = QFileInfo(base + "/exe").canonicalFilePath();
      const auto wanted = QFileInfo(expected.first()).canonicalFilePath();
      auto actual = readBytes(base + "/cmdline", 65536).split('\0');
      if (!actual.isEmpty() && actual.last().isEmpty())
        actual.removeLast();
      QList<QByteArray> args;
      for (const auto &part : expected)
        args << part.toUtf8();
      processRequire(
          !executable.isEmpty() && !wanted.isEmpty() && executable == wanted &&
              actual == args,
          stage, "Application PID belongs to another executable or invocation");
    } catch (...) {
      close();
      throw;
    }
  }
  ~Owned() { close(); }
  void close() {
    if (handle >= 0)
      ::close(handle);
    if (file >= 0)
      ::close(file);
    if (directory >= 0)
      ::close(directory);
    handle = file = directory = -1;
  }
  bool isExited() const {
    struct pollfd item {
      handle, POLLIN, 0
    };
    const auto result = ::poll(&item, 1, 0);
    processRequire(result >= 0 && !(item.revents & (POLLERR | POLLNVAL)), stage,
                   "Cannot inspect application process handle");
    return result > 0 && (item.revents & (POLLIN | POLLHUP));
  }
  void stop(int seconds) {
    if (absent)
      return;
    if (!exited && !isExited()) {
      const auto result = ::syscall(PidfdSignal, handle, SIGTERM, nullptr, 0);
      processRequire(result == 0 || errno == ESRCH, stage,
                     "Cannot signal owned application process",
                     "process-signal");
      QElapsedTimer deadline;
      deadline.start();
      while (!isExited() && deadline.elapsed() < seconds * 1000) {
        struct pollfd item {
          handle, POLLIN, 0
        };
        auto result =
            ::poll(&item, 1,
                   int(qMax<qint64>(1, seconds * 1000 - deadline.elapsed())));
        processRequire(result >= 0 || errno == EINTR, stage,
                       "Owned process exit wait failed");
      }
      processRequire(isExited(), stage,
                     "Owned application process did not exit; PID file "
                     "retained; no force kill",
                     "process-timeout");
    }
    struct stat current;
    char raw[65];
    auto count = ::pread(file, raw, sizeof(raw), 0);
    processRequire(::fstatat(directory, QFile::encodeName(name).constData(),
                             &current, AT_SYMLINK_NOFOLLOW) == 0 &&
                       current.st_dev == original.st_dev &&
                       current.st_ino == original.st_ino &&
                       S_ISREG(current.st_mode) && count == content.size() &&
                       QByteArray(raw, int(qMax<ssize_t>(0, count))) == content,
                   stage, "Application PID file changed; replacement preserved",
                   "pidfile-changed");
    processRequire(
        ::unlinkat(directory, QFile::encodeName(name).constData(), 0) == 0 &&
            ::fsync(directory) == 0,
        stage, "Cannot clear stopped application PID file");
  }
};
} // namespace
ApplicationProcesses::ApplicationProcesses(const QJsonObject &config,
                                           const QString &phase)
    : stage(phase) {
  processRequire(!config.contains("development_mode") ||
                     config["development_mode"].isBool(),
                 stage, "Invalid development mode", "contract");
  const bool dev = config["development_mode"].toBool(false);
  const auto contract = readJson(config["system_contract"].toString());
  const auto root = config["application_root"].toString();
  processRequire(QDir::isAbsolutePath(root) && root != "/" &&
                     QDir::cleanPath(root) == root &&
                     root == contract["application_root"].toString(),
                 stage, "Invalid managed application root", "contract");
  processRequire(!config.contains("application_health") ||
                     config["application_health"].isObject(),
                 stage, "Invalid application health policy", "contract");
  const auto policy = config["application_health"].toObject();
  pidRoot = policy["pid_root"].toString("/var/run/dshancontrol");
  processRequire(QDir::isAbsolutePath(pidRoot) && pidRoot != "/" &&
                     QDir::cleanPath(pidRoot) == pidRoot &&
                     !QFileInfo(pidRoot).isSymLink(),
                 stage, "Invalid application PID root", "contract");
  commands = {
      {"web", {"/usr/bin/python3", root + "/current/lib/web/app.py"}},
      {"hmi", {root + "/current/bin/omnigate-hmi"}},
      {"supervisor",
       {"/usr/bin/python3", root + "/current/lib/supervisor/supervisor.py"}}};
  if (policy.contains("fixture_processes")) {
    const auto fixture = policy["fixture_processes"].toObject();
    processRequire(
        dev && policy["fixture_processes"].isObject() && fixture.size() == 3,
        stage,
        "Fixture process overrides require development mode and three roles",
        "contract");
    for (auto it = commands.begin(); it != commands.end(); ++it) {
      const auto array = fixture[it.key()].toArray();
      QStringList args;
      processRequire(!array.isEmpty(), stage, "Missing fixture process",
                     "contract");
      for (const auto &arg : array) {
        processRequire(arg.isString() && !arg.toString().contains(QChar(0)),
                       stage, "Invalid fixture argv", "contract");
        args << arg.toString();
      }
      processRequire(QDir::isAbsolutePath(args.first()), stage,
                     "Fixture executable must be absolute", "contract");
      it.value() = args;
    }
  }
}
QJsonObject ApplicationProcesses::health() const {
  QJsonObject pids;
  for (auto it = commands.cbegin(); it != commands.cend(); ++it) {
    Owned process(pidRoot, it.key(), it.value(), stage, false);
    pids[it.key()] = double(process.pid);
  }
  return pids;
}
QJsonObject ApplicationProcesses::stop(const QString &role) const {
  processRequire(role.isEmpty() || commands.contains(role), stage,
                 "Invalid application role", "contract");
  if (!QFileInfo(pidRoot).exists())
    return {{"ok", true}, {"cleared_pid_files", 0}};
  struct stat parent;
  processRequire(::lstat(QFile::encodeName(pidRoot).constData(), &parent) ==
                         0 &&
                     S_ISDIR(parent.st_mode) && parent.st_uid == ::geteuid() &&
                     !(parent.st_mode & 0022),
                 stage, "Untrusted application PID directory");
  FileLock lock(pidRoot + "/.lifecycle.lock");
  std::vector<std::unique_ptr<Owned>> processes;
  for (const auto &name :
       {QString("supervisor"), QString("hmi"), QString("web")})
    if (role.isEmpty() || role == name)
      processes.emplace_back(
          new Owned(pidRoot, name, commands[name], stage, true));
  // Verify every live identity before signalling the first process.
  int count = 0;
  for (const auto &process : processes) {
    process->stop(10);
    if (!process->absent)
      ++count;
  }
  return {{"ok", true}, {"cleared_pid_files", count}};
}
