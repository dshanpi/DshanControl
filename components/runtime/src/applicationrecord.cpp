#include "applicationrecord.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QUuid>
#include <errno.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace {
void check(bool ok, const QString &phase, const QString &message,
           const QString &code = "process-record") {
  require(ok, message, phase, phase + '-' + code);
}
QString proc(pid_t pid, const QString &name) {
  return "/proc/" + QString::number(pid) + '/' + name;
}
QString boot() {
  return QString::fromUtf8(readBytes("/proc/sys/kernel/random/boot_id", 128))
      .trimmed();
}
QMap<QByteArray, QByteArray> status(pid_t pid) {
  QMap<QByteArray, QByteArray> fields;
  for (const auto &line : readBytes(proc(pid, "status"), 65536).split('\n')) {
    auto split = line.indexOf(':');
    if (split >= 0)
      fields[line.left(split)] = line.mid(split + 1).simplified();
  }
  return fields;
}
bool identityFields(const QByteArray &bytes, quint64 expected) {
  const auto fields = bytes.split(' ');
  if (fields.size() != 4)
    return false;
  for (const auto &field : fields) {
    bool valid = false;
    auto value = field.toULongLong(&valid);
    if (!valid || value != expected)
      return false;
  }
  return true;
}
quint64 integerString(const QJsonValue &field, const QString &phase,
                      int base = 10) {
  bool valid = false;
  auto value = field.toString().toULongLong(&valid, base);
  check(field.isString() && valid, phase, "Invalid numeric process identity");
  return value;
}
} // namespace
void publishPrivateProcessFile(const QString &directory, const QString &name,
                               const QByteArray &bytes) {
  require(!name.isEmpty() && !name.contains('/') && !name.contains("..") &&
              !name.contains(QChar(0)) && !bytes.isEmpty() &&
              bytes.size() <= 65536,
          "Invalid private process publication", "start");
  const auto parent = ::open(QFile::encodeName(directory).constData(),
                             O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
  require(parent >= 0, "Cannot open process publication directory", "start");
  int file = -1;
  try {
    struct stat owner;
    require(::fstat(parent, &owner) == 0 && owner.st_uid == ::geteuid() &&
                !(owner.st_mode & 0022),
            "Untrusted process publication directory", "start");
    const auto temporary =
        ".new-process-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    file = ::openat(parent, QFile::encodeName(temporary).constData(),
                    O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    require(file >= 0, "Cannot stage private process record", "start");
    int done = 0;
    while (done < bytes.size()) {
      auto count = ::write(file, bytes.constData() + done, bytes.size() - done);
      require(count > 0, "Cannot write private process record", "start");
      done += int(count);
    }
    require(::fsync(file) == 0, "Cannot sync private process record", "start");
    ::close(file);
    file = -1;
#if defined(SYS_renameat2)
    constexpr long renameSystemCall = SYS_renameat2;
#elif defined(__arm__)
    // Selected Linux5.4 arch/arm/tools/syscall.tbl: renameat2 entry382.
    constexpr long renameSystemCall = 382;
#else
#error "Audited renameat2 syscall mapping required"
#endif
    const auto renamed = ::syscall(
        renameSystemCall, parent, QFile::encodeName(temporary).constData(),
        parent, QFile::encodeName(name).constData(), 1U /* RENAME_NOREPLACE */);
    const auto publicationError = errno;
    require(renamed == 0,
            "Private process record already exists or publication failed; "
            "existing state retained",
            "start",
            publicationError == EEXIST ? "process-record-exists"
                                       : "process-publication-failed");
    require(::fsync(parent) == 0, "Cannot sync process record directory",
            "start");
    ::close(parent);
  } catch (...) {
    if (file >= 0)
      ::close(file);
    ::close(parent);
    throw;
  }
}
PrivateProcessFile::PrivateProcessFile(const QString &directory,
                                       const QString &name,
                                       const QString &stage, bool optional,
                                       bool privateMode)
    : leaf(name), phase(stage) {
  try {
    check(QDir::isAbsolutePath(directory) && directory != "/" &&
              QDir::cleanPath(directory) == directory && !name.isEmpty() &&
              !name.contains('/') && !name.contains("..") &&
              !name.contains(QChar(0)),
          phase, "Invalid process record path");
    parent = ::open(QFile::encodeName(directory).constData(),
                    O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    struct stat owner;
    check(parent >= 0 && ::fstat(parent, &owner) == 0 &&
              owner.st_uid == ::geteuid() && !(owner.st_mode & 0022),
          phase, "Untrusted process record directory");
    file = ::openat(parent, QFile::encodeName(name).constData(),
                    O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (file < 0 && errno == ENOENT && optional)
      return;
    check(file >= 0 && ::fstat(file, &identity) == 0 &&
              S_ISREG(identity.st_mode) && identity.st_nlink == 1 &&
              identity.st_uid == ::geteuid() &&
              !(identity.st_mode & (privateMode ? 0077 : 0022)) &&
              identity.st_size > 0 && identity.st_size <= 65536,
          phase, "Missing or unsafe private process record");
    bytes.resize(int(identity.st_size));
    auto count = ::pread(file, bytes.data(), bytes.size(), 0);
    check(count == bytes.size(), phase, "Incomplete private process record");
  } catch (...) {
    if (file >= 0)
      ::close(file);
    if (parent >= 0)
      ::close(parent);
    file = parent = -1;
    throw;
  }
}
PrivateProcessFile::~PrivateProcessFile() {
  if (file >= 0)
    ::close(file);
  if (parent >= 0)
    ::close(parent);
}
QJsonObject PrivateProcessFile::json() const {
  check(exists(), phase, "Process record unavailable");
  const auto value = strictJson(bytes);
  check(value.isObject(), phase, "Process record must be an object");
  return value.toObject();
}
void PrivateProcessFile::removeUnchanged() {
  if (!exists())
    return;
  struct stat current;
  QByteArray now(bytes.size() + 1, 0);
  const auto count = ::pread(file, now.data(), now.size(), 0);
  check(::fstatat(parent, QFile::encodeName(leaf).constData(), &current,
                  AT_SYMLINK_NOFOLLOW) == 0 &&
            current.st_dev == identity.st_dev &&
            current.st_ino == identity.st_ino && S_ISREG(current.st_mode) &&
            current.st_nlink == 1 && current.st_uid == identity.st_uid &&
            current.st_mode == identity.st_mode && count == bytes.size() &&
            now.left(int(qMax<ssize_t>(0, count))) == bytes,
        phase, "Process record changed; replacement preserved",
        "record-changed");
  check(::unlinkat(parent, QFile::encodeName(leaf).constData(), 0) == 0 &&
            ::fsync(parent) == 0,
        phase, "Cannot clear stopped process record");
}
QString applicationStartTicks(pid_t pid) {
  const auto raw = readBytes(proc(pid, "stat"), 4096);
  auto split = raw.lastIndexOf(") ");
  require(split >= 0, "Invalid process birth identity", "identity");
  const auto fields = raw.mid(split + 2).simplified().split(' ');
  require(fields.size() > 19 && QRegularExpression("^[0-9]+$")
                                    .match(QString::fromLatin1(fields[19]))
                                    .hasMatch(),
          "Process birth identity unavailable", "identity");
  return QString::fromLatin1(fields[19]);
}
QJsonObject plannedApplicationRecord(pid_t pid, const QString &role,
                                     const QStringList &argv,
                                     const ApplicationCredentials &credentials,
                                     const QString &commit) {
  require(pid > 1 && !argv.isEmpty() &&
              QRegularExpression("^[a-f0-9]{40}$").match(commit).hasMatch(),
          "Invalid launch identity", "start");
  struct stat executable;
  require(::stat(QFile::encodeName(argv.first()).constData(), &executable) ==
                  0 &&
              S_ISREG(executable.st_mode),
          "Launch executable unavailable", "start");
  QJsonArray args, groups;
  for (const auto &arg : argv)
    args.append(arg);
  for (int i = 0; i < credentials.groupCount; ++i)
    groups.append(double(credentials.groups[i]));
  return {{"schema_version", 1},
          {"role", role},
          {"pid", double(pid)},
          {"boot_id", boot()},
          {"start_ticks", applicationStartTicks(pid)},
          {"uid", double(credentials.uid)},
          {"gid", double(credentials.gid)},
          {"groups", groups},
          {"bind_privileged_port", credentials.bindPrivilegedPort},
          {"bounding_enforced", ::geteuid() == 0},
          {"argv", args},
          {"exe_device", QString::number(qulonglong(executable.st_dev))},
          {"exe_inode", QString::number(qulonglong(executable.st_ino))},
          {"application_commit", commit}};
}
void validateApplicationRecord(const QJsonObject &record, const QString &role,
                               const QString &phase) {
  check(
      record["schema_version"] == 1 && record["role"] == role &&
          positiveInteger(record["pid"]) && record["pid"].toDouble() > 1 &&
          record["pid"].toDouble() <= 2147483647 &&
          record["boot_id"].isString() &&
          !record["boot_id"].toString().isEmpty() &&
          record["start_ticks"].isString() && positiveInteger(record["uid"]) &&
          record["uid"].toDouble() <= 2147483647 &&
          positiveInteger(record["gid"]) &&
          record["gid"].toDouble() <= 2147483647 &&
          record["bind_privileged_port"].isBool() &&
          record["bounding_enforced"].isBool() && record["groups"].isArray() &&
          record["groups"].toArray().size() <= 64 &&
          QRegularExpression("^[a-f0-9]{40}$")
              .match(record["application_commit"].toString())
              .hasMatch(),
      phase, "Invalid private process launch record");
  integerString(record["start_ticks"], phase);
  integerString(record["exe_device"], phase);
  integerString(record["exe_inode"], phase);
  check(record["argv"].isArray() && !record["argv"].toArray().isEmpty(), phase,
        "Missing recorded argv");
  for (const auto &arg : record["argv"].toArray())
    check(arg.isString() && !arg.toString().contains(QChar(0)), phase,
          "Invalid recorded argv");
  QSet<quint64> groups;
  for (const auto &group : record["groups"].toArray()) {
    check(positiveInteger(group) && group.toDouble() <= 2147483647 &&
              !groups.contains(quint64(group.toDouble())),
          phase, "Invalid recorded groups");
    groups.insert(quint64(group.toDouble()));
  }
}
void verifyApplicationRecord(const QJsonObject &record, pid_t pid,
                             const QString &role, const QString &phase,
                             const QString &commit) {
  validateApplicationRecord(record, role, phase);
  check(record["pid"].toDouble() == pid && record["boot_id"] == boot() &&
            record["start_ticks"] == applicationStartTicks(pid) &&
            (commit.isEmpty() || record["application_commit"] == commit),
        phase, "Process launch identity mismatch");
  const bool bind = record["bind_privileged_port"].toBool();
  check(!bind || role == "web", phase, "Role capability policy mismatch");
  struct stat executable;
  check(::stat(QFile::encodeName(proc(pid, "exe")).constData(), &executable) ==
                0 &&
            S_ISREG(executable.st_mode) &&
            qulonglong(executable.st_dev) ==
                integerString(record["exe_device"], phase) &&
            qulonglong(executable.st_ino) ==
                integerString(record["exe_inode"], phase),
        phase, "Process executable differs from launch record");
  auto actual = readBytes(proc(pid, "cmdline"), 65536).split('\0');
  if (!actual.isEmpty() && actual.last().isEmpty())
    actual.removeLast();
  QList<QByteArray> expected;
  for (const auto &arg : record["argv"].toArray()) {
    check(arg.isString() && !arg.toString().contains(QChar(0)), phase,
          "Invalid recorded argv");
    expected << arg.toString().toUtf8();
  }
  check(!expected.isEmpty() && actual == expected, phase,
        "Process arguments differ from launch record");
  const auto fields = status(pid);
  check(
      identityFields(fields.value("Uid"), quint64(record["uid"].toDouble())) &&
          identityFields(fields.value("Gid"),
                         quint64(record["gid"].toDouble())) &&
          fields.value("NoNewPrivs") == "1",
      phase, "Process credentials differ from launch record");
  QSet<quint64> expectedGroups, actualGroups;
  for (const auto &group : record["groups"].toArray()) {
    check(positiveInteger(group) && group.toDouble() <= 2147483647 &&
              !expectedGroups.contains(quint64(group.toDouble())),
          phase, "Invalid recorded supplemental group");
    expectedGroups.insert(quint64(group.toDouble()));
  }
  for (const auto &group : fields.value("Groups").split(' '))
    if (!group.isEmpty()) {
      bool valid = false;
      auto value = group.toULongLong(&valid);
      check(valid, phase, "Invalid process group identity");
      actualGroups.insert(value);
    }
  check(actualGroups == expectedGroups, phase,
        "Process supplemental groups changed");
  const quint64 allowed = bind ? quint64(1) << 10 : 0;
  for (const auto &name :
       {QByteArray("CapInh"), QByteArray("CapPrm"), QByteArray("CapEff"),
        QByteArray("CapAmb"), QByteArray("CapBnd")}) {
    if (name == "CapBnd" && !record["bounding_enforced"].toBool())
      continue;
    bool valid = false;
    const auto value = fields.value(name).toULongLong(&valid, 16);
    check(valid && !(value & ~allowed), phase,
          "Process capability exceeds role policy");
  }
}
