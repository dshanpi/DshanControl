#include "updater.h"
#include "developmentrelease.h"
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSet>
#include <QStorageInfo>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <fcntl.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

namespace {
const QString Repo = "dshanpi/DshanControl";
const QString AssetPrefix =
    "https://github.com/" + Repo + "/releases/download/";
void preparePublicVersion(const QString &directory) {
  // The daemon keeps umask077. Only a fully verified application tree becomes
  // readable/executable by the reduced roles; inputs, locks and state stay private.
  QStringList folders{directory}, files;
  for (int i = 0; i < folders.size(); ++i) {
    struct stat info;
    require(::lstat(QFile::encodeName(folders[i]).constData(), &info) == 0 &&
                S_ISDIR(info.st_mode) && info.st_uid == ::geteuid(),
            "Untrusted version directory", "staging", "publication-permissions");
    for (const auto &entry : QDir(folders[i]).entryInfoList(
             QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System)) {
      require(!entry.isSymLink(), "Linked version entry refused", "staging",
              "publication-permissions");
      if (entry.isDir())
        folders << entry.absoluteFilePath();
      else
        files << entry.absoluteFilePath();
    }
  }
  auto publish = [](const QString &path, bool folder) {
    const int fd = ::open(QFile::encodeName(path).constData(),
                          O_RDONLY | O_NOFOLLOW | O_CLOEXEC |
                              (folder ? O_DIRECTORY : 0));
    require(fd >= 0, "Cannot open version permissions", "staging",
            "publication-permissions");
    struct stat info;
    const bool trusted = ::fstat(fd, &info) == 0 && info.st_uid == ::geteuid() &&
        (folder ? S_ISDIR(info.st_mode) : S_ISREG(info.st_mode) && info.st_nlink == 1);
    const mode_t mode = folder || (trusted && (info.st_mode & 0111)) ? 0755 : 0644;
    const bool complete = trusted && ::fchmod(fd, mode) == 0 && ::fsync(fd) == 0;
    ::close(fd);
    require(complete, "Cannot publish version permissions", "staging",
            "publication-permissions");
  };
  for (const auto &file : files)
    publish(file, false);
  // Keep the private staging root inaccessible until all descendants are ready.
  for (int i = folders.size() - 1; i >= 0; --i)
    publish(folders[i], true);
}
class OwnedHookProcess : public QProcess {
  const pid_t owner = ::getpid();
  int updateLease = -1, controlLease = -1;

protected:
  void setupChildProcess() override {
    // A crashed installer must not leave a detached lifecycle helper executing.
    if (::setsid() < 0 || ::prctl(PR_SET_PDEATHSIG, SIGKILL) < 0 ||
        ::getppid() != owner)
      ::_exit(127);
    for (const auto descriptor : {updateLease, controlLease})
      if (descriptor >= 0 && ::fcntl(descriptor, F_SETFD, 0) < 0)
        ::_exit(127);
  }

public:
  OwnedHookProcess(int update, int control) {
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.remove("DSHANCONTROL_UPDATE_LEASE_FD");
    environment.remove("DSHANCONTROL_CONTROL_LEASE_FD");
    if (update < 0 && control < 0) {
      setProcessEnvironment(environment);
      return;
    }
    require(update >= 0 && control >= 0, "Incomplete application lease handoff",
            "resource-lock");
    updateLease = ::fcntl(update, F_DUPFD_CLOEXEC, 200);
    controlLease = ::fcntl(control, F_DUPFD_CLOEXEC, 200);
    if (updateLease < 0 || controlLease < 0) {
      if (updateLease >= 0)
        ::close(updateLease);
      if (controlLease >= 0)
        ::close(controlLease);
      throw Failure("lease-duplicate", "resource-lock",
                    "Cannot reserve application lease descriptors");
    }
    environment.insert("DSHANCONTROL_UPDATE_LEASE_FD",
                       QString::number(updateLease));
    environment.insert("DSHANCONTROL_CONTROL_LEASE_FD",
                       QString::number(controlLease));
    setProcessEnvironment(environment);
  }
  ~OwnedHookProcess() {
    if (updateLease >= 0)
      ::close(updateLease);
    if (controlLease >= 0)
      ::close(controlLease);
  }
};
class ActiveLeases {
  int &update, &control;

public:
  ActiveLeases(int &u, int &c, const FileLock &a, const FileLock &b)
      : update(u), control(c) {
    update = a.descriptor();
    control = b.descriptor();
  }
  ~ActiveLeases() { update = control = -1; }
};

void removeFile(const QString &path) {
  require(QFile::remove(path), "Cannot clear persistent transaction",
          "storage");
}
void copyBounded(const QString &source, const QString &target, qint64 maximum) {
  QFile input(source), output(target);
  require(input.open(QIODevice::ReadOnly) && input.size() <= maximum &&
              output.open(QIODevice::WriteOnly | QIODevice::NewOnly),
          "Cannot snapshot update input", "storage");
  qint64 total = 0;
  while (!input.atEnd()) {
    auto block = input.read(65536);
    total += block.size();
    require(input.error() == QFile::NoError && total <= maximum &&
                output.write(block) == block.size(),
            "Update input changed or copy failed", "storage");
  }
  require(output.flush() && ::fsync(output.handle()) == 0,
          "Snapshot fsync failed", "storage");
}
QByteArray fetch(QString url, qint64 limit, QFile *output = nullptr) {
  QNetworkAccessManager manager;
  for (int redirects = 0; redirects < 6; ++redirects) {
    QUrl target(url);
    require(target.isValid() && target.scheme() == "https" &&
                target.userInfo().isEmpty(),
            "HTTPS required", "download");
    QNetworkRequest request(target);
    request.setRawHeader("User-Agent", "DshanControl-NativeUpdater/1");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    QNetworkReply *reply = manager.get(request);
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    bool timedOut = false, tooLarge = false, failedWrite = false;
    qint64 total = 0;
    QByteArray bytes;
    QObject::connect(&timer, &QTimer::timeout, &loop, [&]() {
      timedOut = true;
      reply->abort();
    });
    auto consume = [&]() {
      while (reply->bytesAvailable()) {
        QByteArray block = reply->read(65536);
        total += block.size();
        if (total > limit) {
          tooLarge = true;
          reply->abort();
          return;
        }
        // Redirect response bodies do not enter the payload file.
        if (reply->attribute(QNetworkRequest::RedirectionTargetAttribute)
                .isValid())
          continue;
        if (output) {
          if (output->write(block) != block.size()) {
            failedWrite = true;
            reply->abort();
            return;
          }
        } else
          bytes.append(block);
      }
    };
    QObject::connect(reply, &QNetworkReply::readyRead, &loop, consume);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(30000);
    loop.exec();
    timer.stop();
    consume();
    auto redirect =
        reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
    auto error = reply->error();
    int status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();
    require(!timedOut, "Network response timed out", "download",
            "network-timeout");
    require(!tooLarge && !failedWrite, "Download size limit/write failure",
            "download", tooLarge ? "size-exceeded" : "write-failed");
    require(error == QNetworkReply::NoError, "Network request failed",
            "download", QString::number(static_cast<int>(error)));
    if (!redirect.isEmpty()) {
      url = target.resolved(redirect).toString();
      continue;
    }
    require(status >= 200 && status < 300, "HTTP request rejected", "download",
            QString::number(status));
    return bytes;
  }
  throw Failure("redirect-limit", "download", "Too many redirects");
}
void assetUrl(const QString &url) {
  require(url.startsWith(AssetPrefix), "Asset outside configured repository",
          "download");
}
void writeBytes(const QString &path, const QByteArray &data) {
  QFile file(path);
  require(file.open(QIODevice::WriteOnly) && file.write(data) == data.size() &&
              file.flush() && ::fsync(file.handle()) == 0,
          "Downloaded metadata write failed", "storage");
}
QJsonObject draftReleasePin(const QJsonObject &release) {
  return {{"id", release["id"]},
          {"tag_name", release["tag_name"]},
          {"draft", release["draft"]},
          {"prerelease", release["prerelease"]}};
}
QJsonObject draftAssets(const QJsonObject &release) {
  require(release["assets"].isArray(), "Pinned draft assets missing",
          "checking");
  QJsonObject assets;
  for (const auto &value : release["assets"].toArray()) {
    require(value.isObject(), "Invalid pinned draft asset", "checking");
    const auto asset = value.toObject();
    const auto name = asset["name"].toString();
    require(identifier(name) && !assets.contains(name) &&
                positiveInteger(asset["id"]) &&
                positiveInteger(asset["size"]) && asset["state"] == "uploaded",
            "Invalid or duplicate pinned draft asset", "checking");
    require(asset["digest"].isUndefined() || asset["digest"].isNull() ||
                asset["digest"].isString(),
            "Invalid pinned asset digest type", "checking");
    const auto digest =
        asset["digest"].isString() ? asset["digest"].toString() : QString();
    require(digest.isEmpty() ||
                (digest.startsWith("sha256:") && sha256(digest.mid(7))),
            "Unknown pinned asset digest", "checking");
    assets[name] = QJsonObject{{"id", asset["id"]},
                               {"name", name},
                               {"size", asset["size"]},
                               {"digest", digest}};
  }
  return assets;
}
QString draftAssetUrl(const QJsonObject &asset) {
  return "https://api.github.com/repos/" + Repo + "/releases/assets/" +
         QString::number(qint64(asset["id"].toDouble()));
}
} // namespace
NativeUpdater::NativeUpdater(const QJsonObject &configuration)
    : config(configuration) {
  DevelopmentRelease development(configuration);
  require(!config.contains("development_mode") ||
              config["development_mode"].isBool(),
          "development_mode must be a boolean");
  contract = readJson(config["system_contract"].toString());
  root = config["application_root"].toString();
  require(QDir::isAbsolutePath(root) && root != "/" &&
              QDir::cleanPath(root) == root &&
              root == contract["application_root"].toString(),
          "Application root differs from system contract");
  if (!config["development_mode"].toBool(false))
    verifyPersistentStorage(root, contract);
  require(!QFileInfo(root).isSymLink(), "Application root must not be a link");
  releases = root + "/releases";
  require(QDir().mkpath(releases) && !QFileInfo(releases).isSymLink(),
          "Cannot open version directory", "storage");
}
QString NativeUpdater::controlLock() const {
  return config["control_lock"].toString(root + "/.control.lock");
}
QString NativeUpdater::current() const {
  QFileInfo info(root + "/current");
  require(info.isSymLink(), "Factory application not initialized", "identity");
  auto path = info.canonicalFilePath();
  require(!path.isEmpty() && QFileInfo(path).absolutePath() ==
                                 QFileInfo(releases).canonicalFilePath(),
          "Current points outside release directory", "identity");
  return path;
}
QJsonObject NativeUpdater::state() const {
  auto path = root + "/update-state.json";
  auto value = QFileInfo::exists(path)
                   ? readJson(path)
                   : QJsonObject{{"high_water", 0}, {"failed", QJsonArray{}}};
  require(positiveInteger(value["high_water"], true) &&
              value["failed"].isArray(),
          "Invalid accepted sequence/failed state", "recovery");
  for (const auto &name : value["failed"].toArray())
    require(identifier(name.toString()), "Invalid failed version", "recovery");
  return value;
}
bool NativeUpdater::hook(const QString &operation, bool checked) {
  const auto processPolicy = config["application_health"].toObject();
  const bool managed = processPolicy["launch_receipts_required"].toBool(false);
  OwnedHookProcess process(managed ? activeUpdateLease : -1,
                           managed ? activeControlLease : -1);
  process.setProcessChannelMode(QProcess::MergedChannels);
  process.start(config["service_hook"].toString(), {operation});
  bool started = process.waitForStarted(3000);
  QByteArray tail;
  QElapsedTimer clock;
  clock.start();
  while (started && process.state() != QProcess::NotRunning &&
         clock.elapsed() < 120000) {
    process.waitForReadyRead(100);
    tail += process.readAll();
    if (tail.size() > 2048)
      tail = tail.right(2048);
  }
  bool timeout = started && process.state() != QProcess::NotRunning;
  if (timeout) {
    const auto child = static_cast<pid_t>(process.processId());
    if (child > 0)
      ::kill(-child, SIGKILL);
    process.kill();
    process.waitForFinished(3000);
  }
  bool ok = started && !timeout &&
            process.exitStatus() == QProcess::NormalExit &&
            process.exitCode() == 0;
  hookFailureCode =
      ok ? QString()
         : !started ? QString("hook-start-failed")
                    : timeout ? QString("hook-timeout")
                              : process.exitStatus() != QProcess::NormalExit
                                    ? QString("hook-crashed")
                                    : QString::number(process.exitCode());
  // Do not echo hook stdout/stderr; it can contain site credentials. Keep the
  // raw exit code.
  if (checked && !ok)
    throw Failure(hookFailureCode, operation,
                  "System hook failed: " + operation);
  return ok;
}
bool NativeUpdater::healthy() {
  int interval = qBound(1, config["health_interval_seconds"].toInt(2), 30);
  int startup = qBound(0, config["startup_timeout_seconds"].toInt(30), 300);
  int observation =
      qBound(0, config["health_observation_seconds"].toInt(30), 300);
  QElapsedTimer clock;
  clock.start();
  while (!hook("health", false)) {
    if (clock.elapsed() >= startup * 1000)
      return false;
    QThread::sleep(interval);
  }
  clock.restart();
  while (clock.elapsed() < observation * 1000) {
    QThread::sleep(interval);
    if (!hook("health", false))
      return false;
  }
  return true;
}
void NativeUpdater::switchTo(const QString &name) {
  require(identifier(name) && QFileInfo(releases + '/' + name).isDir() &&
              !QFileInfo(releases + '/' + name).isSymLink(),
          "Invalid release selection", "switching");
  QString next = root + "/.current.next";
  if (QFileInfo(next).isSymLink())
    removeFile(next);
  require(!QFileInfo::exists(next) &&
              ::symlink(QFile::encodeName("releases/" + name).constData(),
                        QFile::encodeName(next).constData()) == 0,
          "Cannot create version selection", "switching");
  require(::rename(QFile::encodeName(next).constData(),
                   QFile::encodeName(root + "/current").constData()) == 0,
          "Atomic version switch failed", "switching");
  syncDirectory(root);
}
void NativeUpdater::rollback(const QJsonObject &tx) {
  progress("rollback");
  hook("stop");
  switchTo(tx["old"].toString());
  hook("start");
  if (!healthy())
    throw Failure(hookFailureCode, "rollback",
                  "Rollback health failed; transaction retained");
  auto value = state();
  auto failed = value["failed"].toArray();
  if (!failed.contains(tx["new"]))
    failed.append(tx["new"]);
  value["failed"] = failed;
  durableJson(root + "/update-state.json", value);
  removeFile(root + "/transaction.json");
  syncDirectory(root);
}
void NativeUpdater::recoverInternal() {
  QString path = root + "/transaction.json";
  if (!QFileInfo::exists(path))
    return;
  auto tx = readJson(path);
  require(identifier(tx["old"].toString()) && identifier(tx["new"].toString()),
          "Invalid transaction identity", "recovery");
  auto phase = tx["phase"].toString();
  require(phase == "prepared" || phase == "switched" || phase == "committed",
          "Unknown transaction phase", "recovery");
  if (phase == "committed") {
    removeFile(path);
    syncDirectory(root);
  } else
    rollback(tx);
}
void NativeUpdater::recover() {
  FileLock lock(root + "/.update.lock"), guard(controlLock());
  ActiveLeases active(activeUpdateLease, activeControlLease, lock, guard);
  recoverInternal();
}
QString NativeUpdater::install(const QString &manifestPath,
                               const QString &signaturePath,
                               const QString &packagePath) {
  FileLock lock(root + "/.update.lock"), guard(controlLock());
  ActiveLeases active(activeUpdateLease, activeControlLease, lock, guard);
  progress("recovery");
  recoverInternal();
  QTemporaryDir snapshot(root + "/.download-XXXXXX");
  require(snapshot.isValid(), "Cannot snapshot inputs", "storage");
  copyBounded(manifestPath, snapshot.path() + "/manifest.json", 1024 * 1024);
  copyBounded(signaturePath, snapshot.path() + "/signature", 16384);
  progress("signature");
  auto manifestBytes =
      readBytes(snapshot.path() + "/manifest.json", 1024 * 1024);
  verifySignature(manifestBytes,
                  readBytes(snapshot.path() + "/signature", 16384),
                  config["public_key"].toString());
  auto json = strictJson(manifestBytes);
  require(json.isObject(), "Invalid manifest object");
  auto m = json.toObject();
  progress("compatibility");
  validateManifest(m, !config["development_mode"].toBool(false));
  auto value = state();
  auto old = current();
  auto item = matchTarget(m, contract, installed(),
                          static_cast<qint64>(value["high_water"].toDouble()));
  qint64 size = static_cast<qint64>(item["size"].toDouble()),
         expanded = static_cast<qint64>(item["unpacked_size"].toDouble());
  require(QFileInfo(packagePath).isFile() &&
              QFileInfo(packagePath).size() == size,
          "Package size mismatch");
  require(QStorageInfo(root).bytesAvailable() >
              size + expanded + 16 * 1024 * 1024,
          "Insufficient free space", "storage");
  copyBounded(packagePath, snapshot.path() + "/package.tar.gz", size);
  require(QFileInfo(snapshot.path() + "/package.tar.gz").size() == size &&
              hashFile(snapshot.path() + "/package.tar.gz") ==
                  item["sha256"].toString(),
          "Package hash/size mismatch");
  auto name = QString::number(static_cast<qint64>(m["sequence"].toDouble())) +
              '-' + item["sha256"].toString().left(16);
  require(!value["failed"].toArray().contains(name),
          "Known failed release quarantined");
  progress("staging");
  QTemporaryDir stage(releases + "/.stage-XXXXXX");
  require(stage.isValid(), "Cannot stage application", "storage");
  extractPackage(snapshot.path() + "/package.tar.gz", stage.path(), expanded);
  durableJson(stage.path() + "/installed.json",
              {{"version", m["version"]},
               {"commit", m["commit"]},
               {"sequence", m["sequence"]},
               {"data_schema", m["data_schema"]},
               {"package_sha256", item["sha256"]}});
  preparePublicVersion(stage.path());
  auto destination = releases + '/' + name;
  require(destination != old && !QFileInfo(destination).isSymLink(),
          "Invalid staging destination");
  if (QFileInfo::exists(destination))
    require(QDir(destination).removeRecursively(),
            "Cannot replace interrupted stage", "storage");
  require(QDir().rename(stage.path(), destination), "Cannot finalize staging",
          "storage");
  syncDirectory(releases);
  progress("maintenance");
  hook("maintenance");
  QJsonObject tx{
      {"old", QFileInfo(old).fileName()}, {"new", name}, {"phase", "prepared"}};
  durableJson(root + "/transaction.json", tx);
  value["high_water"] = m["sequence"];
  durableJson(root + "/update-state.json", value);
  try {
    progress("stopping");
    hook("stop");
    progress("switching");
    switchTo(name);
    tx["phase"] = "switched";
    durableJson(root + "/transaction.json", tx);
    progress("starting");
    hook("start");
    progress("health");
    if (!healthy())
      throw Failure(hookFailureCode, "health", "New application health failed");
    tx["phase"] = "committed";
    durableJson(root + "/transaction.json", tx);
    removeFile(root + "/transaction.json");
    syncDirectory(root);
  } catch (const Failure &original) {
    try {
      rollback(tx);
    } catch (const Failure &recovery) {
      throw Failure(original.code, original.stage,
                    original.detail + "; recovery failed: " + recovery.code +
                        " at " + recovery.stage + " (transaction retained)");
    }
    throw;
  }
  progress("completed");
  return name;
}
QJsonObject NativeUpdater::check() {
  DevelopmentRelease development(config);
  if (development.enabled())
    return checkDevelopment(development);
  progress("checking");
  QJsonObject best;
  auto currentInfo = installed();
  auto watermark = state()["high_water"].toDouble();
  QElapsedTimer deadline;
  deadline.start();
  for (int page = 1; page <= 10; ++page) {
    require(deadline.elapsed() < 600000, "Update search timed out", "checking");
    auto json = strictJson(
        fetch("https://api.github.com/repos/" + Repo +
                  "/releases?per_page=100&page=" + QString::number(page),
              4 * 1024 * 1024));
    require(json.isArray(), "Invalid GitHub release response", "checking");
    auto entries = json.toArray();
    for (const auto &entry : entries) {
      auto release = entry.toObject();
      if (release["draft"].toBool() ||
          !release["tag_name"].toString().startsWith("app-v"))
        continue;
      QJsonObject assets;
      for (const auto &asset : release["assets"].toArray()) {
        auto a = asset.toObject();
        assets[a["name"].toString()] = a["browser_download_url"];
      }
      if (!assets.contains("application-release.json") ||
          !assets.contains("application-release.json.sig"))
        continue;
      try {
        require(deadline.elapsed() < 600000, "Update search timed out",
                "checking");
        auto manifestUrl = assets["application-release.json"].toString(),
             signatureUrl = assets["application-release.json.sig"].toString();
        assetUrl(manifestUrl);
        assetUrl(signatureUrl);
        auto bytes = fetch(manifestUrl, 1024 * 1024);
        verifySignature(bytes, fetch(signatureUrl, 16384),
                        config["public_key"].toString());
        auto value = strictJson(bytes);
        require(value.isObject(), "Invalid manifest");
        auto m = value.toObject();
        validateManifest(m, true);
        require(m["tag"] == release["tag_name"] &&
                    (m["channel"] == "prerelease") ==
                        release["prerelease"].toBool(),
                "GitHub release identity/channel mismatch");
        auto target = matchTarget(m, contract, currentInfo,
                                  static_cast<qint64>(watermark));
        require(assets.contains(target["filename"].toString()),
                "Target asset missing");
        auto name =
            QString::number(static_cast<qint64>(m["sequence"].toDouble())) +
            '-' + target["sha256"].toString().left(16);
        require(!state()["failed"].toArray().contains(name),
                "Failed release quarantined");
        auto packageUrl = assets[target["filename"].toString()].toString();
        assetUrl(packageUrl);
        if (m["sequence"].toDouble() > best["sequence"].toDouble())
          best = {{"version", m["version"]},
                  {"sequence", m["sequence"]},
                  {"manifest_url", manifestUrl},
                  {"signature_url", signatureUrl},
                  {"package_url", packageUrl},
                  {"manifest_sha256", hashBytes(bytes)},
                  {"package_sha256", target["sha256"]}};
      } catch (const Failure &) { /* Invalid/incompatible releases never become
                                     candidates. */
      }
    }
    if (entries.size() < 100)
      break;
  }
  return best;
}
QJsonObject NativeUpdater::download(const QJsonObject &candidate,
                                    const QString &directory) {
  DevelopmentRelease development(config);
  if (development.enabled())
    return downloadDevelopment(development, candidate, directory);
  require(!candidate.contains("development_release"),
          "Development candidate is not enabled", "download");
  FileLock lock(root + "/.update.lock");
  progress("download-metadata");
  require(QDir().mkpath(directory), "Cannot create download directory",
          "storage");
  auto manifestUrl = candidate["manifest_url"].toString(),
       signatureUrl = candidate["signature_url"].toString(),
       packageUrl = candidate["package_url"].toString();
  assetUrl(manifestUrl);
  assetUrl(signatureUrl);
  assetUrl(packageUrl);
  auto bytes = fetch(manifestUrl, 1024 * 1024),
       signature = fetch(signatureUrl, 16384);
  verifySignature(bytes, signature, config["public_key"].toString());
  auto value = strictJson(bytes);
  require(value.isObject(), "Invalid manifest");
  auto m = value.toObject();
  validateManifest(m, true);
  auto target =
      matchTarget(m, contract, installed(),
                  static_cast<qint64>(state()["high_water"].toDouble()));
  require(m["version"] == candidate["version"] &&
              m["sequence"] == candidate["sequence"] &&
              hashBytes(bytes) == candidate["manifest_sha256"].toString() &&
              target["sha256"] == candidate["package_sha256"],
          "Selected release changed");
  require(packageUrl.endsWith('/' + target["filename"].toString()),
          "Asset selection changed");
  auto partial = directory + "/.package.part";
  QFile output(partial);
  qint64 size = static_cast<qint64>(target["size"].toDouble());
  require(QStorageInfo(directory).bytesAvailable() > size + 16 * 1024 * 1024 &&
              output.open(QIODevice::WriteOnly),
          "No download space or unwritable destination", "storage");
  try {
    progress("download-package");
    fetch(packageUrl, size, &output);
    require(output.flush() && ::fsync(output.handle()) == 0,
            "Download fsync failed", "storage");
    output.close();
    require(QFileInfo(partial).size() == size &&
                hashFile(partial) == target["sha256"].toString(),
            "Downloaded payload hash/size mismatch");
    writeBytes(directory + "/application-release.json", bytes);
    writeBytes(directory + "/application-release.json.sig", signature);
    auto destination = directory + '/' + target["filename"].toString();
    if (QFileInfo::exists(destination))
      removeFile(destination);
    require(QFile::rename(partial, destination), "Cannot finalize download",
            "storage");
    syncDirectory(directory);
  } catch (...) {
    output.close();
    QFile::remove(partial);
    throw;
  }
  return target;
}

