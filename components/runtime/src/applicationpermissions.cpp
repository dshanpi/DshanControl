#include "applicationpermissions.h"
#include "applicationhealth.h"
#include "systemstorage.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>
#include <QUuid>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <memory>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#include <vector>

namespace {
void check(bool value, const QString &message,
           const QString &code = "permissions-refused") {
  require(value, message, "permissions", code);
}
void systemCall(int result, const QString &step) {
  if (result == 0)
    return;
  const auto number = errno;
  throw Failure(QString::number(number), "permissions-" + step,
                "Permission metadata operation failed; state retained");
}
void plain(const QString &path) {
  check(QDir::isAbsolutePath(path) && path != "/" &&
            QDir::cleanPath(path) == path && !path.contains(QChar(0)),
        "Invalid permission path");
  QString parent;
  for (const auto &part : path.mid(1).split('/')) {
    parent += '/' + part;
    check(!QFileInfo(parent).isSymLink(), "Linked permission path refused");
  }
}
quint32 number(const QJsonValue &value) {
  check(positiveInteger(value) && value.toDouble() <= 2147483647,
        "Invalid nonroot fixture identity");
  return quint32(value.toDouble());
}
struct Profile {
  uid_t web = 400, hmi = 401;
  gid_t group = 400;
  QString base, apps, data, run, records, heartbeat, framebuffer, touch;
  bool fixture = false;
  explicit Profile(const QJsonObject &config) {
    check(!config.contains("development_mode") ||
              config["development_mode"].isBool(),
          "Invalid development mode");
    const bool dev = config["development_mode"].toBool(false);
    fixture = config.contains("permission_fixtures");
    check(fixture == dev, "Development permission fixtures must be explicit");
    const auto contract = readJson(config["system_contract"].toString());
    apps = config["application_root"].toString();
    data = contract["data_root"].toString();
    base = QFileInfo(apps).absolutePath();
    plain(apps);
    plain(data);
    plain(base);
    check(QFileInfo(data).absolutePath() == base && apps != data,
          "Permission data roots disagree");
    const auto policy = config["application_health"].toObject();
    check(policy["launch_receipts_required"] == true,
          "Managed launch records required");
    if (fixture) {
      check(config["permission_fixtures"].isObject(),
            "Invalid permission fixture policy");
      const auto values = config["permission_fixtures"].toObject();
      const QSet<QString> allowed{"web_uid",  "hmi_uid",     "gid",
                                  "run_root", "framebuffer", "touchscreen"};
      for (const auto &key : values.keys())
        check(allowed.contains(key), "Unknown permission fixture field");
      web = number(values["web_uid"]);
      hmi = number(values["hmi_uid"]);
      group = number(values["gid"]);
      check(web != hmi, "Distinct application identities required");
      run = values["run_root"].toString();
      framebuffer = values["framebuffer"].toString();
      touch = values["touchscreen"].toString();
      plain(run);
      plain(framebuffer);
      plain(touch);
      check(run.startsWith(base + '/') && framebuffer.startsWith(base + '/') &&
                touch.startsWith(base + '/'),
            "Permission fixtures must stay under the fixture root");
    } else {
      check(apps == "/mnt/UDISK/dshancontrol/apps" &&
                data == "/mnt/UDISK/dshancontrol/data" &&
                policy["native_supervisor"] == true,
            "Fixed production permission roots required");
      ensureSystemStorage(config, false);
      verifyPersistentStorage(apps, contract);
      verifyPersistentStorage(data, contract);
      const auto first = ::getpwnam("dshan-web");
      check(first && first->pw_uid == web && first->pw_gid == group,
            "Dedicated web account missing or mismatched");
      const auto second = ::getpwnam("dshan-hmi");
      check(second && second->pw_uid == hmi && second->pw_gid == group,
            "Dedicated HMI account missing or mismatched");
      const auto shared = ::getgrnam("dshan-app");
      check(shared && shared->gr_gid == group,
            "Dedicated application group missing or mismatched");
      run = "/var/run/dshancontrol";
      framebuffer = "/dev/fb0";
      for (const auto &entry :
           QDir("/sys/class/input").entryList({"event*"}, QDir::Dirs)) {
        const auto sys = "/sys/class/input/" + entry;
        if (QString::fromUtf8(readBytes(sys + "/device/name", 256)).trimmed() !=
            "gt9xxnew_ts")
          continue;
        check(touch.isEmpty(), "Ambiguous named touchscreen",
              "permissions-device");
        touch = "/dev/input/" + entry;
      }
      check(!touch.isEmpty(), "Named touchscreen unavailable",
            "permissions-device");
      for (const auto &pair :
           {qMakePair(framebuffer, QString("/sys/class/graphics/fb0/dev")),
            qMakePair(touch, "/sys/class/input/" + QFileInfo(touch).fileName() +
                                 "/dev")}) {
        struct stat info;
        check(::lstat(QFile::encodeName(pair.first).constData(), &info) == 0 &&
                  S_ISCHR(info.st_mode),
              "Expected character device missing", "permissions-device");
        const auto actual = QString::number(major(info.st_rdev)) + ':' +
                            QString::number(minor(info.st_rdev));
        check(QString::fromUtf8(readBytes(pair.second, 128)).trimmed() ==
                  actual,
              "Device and sysfs identity disagree", "permissions-device");
      }
    }
    records = run + "/processes";
    heartbeat = run + "/hmi/heartbeat.json";
    check(policy["pid_root"] == records && policy["hmi_heartbeat"] == heartbeat,
          "Private records/heartbeat layout required");
    plain(run);
    plain(records);
    plain(QFileInfo(heartbeat).absolutePath());
    applicationIdentity(config);
  }
};
struct Entry {
  QString path;
  uid_t uid;
  gid_t gid;
  mode_t mode;
  int type, fd = -1;
  bool missing = false;
  struct stat original {};
  Entry(const QString &p, uid_t u, gid_t g, mode_t m, int t)
      : path(p), uid(u), gid(g), mode(m), type(t) {
    try {
      plain(path);
      fd = ::open(QFile::encodeName(path).constData(),
                  O_PATH | O_NOFOLLOW | O_CLOEXEC);
      if (fd < 0 && errno == ENOENT && type == S_IFDIR) {
        missing = true;
        return;
      }
      check(fd >= 0 && ::fstat(fd, &original) == 0,
            "Cannot inspect permission target");
      check((original.st_mode & S_IFMT) == mode_t(type) &&
                (type == S_IFDIR || original.st_nlink == 1) &&
                !(original.st_mode & 07000),
            "Special, linked or multiply linked permission target");
      check(original.st_uid == ::geteuid() || original.st_uid == uid,
            "Unexpected permission target owner");
    } catch (...) {
      if (fd >= 0)
        ::close(fd);
      fd = -1;
      throw;
    }
  }
  ~Entry() {
    if (fd >= 0)
      ::close(fd);
  }
  QJsonObject json() const {
    return {{"path", path},
            {"uid", double(uid)},
            {"gid", double(gid)},
            {"mode", QString::number(mode, 8)},
            {"create", missing},
            {"before_uid", double(original.st_uid)},
            {"before_gid", double(original.st_gid)},
            {"before_mode", QString::number(original.st_mode & 07777, 8)}};
  }
  void apply() {
    if (missing) {
      systemCall(::mkdir(QFile::encodeName(path).constData(), 0700), "mkdir");
      fd = ::open(QFile::encodeName(path).constData(),
                  O_PATH | O_NOFOLLOW | O_CLOEXEC);
      check(fd >= 0 && ::fstat(fd, &original) == 0 && S_ISDIR(original.st_mode),
            "Cannot bind created permission directory");
      missing = false;
    }
    struct stat current;
    check(::lstat(QFile::encodeName(path).constData(), &current) == 0 &&
              current.st_dev == original.st_dev &&
              current.st_ino == original.st_ino &&
              current.st_mode == original.st_mode &&
              current.st_uid == original.st_uid &&
              current.st_gid == original.st_gid,
          "Permission target changed; state retained");
    // O_PATH does not open the framebuffer/input driver. chmod through its
    // pinned proc descriptor follows the same inode without driver I/O.
    const auto pinned = QByteArray("/proc/self/fd/") + QByteArray::number(fd);
    systemCall(::fchownat(fd, "", uid, gid, AT_EMPTY_PATH), "chown");
    systemCall(::chmod(pinned.constData(), mode), "chmod");
    check(::fstat(fd, &current) == 0 && current.st_uid == uid &&
              current.st_gid == gid && (current.st_mode & 07777) == mode,
          "Permission metadata verification failed");
    struct stat published;
    check(::lstat(QFile::encodeName(path).constData(), &published) == 0 &&
              published.st_dev == current.st_dev &&
              published.st_ino == current.st_ino,
          "Permission target replaced during application");
    if (type != S_IFCHR) {
      const auto sync =
          ::open(pinned.constData(),
                 O_RDONLY | O_CLOEXEC | (type == S_IFDIR ? O_DIRECTORY : 0));
      if (sync < 0)
        systemCall(-1, "open-sync");
      const auto result = ::fsync(sync);
      const auto syncError = errno;
      if (sync >= 0)
        ::close(sync);
      if (result != 0) {
        errno = syncError;
        systemCall(result, "sync");
      }
    }
  }
};
void auditVersionTree(const QString &path) {
  QStringList dirs{path};
  for (int i = 0; i < dirs.size(); ++i) {
    struct stat parent;
    check(::lstat(QFile::encodeName(dirs[i]).constData(), &parent) == 0 &&
              S_ISDIR(parent.st_mode) && parent.st_uid == ::geteuid() &&
              !(parent.st_mode & 0022),
          "Mutable application version directory refused");
    for (const auto &entry :
         QDir(dirs[i]).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot |
                                     QDir::Hidden | QDir::System)) {
      check(!entry.isSymLink(), "Linked version entry refused");
      if (entry.isDir()) {
        dirs << entry.absoluteFilePath();
        continue;
      }
      struct stat info;
      check(::lstat(QFile::encodeName(entry.absoluteFilePath()).constData(),
                    &info) == 0 &&
                S_ISREG(info.st_mode) && info.st_nlink == 1 &&
                info.st_uid == ::geteuid() && !(info.st_mode & 07022),
            "Mutable or special application payload refused");
    }
  }
}
void requireNoRoleProcesses(uid_t web, uid_t hmi) {
  for (const auto &name :
       QDir("/proc").entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
    if (!QRegularExpression("^[0-9]+$").match(name).hasMatch())
      continue;
    const auto base = "/proc/" + name;
    QFile file(base + "/status");
    if (!file.open(QIODevice::ReadOnly)) {
      if (!QFileInfo(base).exists())
        continue;
      check(false, "Cannot inspect application role identities");
    }
    const auto bytes = file.read(65537);
    check(bytes.size() <= 65536, "Unexpected process status size");
    bool found = false;
    for (const auto &line : bytes.split('\n')) {
      if (!line.startsWith("Uid:"))
        continue;
      const auto ids = line.mid(4).simplified().split(' ');
      check(ids.size() == 4, "Invalid process role identity");
      for (const auto &id : ids) {
        bool valid = false;
        const auto value = id.toUInt(&valid);
        check(valid, "Invalid process UID");
        check(value != web && value != hmi,
              "Application role process remains active; no permission change",
              "permissions-active");
      }
      found = true;
    }
    check(found || !QFileInfo(base).exists(),
          "Process role identity unavailable");
  }
}
} // namespace
QJsonObject applicationPermissions(const QJsonObject &config, bool apply) {
  Profile profile(config);
  std::unique_ptr<FileLock> update, control;
  if (apply) {
    check(::geteuid() == 0, "Permission application requires system owner");
    update.reset(new FileLock(profile.apps + "/.update.lock"));
    control.reset(new FileLock(
        config["control_lock"].toString(profile.apps + "/.control.lock")));
    for (const auto &root : {profile.records, profile.run})
      for (const auto &role :
           {QString("web"), QString("hmi"), QString("supervisor")})
        for (const auto &suffix : {QString(".pid"), QString(".launch.json")}) {
          const auto file = root + '/' + role + suffix;
          check(!QFileInfo(file).exists() && !QFileInfo(file).isSymLink(),
                "Stop application roles before changing permissions",
                "permissions-active");
        }
    requireNoRoleProcesses(profile.web, profile.hmi);
  }
  auditVersionTree(profile.apps + "/releases");
  std::vector<std::unique_ptr<Entry>> entries;
  auto add = [&](const QString &path, uid_t uid, gid_t gid, mode_t mode,
                 int type = S_IFDIR) {
    entries.emplace_back(new Entry(path, uid, gid, mode, type));
  };
  add(profile.base, ::geteuid(), profile.group, 0750);
  add(profile.apps, ::geteuid(), profile.group, 0750);
  add(profile.run, ::geteuid(), profile.group, 0750);
  add(profile.records, ::geteuid(), ::getegid(), 0700);
  add(QFileInfo(profile.heartbeat).absolutePath(), profile.hmi, profile.group,
      0700);
  add(profile.data, profile.web, profile.group, 0700);
  const QSet<QString> known{
      "web.json",    "hmi.json",    "session.key", "edge.db",    "hmi.db",
      "edge.db-wal", "edge.db-shm", "hmi.db-wal",  "hmi.db-shm", "eds"};
  for (const auto &entry :
       QDir(profile.data)
           .entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot |
                          QDir::Hidden | QDir::System)) {
    check(known.contains(entry.fileName()),
          "Unknown application data entry retained",
          "permissions-unknown-data");
    if (entry.fileName().endsWith("-wal") ||
        entry.fileName().endsWith("-shm")) {
      const auto database = profile.data + '/' +
                            entry.fileName().left(entry.fileName().size() - 4);
      check(QFileInfo(database).isFile() && !QFileInfo(database).isSymLink(),
            "SQLite auxiliary entry has no regular main database");
      // After a power interruption these may be necessary for SQLite recovery.
      // Roles are quiescent; preserve every byte and only migrate metadata.
    }
    if (entry.fileName() == "eds") {
      add(entry.absoluteFilePath(), profile.web, profile.group, 0700);
      for (const auto &leaf :
           QDir(entry.absoluteFilePath())
               .entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot |
                              QDir::Hidden | QDir::System))
        add(leaf.absoluteFilePath(), profile.web, profile.group, 0600, S_IFREG);
    } else
      add(entry.absoluteFilePath(), profile.web, profile.group, 0600, S_IFREG);
  }
  if (!QFileInfo::exists(profile.data + "/eds"))
    add(profile.data + "/eds", profile.web, profile.group, 0700);
  add(profile.framebuffer, profile.hmi, profile.group, 0600,
      profile.fixture ? S_IFREG : S_IFCHR);
  add(profile.touch, profile.hmi, profile.group, 0600,
      profile.fixture ? S_IFREG : S_IFCHR);
  QJsonArray plan;
  for (const auto &entry : entries)
    plan.append(entry->json());
  if (!apply)
    return {{"applied", false}, {"entries", plan}};
  // The private journal precedes changes to existing application/data/device
  // metadata. Incomplete attempts are retained; no automatic rollback or retry.
  Entry journalRoot(profile.base + "/system", 0, 0, 0700, S_IFDIR);
  journalRoot.apply();
  const auto journal = profile.base + "/system/permissions-attempt." +
                       QUuid::createUuid().toString(QUuid::WithoutBraces) +
                       ".json";
  QJsonObject state{
      {"schema_version", 1}, {"status", "applying"}, {"entries", plan}};
  durableJson(journal, state);
  int completed = 0;
  try {
    for (const auto &entry : entries) {
      entry->apply();
      ++completed;
    }
    state["status"] = "completed";
    state["completed_entries"] = completed;
    durableJson(journal, state);
  } catch (const Failure &error) {
    state["status"] = "failed";
    state["completed_entries"] = completed;
    state["error"] = QJsonObject{{"stage", error.stage}, {"code", error.code}};
    try {
      durableJson(journal, state);
    } catch (...) {
    }
    throw;
  }
  return {{"applied", true},
          {"entries", plan},
          {"journal", journal},
          {"application_data_contents_written", false}};
}
