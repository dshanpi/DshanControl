#include "applicationlaunch.h"
#include "applicationcredentials.h"
#include "applicationhealth.h"
#include "applicationprocess.h"
#include "applicationrecord.h"
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcessEnvironment>
#include <QSet>
#include <QThread>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <linux/capability.h>
#include <memory>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {
void launchRequire(bool ok, const QString &message,
                   const QString &code = "launch-refused") {
  require(ok, message, "start", code);
}
struct Descriptor {
  int value = -1;
  ~Descriptor() {
    if (value >= 0)
      ::close(value);
  }
  void close() {
    if (value >= 0)
      ::close(value);
    value = -1;
  }
};
struct Role {
  QString name;
  QStringList arguments;
  ApplicationCredentials credentials{};
  QByteArray executable, workingDirectory;
  QList<QByteArray> argumentBytes, environmentBytes;
  std::vector<char *> argv, environment;
  Descriptor log;
};
struct ChildError {
  int step;
  int number;
};
void childError(int channel, int step, int number) {
  const ChildError value{step, number};
  const auto written = ::write(channel, &value, sizeof(value));
  if (written != sizeof(value))
    ::_exit(126);
  ::_exit(127);
}
quint32 profileNumber(const QJsonValue &value) {
  launchRequire(positiveInteger(value) && value.toDouble() <= 2147483647,
                "Invalid fixture credential identity", "launch-profile");
  return quint32(value.toDouble());
}
void trustedDirectory(const QString &path) {
  launchRequire(QDir::isAbsolutePath(path) && path != "/" &&
                    QDir::cleanPath(path) == path,
                "Invalid runtime directory");
  struct stat info;
  if (::lstat(QFile::encodeName(path).constData(), &info) < 0) {
    launchRequire(errno == ENOENT &&
                      ::mkdir(QFile::encodeName(path).constData(), 0700) == 0,
                  "Cannot create runtime directory");
    launchRequire(::lstat(QFile::encodeName(path).constData(), &info) == 0,
                  "Cannot inspect runtime directory");
  }
  launchRequire(S_ISDIR(info.st_mode) && info.st_uid == ::geteuid() &&
                    !(info.st_mode & 0022),
                "Untrusted runtime directory");
}
ApplicationCredentials credentials(const QString &name,
                                   const QJsonObject &policy, bool dev,
                                   int last) {
  ApplicationCredentials value{};
  value.lastKernelCapability = last;
  if (!dev) {
    launchRequire(::geteuid() == 0,
                  "Production launcher requires the system owner",
                  "launch-owner");
    value.uid = name == "web" ? 400 : 401;
    value.gid = 400;
    value.groupCount = 1;
    value.groups[0] = 400;
    value.bindPrivilegedPort = name == "web";
    const auto account = ::getpwnam(name == "web" ? "dshan-web" : "dshan-hmi");
    launchRequire(account && account->pw_uid == value.uid &&
                      account->pw_gid == value.gid,
                  "Dedicated application account is missing or mismatched",
                  "launch-account");
  } else {
    value.uid = ::geteuid() == 0 ? (name == "hmi" ? 60001 : 60000) : ::getuid();
    value.gid = ::geteuid() == 0 ? 60000 : ::getgid();
    if (::geteuid() == 0) {
      value.groupCount = 1;
      value.groups[0] = value.gid;
    } else {
      value.groupCount = ::getgroups(64, value.groups);
      launchRequire(value.groupCount >= 0, "Cannot inspect fixture groups");
    }
    if (policy.contains("fixture_credentials")) {
      launchRequire(policy["fixture_credentials"].isObject(),
                    "Invalid fixture credentials", "launch-profile");
      const auto entry =
          policy["fixture_credentials"].toObject()[name].toObject();
      const QSet<QString> allowed{"uid", "gid", "groups", "bind"};
      for (const auto &key : entry.keys())
        launchRequire(allowed.contains(key), "Unknown fixture credential field",
                      "launch-profile");
      value.uid = profileNumber(entry["uid"]);
      value.gid = profileNumber(entry["gid"]);
      launchRequire(
          entry["groups"].isArray() && entry["groups"].toArray().size() <= 64 &&
              entry["bind"].isBool(),
          "Invalid fixture credential groups/capability", "launch-profile");
      value.groupCount = 0;
      for (const auto &group : entry["groups"].toArray())
        value.groups[value.groupCount++] = profileNumber(group);
      value.bindPrivilegedPort = entry["bind"].toBool();
    }
  }
  launchRequire(!value.bindPrivilegedPort || name == "web",
                "Capability is not allowed for this role", "launch-profile");
  launchRequire(value.uid > 0 && value.gid > 0 && value.groupCount >= 0 &&
                    value.groupCount <= 64,
                "Nonroot role identity required", "launch-profile");
  QSet<gid_t> unique;
  for (int i = 0; i < value.groupCount; ++i) {
    launchRequire(value.groups[i] > 0 && !unique.contains(value.groups[i]),
                  "Root or duplicate supplemental group refused",
                  "launch-profile");
    unique.insert(value.groups[i]);
  }
  return value;
}
QProcessEnvironment environment(const QJsonObject &config,
                                const QString &commit, const QString &pidRoot) {
  const auto root = config["application_root"].toString();
  const auto contract = readJson(config["system_contract"].toString());
  const auto data = contract["data_root"].toString();
  QProcessEnvironment result;
  const QMap<QString, QString> fixed{
      {"PATH", "/usr/bin:/bin:/usr/sbin:/sbin"},
      {"HOME", "/nonexistent"},
      {"LANG", "C"},
      {"PYTHONDONTWRITEBYTECODE", "1"},
      {"DSHANCONTROL_APPLICATION_COMMIT", commit},
      {"OMNIGATE_CORE_PATH", root + "/current/lib/core"},
      {"PYTHONPATH",
       "/usr/lib/dshancontrol-python:" + root + "/current/lib/core"},
      {"OMNIGATE_WEB_ASSETS", root + "/current/share/web"},
      {"OMNIGATE_WEB_CONFIG", data + "/web.json"},
      {"OMNIGATE_CONFIG", data + "/web.json"},
      {"OMNIGATE_WEB_SECRET", data + "/session.key"},
      {"OMNIGATE_EDGE_DB", data + "/edge.db"},
      {"OMNIGATE_HMI_DB", data + "/hmi.db"},
      {"OMNIGATE_HMI_SETTINGS", data + "/hmi.json"},
      {"OMNIGATE_EDS_DIR", data + "/eds"},
      {"OMNIGATE_LISTEN_PORT", "80"},
      {"HEALTH_URL", "http://127.0.0.1/api/health"},
      {"DSHANCONTROL_NATIVE_PLATFORM_SOCKET",
       "/var/run/dshancontrol/platform.sock"},
      {"DSHANCONTROL_PLATFORM_SOCKET", "/var/run/dshancontrol/platform.sock"},
      {"DSHANCONTROL_UPDATER_EXECUTABLE", "/usr/bin/dshancontrol-runtime"},
      {"DSHANCONTROL_UPDATER_CONFIG", "/etc/dshancontrol/updater.json"},
      {"DSHANCONTROL_HMI_HEARTBEAT", pidRoot + "/hmi/heartbeat.json"},
      {"QT_QPA_PLATFORM", "linuxfb:fb=/dev/fb0"},
      {"QT_QPA_PLATFORM_PLUGIN_PATH", "/usr/lib/qt/plugins"},
      {"QT_QPA_FONTDIR", "/usr/share/fonts/wqy-zenhei"},
      {"QT_QPA_FB_NO_LIBINPUT", "1"}};
  for (auto it = fixed.cbegin(); it != fixed.cend(); ++it)
    result.insert(it.key(), it.value());
  // Only system-owned display setup values cross this boundary. No LD_*,
  // credential, proxy, lease or general caller environment is inherited.
  for (const auto &name :
       {QString("QT_QPA_PLATFORM"), QString("QT_QPA_PLATFORM_PLUGIN_PATH"),
        QString("QT_QPA_FONTDIR"), QString("QT_QPA_FB_NO_LIBINPUT"),
        QString("QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS")}) {
    const auto value = qEnvironmentVariable(qPrintable(name));
    if (!value.isEmpty())
      result.insert(name, value);
  }
  return result;
}
void prepare(Role &role, const QProcessEnvironment &values, const QString &cwd,
             bool development) {
  launchRequire(!role.arguments.isEmpty() && role.arguments.size() <= 32,
                "Invalid role arguments", "launch-executable");
  role.executable = QFile::encodeName(role.arguments.first());
  role.workingDirectory = QFile::encodeName(cwd);
  struct stat file;
  launchRequire(
      ::stat(role.executable.constData(), &file) == 0 &&
          S_ISREG(file.st_mode) &&
          (file.st_uid == ::geteuid() || file.st_uid == 0) &&
          !(file.st_mode & 0022) && (file.st_mode & 0005) == 0005,
      "Role executable is missing, writable or inaccessible to its user",
      "launch-executable");
  QFile executable(role.arguments.first());
  launchRequire(executable.open(QIODevice::ReadOnly),
                "Cannot inspect role executable");
  const auto header = executable.read(52);
  launchRequire(header.size() >= 20 && header.left(4) == QByteArray("\x7f"
                                                                    "ELF",
                                                                    4),
                "Role executable must be native ELF", "launch-executable");
  if (!development)
    launchRequire(header[4] == 1 && header[5] == 1 &&
                      static_cast<unsigned char>(header[18]) == 40 &&
                      header[19] == 0,
                  "S4 role executable ABI mismatch", "launch-executable");
  if (!development) {
    launchRequire(header.size() >= 40, "Incomplete S4 executable header",
                  "launch-executable");
    const auto flags = quint32(static_cast<unsigned char>(header[36])) |
                       quint32(static_cast<unsigned char>(header[37])) << 8 |
                       quint32(static_cast<unsigned char>(header[38])) << 16 |
                       quint32(static_cast<unsigned char>(header[39])) << 24;
    launchRequire(!(flags & 0x400U), "Hard-float S4 role executable refused",
                  "launch-executable");
  }
  int total = 0;
  QJsonArray encoded;
  for (const auto &arg : role.arguments) {
    launchRequire(!arg.contains(QChar(0)), "NUL in role argument");
    role.argumentBytes.append(arg.toUtf8());
    total += role.argumentBytes.last().size();
    encoded.append(arg);
  }
  launchRequire(
      total <= 32768 &&
          QJsonDocument(encoded).toJson(QJsonDocument::Compact).size() <= 60000,
      "Role arguments exceed launch record limit", "launch-executable");
  for (auto &arg : role.argumentBytes)
    role.argv.push_back(arg.data());
  role.argv.push_back(nullptr);
  for (const auto &name : values.keys())
    role.environmentBytes.append((name + '=' + values.value(name)).toUtf8());
  for (auto &entry : role.environmentBytes)
    role.environment.push_back(entry.data());
  role.environment.push_back(nullptr);
}
QList<int> descriptors() {
  QList<int> values;
  for (const auto &name :
       QDir("/proc/self/fd")
           .entryList(QDir::AllEntries | QDir::NoDotAndDotDot)) {
    bool valid = false;
    const auto descriptor = name.toInt(&valid);
    if (valid && descriptor > 2)
      values << descriptor;
  }
  return values;
}
void launch(Role &role, const QString &pidRoot, const QString &commit) {
  Descriptor gateParent, gateChild, errorRead, errorWrite, input;
  int gate[2], errors[2];
  launchRequire(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, gate) == 0,
                "Cannot create launch gate", "launch-channel");
  gateParent.value = gate[0];
  gateChild.value = gate[1];
  launchRequire(::pipe2(errors, O_CLOEXEC) == 0,
                "Cannot create exec acknowledgment channel", "launch-channel");
  errorRead.value = errors[0];
  errorWrite.value = errors[1];
  input.value = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
  launchRequire(input.value >= 0, "Cannot prepare application input");
  const auto inherited = descriptors();
  const pid_t child = ::fork();
  launchRequire(child >= 0, "Cannot fork application role", "launch-fork");
  if (child == 0) {
    ::close(gateParent.value);
    ::close(errorRead.value);
    char go;
    const auto count = ::read(gateChild.value, &go, 1);
    ::close(gateChild.value);
    if (count != 1 || go != 'G')
      ::_exit(125);
    for (const auto fd : inherited)
      if (fd != gateParent.value && fd != gateChild.value &&
          fd != errorRead.value)
        if (::fcntl(fd, F_SETFD, FD_CLOEXEC) < 0 && errno != EBADF)
          childError(errorWrite.value, 1, errno);
    if (::setsid() < 0)
      childError(errorWrite.value, 2, errno);
    if (::dup2(input.value, 0) < 0 || ::dup2(role.log.value, 1) < 0 ||
        ::dup2(role.log.value, 2) < 0)
      childError(errorWrite.value, 3, errno);
    if (::chdir(role.workingDirectory.constData()) < 0)
      childError(errorWrite.value, 4, errno);
    const auto reduced = applyApplicationCredentials(role.credentials);
    if (reduced.stage != CredentialStage::None)
      childError(errorWrite.value, 100 + int(reduced.stage), reduced.error);
    ::execve(role.executable.constData(), role.argv.data(),
             role.environment.data());
    childError(errorWrite.value, 5, errno);
  }
  gateChild.close();
  errorWrite.close();
  // Until this gate is released, a failed parent closes the channel and the
  // child exits without executing application code.
  const auto record = plannedApplicationRecord(child, role.name, role.arguments,
                                               role.credentials, commit);
  validateApplicationRecord(record, role.name, "start");
  publishPrivateProcessFile(
      pidRoot, role.name + ".launch.json",
      QJsonDocument(record).toJson(QJsonDocument::Compact));
  publishPrivateProcessFile(pidRoot, role.name + ".pid",
                            QByteArray::number(child) + '\n');
  const char go = 'G';
  launchRequire(::send(gateParent.value, &go, 1, MSG_NOSIGNAL) == 1,
                "Recorded child exited before launch gate", "launch-channel");
  gateParent.close();
  struct pollfd event {
    errorRead.value, POLLIN | POLLHUP, 0
  };
  const auto ready = ::poll(&event, 1, 10000);
  launchRequire(
      ready > 0,
      "Application exec acknowledgment timed out; launch record retained",
      "launch-timeout");
  ChildError failure{};
  const auto count = ::read(errorRead.value, &failure, sizeof(failure));
  if (count > 0) {
    launchRequire(count == sizeof(failure),
                  "Incomplete application child error acknowledgment",
                  "launch-channel");
    int status;
    ::waitpid(child, &status, WNOHANG);
    launchRequire(false,
                  "Application child failed at step " +
                      QString::number(failure.step) +
                      "; errno=" + QString::number(failure.number),
                  "launch-child-failed");
  }
  launchRequire(
      count == 0,
      "Application exec acknowledgment failed; launch record retained",
      "launch-channel");
  QElapsedTimer elapsed;
  elapsed.start();
  for (;;) {
    try {
      verifyApplicationRecord(record, child, role.name, "start", commit);
      break;
    } catch (const Failure &) {
      int status;
      const auto ended = ::waitpid(child, &status, WNOHANG);
      launchRequire(
          ended == 0 && elapsed.elapsed() < 5000,
          "Application exited or differs from launch record; state retained",
          "launch-identity");
      QThread::msleep(10);
    }
  }
}
} // namespace
QJsonObject startApplication(const QJsonObject &config) {
  ApplicationProcesses processes(config, "start");
  const auto commands = processes.launchCommands();
  const bool dev = config["development_mode"].toBool(false);
  const auto policy = config["application_health"].toObject();
  launchRequire(dev || !policy.contains("fixture_credentials"),
                "Fixture credentials require development mode",
                "launch-profile");
  const auto root = config["application_root"].toString(),
             pidRoot = processes.pidDirectory();
  if (!dev) {
    const auto contract = readJson(config["system_contract"].toString());
    verifyPersistentStorage(root, contract);
    verifyPersistentStorage(contract["data_root"].toString(), contract);
  }
  const auto updateDescriptor =
      inheritedApplicationLease("DSHANCONTROL_UPDATE_LEASE_FD");
  const auto controlDescriptor =
      inheritedApplicationLease("DSHANCONTROL_CONTROL_LEASE_FD");
  launchRequire((updateDescriptor < 0) == (controlDescriptor < 0),
                "Incomplete application lease handoff", "lease-denied");
  FileLock update(root + "/.update.lock", true, updateDescriptor);
  FileLock control(config["control_lock"].toString(root + "/.control.lock"),
                   true, controlDescriptor);
  const auto identity = applicationIdentity(config);
  const auto commit = identity["commit"].toString();
  const auto current = QFileInfo(root + "/current").canonicalFilePath();
  if (::geteuid() == 0) {
    QMap<QByteArray, quint64> capabilities;
    for (const auto &line : readBytes("/proc/self/status", 65536).split('\n')) {
      const auto split = line.indexOf(':');
      if (split < 0)
        continue;
      const auto name = line.left(split);
      if (name != "CapEff" && name != "CapPrm" && name != "CapBnd")
        continue;
      bool valid = false;
      const auto value = line.mid(split + 1).trimmed().toULongLong(&valid, 16);
      launchRequire(valid, "Cannot inspect launcher authority", "launch-owner");
      capabilities[name] = value;
    }
    const quint64 required =
        (quint64(1) << CAP_SETUID) | (quint64(1) << CAP_SETGID) |
        (quint64(1) << CAP_SETPCAP) | (quint64(1) << CAP_SYS_PTRACE);
    launchRequire((capabilities.value("CapEff") & required) == required,
                  "Launcher cannot establish or verify application identities",
                  "launch-owner");
  }
  bool valid = false;
  const auto last =
      QString::fromUtf8(readBytes("/proc/sys/kernel/cap_last_cap", 128))
          .trimmed()
          .toInt(&valid);
  launchRequire(valid && last >= 10 && last <= 63,
                "Kernel capability limit unavailable", "launch-profile");
  const auto values = environment(config, commit, pidRoot);
  std::vector<std::unique_ptr<Role>> roles;
  for (const auto &name :
       {QString("web"), QString("hmi"), QString("supervisor")})
    if (commands.contains(name)) {
      std::unique_ptr<Role> role(new Role);
      role->name = name;
      role->arguments = commands[name];
      role->credentials = credentials(name, policy, dev, last);
      prepare(*role, values, current, dev);
      roles.push_back(std::move(role));
    }
  trustedDirectory(pidRoot);
  FileLock lifecycle(pidRoot + "/.lifecycle.lock");
  for (const auto &role : roles) {
    for (const auto &suffix : {QString(".pid"), QString(".launch.json")}) {
      const auto path = pidRoot + '/' + role->name + suffix;
      launchRequire(
          !QFileInfo(path).exists() && !QFileInfo(path).isSymLink(),
          "Existing application launch state retained; stop/recover explicitly",
          "launch-state-exists");
    }
  }
  // Complete every preflight before any application is started.
  for (const auto &role : roles) {
    role->log.value = ::open(
        QFile::encodeName(pidRoot + '/' + role->name + ".log").constData(),
        O_WRONLY | O_CREAT | O_APPEND | O_NOFOLLOW | O_CLOEXEC, 0600);
    struct stat info;
    launchRequire(role->log.value >= 0 &&
                      ::fstat(role->log.value, &info) == 0 &&
                      S_ISREG(info.st_mode) && info.st_uid == ::geteuid() &&
                      info.st_nlink == 1 && !(info.st_mode & 0077),
                  "Untrusted application log", "launch-log");
  }
  QJsonArray launched;
  for (const auto &role : roles) {
    launch(*role, pidRoot, commit);
    launched.append(role->name);
  }
  return {{"ok", true}, {"launched", launched}, {"application_commit", commit}};
}