QJsonObject NativeUpdater::checkDevelopment(const DevelopmentRelease &source,
                                            QByteArray *manifestBytes,
                                            QByteArray *signatureBytes) {
  if (!manifestBytes)
    progress("checking");
  const auto release = source.metadata(), assets = draftAssets(release);
  const auto manifestAsset = assets["application-release.json"].toObject();
  const auto signatureAsset = assets["application-release.json.sig"].toObject();
  require(!manifestAsset.isEmpty() && !signatureAsset.isEmpty(),
          "Pinned draft manifest/signature missing", "checking");
  require(manifestAsset["size"].toDouble() <= 1024 * 1024 &&
              signatureAsset["size"].toDouble() <= 16384,
          "Pinned metadata exceeds size bound", "checking");
  const auto bytes =
      source.asset(qint64(manifestAsset["id"].toDouble()), 1024 * 1024);
  const auto signature =
      source.asset(qint64(signatureAsset["id"].toDouble()), 16384);
  require(bytes.size() == manifestAsset["size"].toDouble() &&
              signature.size() == signatureAsset["size"].toDouble(),
          "Pinned metadata size changed", "checking");
  verifySignature(bytes, signature, config["public_key"].toString());
  const auto value = strictJson(bytes);
  require(value.isObject(), "Invalid pinned draft manifest");
  const auto manifest = value.toObject();
  validateManifest(manifest, false);
  require(manifest["release_state"] == "draft" &&
              manifest["tag"] == release["tag_name"] &&
              (manifest["channel"] == "prerelease") ==
                  release["prerelease"].toBool(),
          "Pinned draft identity/channel mismatch", "checking");
  const auto target = matchTarget(manifest, contract, installed(),
                                  qint64(state()["high_water"].toDouble()));
  const auto package = assets[target["filename"].toString()].toObject();
  require(!package.isEmpty() && package["size"] == target["size"],
          "Pinned target asset missing or changed", "checking");
  const auto digest = package["digest"].toString();
  require(digest.isEmpty() || digest == "sha256:" + target["sha256"].toString(),
          "Pinned target digest differs from signed manifest", "checking");
  const auto name = QString::number(qint64(manifest["sequence"].toDouble())) +
                    '-' + target["sha256"].toString().left(16);
  require(!state()["failed"].toArray().contains(name),
          "Failed pinned draft remains quarantined", "checking");
  if (manifestBytes)
    *manifestBytes = bytes;
  if (signatureBytes)
    *signatureBytes = signature;
  return {{"version", manifest["version"]},
          {"sequence", manifest["sequence"]},
          {"development_release", true},
          {"release", draftReleasePin(release)},
          {"asset_pins", QJsonObject{{"manifest", manifestAsset},
                                     {"signature", signatureAsset},
                                     {"package", package}}},
          {"manifest_url", draftAssetUrl(manifestAsset)},
          {"signature_url", draftAssetUrl(signatureAsset)},
          {"package_url", draftAssetUrl(package)},
          {"manifest_sha256", hashBytes(bytes)},
          {"signature_sha256", hashBytes(signature)},
          {"package_sha256", target["sha256"]}};
}

