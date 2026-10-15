#include "systemstorage.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QMap>
#include <QProcess>
#include <QRegularExpression>
#include <QStorageInfo>
#include <QUuid>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/sysmacros.h>
#include <unistd.h>

namespace {
void storageRequire(bool ok, const QString &message,
                    const QString &code = "storage-mismatch") {
  require(ok, message, "storage", code);
}
void plainPath(const QString &path) {
  storageRequire(QDir::isAbsolutePath(path) && path != "/" &&
                     QDir::cleanPath(path) == path && !path.contains(QChar(0)),
                 "Invalid managed storage path");
  QString current;
  for (const auto &part : path.mid(1).split('/')) {
    current += '/' + part;
    storageRequire(!QFileInfo(current).isSymLink(), "Linked managed storage path refused");
  }
}
QString blockIdentity(const QString &path) {
  struct stat info;
  storageRequire(::stat(QFile::encodeName(path).constData(), &info) == 0 &&
                     S_ISBLK(info.st_mode), "Persistent device is unavailable or not a block device");
  return QString::number(major(info.st_rdev)) + ':' + QString::number(minor(info.st_rdev));
}
bool development(const QJsonObject &config) {
  storageRequire(!config.contains("development_mode") || config["development_mode"].isBool(),
                 "Invalid development mode", "storage-contract-missing");
  return config["development_mode"].toBool(false);
}
QByteArray fixedTool(const QString &program, const QStringList &arguments, int timeout) {
  QProcess process;
  process.setStandardErrorFile(QProcess::nullDevice());
  process.start(program, arguments);
  storageRequire(process.waitForStarted(3000), "Cannot start system storage tool", "storage-tool-start");
  if (process.state()!=QProcess::NotRunning && !process.waitForFinished(timeout)) {
    process.kill(); process.waitForFinished(3000);
    throw Failure("storage-tool-timeout", "storage", "System storage tool timed out; not relaunched");
  }
  storageRequire(process.exitStatus() == QProcess::NormalExit,
                 "System storage tool crashed", "storage-tool-crashed");
  storageRequire(process.exitCode() == 0, "System storage tool failed",
                 QString::number(process.exitCode()));
  auto output = process.readAllStandardOutput();
  storageRequire(output.size() <= 65536, "System storage tool output exceeds limit");
  return output;
}
void directory(const QString &path, QFile::Permissions mode) {
  plainPath(path);
  QFileInfo info(path);
  if (info.exists()) {
    storageRequire(info.isDir(), "Managed directory is not a directory");
    return;
  }
  const auto parent = info.absolutePath();
  storageRequire(QFileInfo(parent).isDir() && QDir().mkdir(path), "Cannot create managed directory");
  storageRequire(QFile::setPermissions(path, mode), "Cannot protect managed directory");
  syncDirectory(parent);
}
const QFile::Permissions PublicDirectory = QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
    QFile::ReadGroup | QFile::ExeGroup | QFile::ReadOther | QFile::ExeOther;
const QFile::Permissions PrivateDirectory = QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner;
QMap<QString, QString> tree(const QString &root) {
  plainPath(root);
  storageRequire(QFileInfo(root).isDir(), "Factory source directory missing");
  QMap<QString, QString> files;
  QStringList directories{root};
  for (int i = 0; i < directories.size(); ++i) {
    for (const auto &entry : QDir(directories[i]).entryInfoList(
             QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System)) {
      storageRequire(!entry.isSymLink(), "Linked factory entry refused");
      struct stat info;
      storageRequire(::lstat(QFile::encodeName(entry.absoluteFilePath()).constData(), &info) == 0,
                     "Cannot inspect factory entry");
      if (S_ISDIR(info.st_mode)) directories << entry.absoluteFilePath();
      else {
        storageRequire(S_ISREG(info.st_mode) && info.st_nlink == 1 && !(info.st_mode & 07000),
                       "Special or multiply linked factory entry refused");
        files[QDir(root).relativeFilePath(entry.absoluteFilePath())] = hashFile(entry.absoluteFilePath());
      }
    }
  }
  return files;
}
QJsonObject installed(const QString &path) {
  plainPath(path);
  auto value = readJson(path);
  storageRequire(QRegularExpression("^[a-f0-9]{40}$").match(value["commit"].toString()).hasMatch() &&
                     !value["version"].toString().isEmpty() && value["data_schema"] == 1,
                 "Invalid installed application identity", "initialize-identity");
  return value;
}
void copyFile(const QString &source, const QString &destination) {
  QFile input(source);
  storageRequire(input.open(QIODevice::ReadOnly), "Cannot read factory source");
  int fd = ::open(QFile::encodeName(destination).constData(),
                  O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
  storageRequire(fd >= 0, "Cannot create factory staging file");
  try {
    while (!input.atEnd()) {
      auto bytes = input.read(65536);
      storageRequire(input.error() == QFile::NoError, "Factory source read failed");
      qint64 done = 0;
      while (done < bytes.size()) {
        auto count = ::write(fd, bytes.constData() + done, bytes.size() - done);
        storageRequire(count > 0, "Factory copy failed");
        done += count;
      }
    }
    const auto mode = input.permissions().testFlag(QFile::ExeOwner) ? 0755 : 0644;
    storageRequire(::fchmod(fd, mode) == 0 && ::fsync(fd) == 0, "Cannot seal factory file");
    ::close(fd);
  } catch (...) { ::close(fd); throw; }
}
void syncTree(const QString &root) {
  QStringList directories{root};
  for (int i = 0; i < directories.size(); ++i)
    for (const auto &entry : QDir(directories[i]).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
      directories << entry.absoluteFilePath();
  for (int i = directories.size()-1; i >= 0; --i) syncDirectory(directories[i]);
}
} // namespace

QString bootPersistentDevice(const QByteArray &cmdline) {
  auto fields = QString::fromUtf8(cmdline).trimmed().split(QRegularExpression("\\s+"), QString::SkipEmptyParts);
  QStringList maps;
  for (const auto &field : fields) if (field.startsWith("partitions=")) maps << field.mid(11);
  storageRequire(maps.size() == 1, "Exactly one boot partition map required", "storage-boot-map");
  QStringList devices;
  for (const auto &part : maps.first().split(':')) if (part.startsWith("UDISK@")) devices << part.mid(6);
  storageRequire(devices.size() == 1 && QRegularExpression("^mmcblk[0-9]+p[1-9][0-9]*$")
                     .match(devices.value(0)).hasMatch(), "Missing or ambiguous SD NAND UDISK partition", "storage-boot-map");
  return "/dev/" + devices.first();
}
bool persistentMountPresent(const QByteArray &mountinfo, const QString &mount, const QString &identity) {
  int matches = 0;
  for (const auto &line : mountinfo.split('\n')) {
    auto fields = line.simplified().split(' ');
    if (fields.size() < 5 || fields[4] != mount.toUtf8()) continue;
    ++matches;
    auto split = fields.indexOf("-");
    storageRequire(split >= 6 && fields.size() >= split+4 && fields[2] == identity.toUtf8() &&
                       fields[3] == "/" && fields[split+1] == "ext4" &&
                       fields[5].split(',').contains("rw") && !fields[5].split(',').contains("ro") &&
                       fields[split+3].split(',').contains("rw") && !fields[split+3].split(',').contains("ro"),
                   "Persistent mount must be the expected writable ext4 root");
  }
  storageRequire(matches <= 1, "Stacked persistent mounts refused");
  return matches == 1;
}
void verifyPersistentStorage(const QString &root, const QJsonObject &contract) {
  const auto storage = contract["storage"].toObject();
  const auto mount = storage["mount_point"].toString(), device = storage["device"].toString();
  storageRequire(QDir::isAbsolutePath(mount) && mount != "/" && QDir::cleanPath(mount) == mount &&
                     QDir::isAbsolutePath(device) && storage["filesystem"] == "ext4" && root.startsWith(mount+'/'),
                 "Production storage contract missing or unsupported", "storage-contract-missing");
  plainPath(root);plainPath(mount);
  if (contract["board_id"]=="100ask-t113s4-sdnand") {
    storageRequire(mount=="/mnt/UDISK" && device=="/dev/by-name/UDISK",
                   "S4 persistent storage paths must match its boot contract");
    const auto boot=bootPersistentDevice(readBytes("/proc/cmdline",65536));
    storageRequire(!QFileInfo(device).canonicalFilePath().isEmpty() &&
                       QFileInfo(device).canonicalFilePath()==QFileInfo(boot).canonicalFilePath(),
                   "S4 persistent device differs from actual boot map");
    storageRequire(readBytes("/proc/device-tree/compatible",4096).split('\0').contains("allwinner,t113_s4"),
                   "S4 device-tree identity required", "storage-board-mismatch");
  }
  const auto info = readBytes("/proc/self/mountinfo", 1024*1024);
  storageRequire(persistentMountPresent(info,mount,blockIdentity(device)), "Persistent storage is not mounted");
  for (const auto &line : info.split('\n')) {
    auto fields = line.simplified().split(' ');
    if (fields.size() < 7) continue;
    const auto point = QString::fromUtf8(fields[4]);
    storageRequire(point == mount || !point.startsWith(mount+'/') ||
                       !(root == point || root.startsWith(point+'/')), "Nested persistent mount refused");
  }
  struct statvfs status;
  storageRequire(::statvfs(QFile::encodeName(mount).constData(),&status) == 0 && !(status.f_flag & ST_RDONLY),
                 "Persistent storage is read-only or unavailable; no formatting performed");
}
QJsonObject ensureSystemStorage(const QJsonObject &config, bool mount) {
  // This operation always reads actual kernel/device state, including in development mode.
  development(config);
  const auto contract = readJson(config["system_contract"].toString());
  const auto policy = contract["storage"].toObject();
  storageRequire(contract["board_id"] == "100ask-t113s4-sdnand" &&
                     policy["mount_point"] == "/mnt/UDISK" && policy["device"] == "/dev/by-name/UDISK" &&
                     policy["filesystem"] == "ext4", "S4 boot storage contract missing", "storage-contract-missing");
  storageRequire(readBytes("/proc/device-tree/compatible",4096).split('\0').contains("allwinner,t113_s4"),
                 "S4 device-tree identity required", "storage-board-mismatch");
  const auto device = bootPersistentDevice(readBytes("/proc/cmdline",65536));
  const auto identity = blockIdentity(device);
  const QString target = "/mnt/UDISK", alias = "/dev/by-name/UDISK";
  plainPath(target);
  QFileInfo aliasInfo(alias);
  if (aliasInfo.exists() || aliasInfo.isSymLink())
    storageRequire(aliasInfo.canonicalFilePath() == QFileInfo(device).canonicalFilePath(),
                   "UDISK alias differs from actual boot map");
  auto present = [&] { return persistentMountPresent(readBytes("/proc/self/mountinfo",1024*1024),target,identity); };
  if (!present()) {
    storageRequire(mount, "UDISK is not mounted; refusing rootfs data writes");
    auto type = fixedTool("/sbin/blkid",{device},10000);
    storageRequire(QRegularExpression("(^|\\s)TYPE=\"ext4\"(\\s|$)").match(QString::fromUtf8(type)).hasMatch(),
                   "Factory ext4 required; existing data left untouched");
    directory(target,PublicDirectory);
    fixedTool("/bin/mount",{"-t","ext4","-o","rw,nosuid,nodev",device,target},30000);
    storageRequire(present(), "UDISK mount verification failed");
  }
  struct statvfs status;
  storageRequire(::statvfs(QFile::encodeName(target).constData(),&status) == 0 && !(status.f_flag & ST_RDONLY),
                 "UDISK is read-only or unavailable");
  return {{"ok",true},{"device",device},{"filesystem","ext4"},{"mounted",true}};
}
QJsonObject initializeApplication(const QJsonObject &config) {
  const bool dev = development(config);
  const auto contract = readJson(config["system_contract"].toString());
  const auto apps = config["application_root"].toString(), data = contract["data_root"].toString();
  plainPath(apps);plainPath(data);
  storageRequire(contract["application_root"] == apps && contract["data_schema"] == 1 &&
                     QFileInfo(apps).absolutePath() == QFileInfo(data).absolutePath() && apps != data,
                 "Initialization application/data contract mismatch", "initialize-contract");
  storageRequire(dev || (!config.contains("factory_root") && !config.contains("defaults_root") &&
                     apps == "/mnt/UDISK/dshancontrol/apps" && data == "/mnt/UDISK/dshancontrol/data"),
                 "Production initialization paths are fixed", "initialize-contract");
  if (!dev) {ensureSystemStorage(config,false);verifyPersistentStorage(apps,contract);verifyPersistentStorage(data,contract);}
  const auto releases = apps+"/releases", current = apps+"/current", factory = releases+"/factory";
  plainPath(releases);
  QFileInfo active(current);
  const bool haveCurrent = active.exists() || active.isSymLink();
  QJsonObject identity;
  if (haveCurrent) {
    const auto path = active.canonicalFilePath();
    storageRequire(active.isSymLink() && !path.isEmpty() && QFileInfo(path).absolutePath() ==
                       QFileInfo(releases).canonicalFilePath(), "Invalid managed current link", "initialize-identity");
    identity=installed(path+"/installed.json");
  }
  const auto source = dev ? config["factory_root"].toString("/usr/share/dshancontrol/factory") :
                            QString("/usr/share/dshancontrol/factory");
  const auto defaults = dev ? config["defaults_root"].toString("/usr/share/dshancontrol/defaults") :
                              QString("/usr/share/dshancontrol/defaults");
  QMap<QString,QString> files;
  if (!haveCurrent) {
    identity=installed(source+"/installed.json");files=tree(source);
    storageRequire(files.contains("bin/omnigate-hmi") && files.contains("lib/web/app.py") &&
                       files.contains("lib/supervisor/supervisor.py"), "Incomplete factory application", "initialize-identity");
    if (QFileInfo(factory).exists() || QFileInfo(factory).isSymLink())
      storageRequire(tree(factory)==files, "Existing factory differs from immutable source", "initialize-identity");
  }
  // Preflight existing leaves before creating directories or writing defaults.
  plainPath(factory);
  for (const auto &name : {QString("web.json"),QString("hmi.json")}) {
    const auto path = data+'/'+name;plainPath(path);
    storageRequire(!QFileInfo(path).exists() || QFileInfo(path).isFile(), "Invalid existing user configuration");
    if (!QFileInfo(path).exists()) {plainPath(defaults+'/'+name);readJson(defaults+'/'+name);}
  }
  const auto managed = QFileInfo(apps).absolutePath();
  directory(managed,PublicDirectory);directory(apps,PublicDirectory);directory(releases,PublicDirectory);
  FileLock lock(apps+"/.update.lock");
  // Recheck after taking the same lock as application installation/recovery.
  storageRequire((haveCurrent || (!QFileInfo(current).exists() && !QFileInfo(current).isSymLink())) &&
                     QFileInfo(current).isSymLink() == haveCurrent &&
                     (!haveCurrent || installed(QFileInfo(current).canonicalFilePath()+"/installed.json") == identity),
                 "Application identity changed during initialization", "initialize-identity");
  directory(data,PrivateDirectory);
  for (const auto &name : {QString("web.json"),QString("hmi.json")}) {
    const auto destination=data+'/'+name;
    if (QFileInfo(destination).exists()) continue;
    const auto temporary=data+"/.initialize-default."+QUuid::createUuid().toString(QUuid::WithoutBraces);
    durableJson(temporary,readJson(defaults+'/'+name));
    // Publish only if absent; a concurrent user configuration must survive.
    const auto result=::link(QFile::encodeName(temporary).constData(),QFile::encodeName(destination).constData());
    const auto error=errno;
    storageRequire(result==0 || error==EEXIST,"Cannot publish initial configuration");
    plainPath(destination);
    storageRequire(QFileInfo(destination).isFile(),"Published configuration is not a regular file");
    storageRequire(QFile::remove(temporary),"Cannot clear default staging file");syncDirectory(data);
  }
  if (!haveCurrent) {
    if (!QFileInfo(factory).exists()) {
      qint64 bytes=0;for(auto it=files.cbegin();it!=files.cend();++it)bytes+=QFileInfo(source+'/'+it.key()).size();
      QStorageInfo available(releases);
      storageRequire(available.isValid() && available.bytesAvailable() >= bytes+4*1024*1024,
                     "Insufficient storage for factory initialization", "initialize-space");
      // Each attempt has its own staging tree. Interrupted trees remain intact
      // for diagnosis; never delete unknown files or overwrite a partial tree.
      const auto stage=releases+"/.factory-initialize."+QUuid::createUuid().toString(QUuid::WithoutBraces);
      directory(stage,PublicDirectory);
      for(auto it=files.cbegin();it!=files.cend();++it) {
        const auto destination=stage+'/'+it.key();
        QString parent=stage;
        const auto parts=it.key().split('/');
        for(int i=0;i<parts.size()-1;++i){parent+='/'+parts[i];directory(parent,PublicDirectory);}
        copyFile(source+'/'+it.key(),destination);
        storageRequire(hashFile(destination)==it.value(), "Factory source changed during copy");
      }
      syncTree(stage);
      storageRequire(QDir().rename(stage,factory), "Cannot publish initialized factory");syncDirectory(releases);
    }
    storageRequire(::symlink("releases/factory",QFile::encodeName(current).constData())==0,
                   "Cannot publish initial current link");syncDirectory(apps);
  }
  return {{"ok",true},{"initialized",!haveCurrent},{"application_commit",identity["commit"]},
          {"user_data_overwritten",false}};
}
