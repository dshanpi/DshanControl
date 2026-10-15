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
#include <QRegularExpression>
#include <QSet>
#include <QTimer>
#include <QUrl>
#include <fcntl.h>
#include <linux/magic.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <unistd.h>

namespace {
const QString Api =
    "https://api.github.com/repos/dshanpi/DshanControl/releases/";
void check(bool value, const QString &message,
           const QString &code = "development-release") {
  require(value, message, "download", code);
}
QByteArray secret(const QString &path) {
  const auto parent = QFileInfo(path).absolutePath();
  bool permitted = false;
  for (const auto &prefix : {QString("/run/dshancontrol-development-"),
                             QString("/tmp/dshancontrol-development-"),
                             QString("/dev/shm/dshancontrol-development-")})
    if (parent.startsWith(prefix) && QRegularExpression("^[A-Za-z0-9_-]{8,64}$")
                                         .match(parent.mid(prefix.size()))
                                         .hasMatch())
      permitted = true;
  check(permitted && QDir::cleanPath(path) == path &&
            QFileInfo(path).fileName() == "token" &&
            !QFileInfo(parent).isSymLink(),
        "Credential must use an explicit private volatile directory",
        "credential-path");
  struct stat directory;
  check(::lstat(QFile::encodeName(parent).constData(), &directory) == 0 &&
            S_ISDIR(directory.st_mode) && directory.st_uid == ::geteuid() &&
            (directory.st_mode & 07777) == 0700,
        "Private credential directory unavailable", "credential-permissions");
  const auto fd = ::open(QFile::encodeName(path).constData(),
                         O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
  check(fd >= 0, "Private development credential unavailable",
        "credential-unavailable");
  QByteArray value;
  try {
    struct stat info;
    struct statfs filesystem;
    check(::fstat(fd, &info) == 0 && S_ISREG(info.st_mode) &&
              info.st_nlink == 1 && info.st_uid == ::geteuid() &&
              (info.st_mode & 07777) == 0600 && info.st_size > 0 &&
              info.st_size <= 4096,
          "Unsafe development credential file", "credential-permissions");
    check(::fstatfs(fd, &filesystem) == 0 && filesystem.f_type == TMPFS_MAGIC,
          "Development credential must remain on tmpfs",
          "credential-persistence");
    value.resize(int(info.st_size));
    check(::read(fd, value.data(), value.size()) == value.size(),
          "Cannot read bounded development credential", "credential-read");
    struct stat after;
    check(::fstat(fd, &after) == 0 && after.st_dev == info.st_dev &&
              after.st_ino == info.st_ino && after.st_size == info.st_size &&
              after.st_uid == info.st_uid && after.st_mode == info.st_mode,
          "Development credential changed during read", "credential-changed");
    ::close(fd);
  } catch (...) {
    ::close(fd);
    throw;
  }
  value = value.trimmed();
  check(QRegularExpression("^[A-Za-z0-9_]{20,1024}$")
            .match(QString::fromLatin1(value))
            .hasMatch(),
        "Invalid development credential format", "credential-format");
  return value;
}
bool downloadHost(const QUrl &url) {
  return url.isValid() && url.scheme() == "https" && url.userInfo().isEmpty() &&
         url.port(443) == 443 && url.fragment().isEmpty() &&
         QSet<QString>{"release-assets.githubusercontent.com",
                       "objects.githubusercontent.com"}
             .contains(url.host().toLower());
}
} // namespace
DevelopmentRelease::DevelopmentRelease(const QJsonObject &config) {
  if (!config.contains("development_release"))
    return;
  check(config["development_mode"].isBool() &&
            config["development_mode"].toBool() &&
            config["automatic_install"].isBool() &&
            !config["automatic_install"].toBool(),
        "Pinned development releases require explicit development mode and "
        "disabled automatic installation");
  check(config["development_release"].isObject(),
        "Invalid pinned development release");
  const auto policy = config["development_release"].toObject();
  check(policy.size() == 2 && policy.contains("release_id") &&
            policy.contains("token_file") &&
            positiveInteger(policy["release_id"]) &&
            policy["token_file"].isString(),
        "Pinned release ID and private credential path required");
  releaseId = qint64(policy["release_id"].toDouble());
  credential = policy["token_file"].toString();
  check(!credential.isEmpty(), "Private credential path required");
}
QJsonObject DevelopmentRelease::metadata() const {
  check(enabled(), "Pinned development release not configured");
  const auto value =
      strictJson(fetch(QString::number(releaseId), 4 * 1024 * 1024, false));
  check(value.isObject(), "Invalid pinned release response");
  const auto release = value.toObject();
  check(release["id"].toDouble() == double(releaseId) &&
            release["draft"].isBool() && release["draft"].toBool() &&
            release["prerelease"].isBool() &&
            release["tag_name"].toString().startsWith("app-v"),
        "Pinned release is no longer the selected application draft",
        "development-release-changed");
  return release;
}
QByteArray DevelopmentRelease::asset(qint64 assetId, qint64 maximum,
                                     QFile *destination) const {
  check(enabled() && assetId > 0, "Invalid pinned asset identity");
  return fetch("assets/" + QString::number(assetId), maximum, true,
               destination);
}
QByteArray DevelopmentRelease::fetch(const QString &path, qint64 maximum,
                                     bool asset, QFile *destination) const {
  check(maximum > 0, "Invalid bounded asset size");
  const auto token = secret(credential);
  QNetworkAccessManager manager;
  QUrl target(Api + path);
  for (int redirect = 0; redirect < 6; ++redirect) {
    QNetworkRequest request(target);
    request.setRawHeader("User-Agent", "DshanControl-NativeUpdater/1");
    request.setRawHeader("Accept-Encoding", "identity");
    request.setRawHeader("Accept", asset ? "application/octet-stream"
                                         : "application/vnd.github+json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    if (redirect == 0) {
      check(target.toString() == Api + path &&
                target.host() == "api.github.com" && target.port(443) == 443 &&
                target.userInfo().isEmpty() && !target.hasQuery() &&
                !target.hasFragment(),
            "Credential endpoint differs from fixed repository",
            "credential-endpoint");
      request.setRawHeader("Authorization", QByteArray("Bearer ") + token);
      request.setRawHeader("X-GitHub-Api-Version", "2026-03-10");
    } else
      check(asset && downloadHost(target), "Untrusted asset redirect refused",
            "asset-redirect");
    auto *reply = manager.get(request);
    reply->setReadBufferSize(65536);
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    bool timeout = false, large = false, writeFailure = false;
    QByteArray bytes;
    qint64 total = 0;
    QObject::connect(&timer, &QTimer::timeout, &loop, [&] {
      timeout = true;
      reply->abort();
    });
    auto consume = [&] {
      while (reply->bytesAvailable() > 0) {
        const auto block = reply->read(65536);
        total += block.size();
        if (total > maximum) {
          large = true;
          reply->abort();
          return;
        }
        if (reply->attribute(QNetworkRequest::RedirectionTargetAttribute)
                .isValid())
          continue;
        if (destination) {
          if (destination->write(block) != block.size()) {
            writeFailure = true;
            reply->abort();
            return;
          }
        } else
          bytes += block;
      }
    };
    QObject::connect(reply, &QNetworkReply::readyRead, &loop, consume);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(30000);
    loop.exec();
    timer.stop();
    consume();
    const auto status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto next =
        reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
    const auto error = reply->error();
    reply->deleteLater();
    check(!timeout, "Pinned release request timed out", "network-timeout");
    check(!large && !writeFailure,
          "Pinned asset exceeds bound or destination write failed",
          large ? "size-exceeded" : "write-failed");
    if (status >= 400)
      throw Failure(QString::number(status), "download",
                    "GitHub request rejected");
    check(error == QNetworkReply::NoError,
          "Pinned release network request failed", QString::number(int(error)));
    if (!next.isEmpty()) {
      check(asset && (status == 301 || status == 302 || status == 303 ||
                      status == 307 || status == 308),
            "Release metadata redirect refused", "api-redirect");
      target = target.resolved(next);
      check(downloadHost(target), "Asset redirect host refused",
            "asset-redirect");
      continue;
    }
    check(status == 200, "Unexpected pinned release HTTP status",
          QString::number(status));
    return bytes;
  }
  throw Failure("redirect-limit", "download",
                "Pinned asset redirect limit reached");
}