QJsonObject NativeUpdater::downloadDevelopment(const DevelopmentRelease &source,
                                               const QJsonObject &candidate,
                                               const QString &directory) {
  FileLock lock(root + "/.update.lock");
  progress("download-metadata");
  require(candidate["development_release"].isBool() &&
              candidate["development_release"].toBool(),
          "Pinned development candidate required", "download");
  QByteArray bytes, signature;
  const auto fresh = checkDevelopment(source, &bytes, &signature);
  require(fresh == candidate, "Selected draft or asset identity changed",
          "download", "development-release-changed");
  const auto manifest = strictJson(bytes).toObject();
  const auto target = matchTarget(manifest, contract, installed(),
                                  qint64(state()["high_water"].toDouble()));
  const auto package = candidate["asset_pins"].toObject()["package"].toObject();
  require(QDir().mkpath(directory) && !QFileInfo(directory).isSymLink(),
          "Cannot create private download directory", "storage");
  const auto size = qint64(target["size"].toDouble());
  const auto partial = directory + "/.package.part";
  QFile output(partial);
  require(QStorageInfo(directory).bytesAvailable() > size + 16 * 1024 * 1024 &&
              output.open(QIODevice::WriteOnly | QIODevice::NewOnly),
          "No download space or existing partial retained", "storage");
  progress("download-package");
  source.asset(qint64(package["id"].toDouble()), size, &output);
  require(output.flush() && ::fsync(output.handle()) == 0,
          "Pinned payload fsync failed", "storage");
  output.close();
  require(QFileInfo(partial).size() == size &&
              hashFile(partial) == target["sha256"].toString(),
          "Pinned payload hash/size mismatch", "download");
  writeBytes(directory + "/application-release.json", bytes);
  writeBytes(directory + "/application-release.json.sig", signature);
  const auto destination = directory + '/' + target["filename"].toString();
  require(!QFileInfo(destination).exists() &&
              !QFileInfo(destination).isSymLink() &&
              QFile::rename(partial, destination),
          "Pinned download destination exists; state retained", "storage");
  syncDirectory(directory);
  return target;
}
