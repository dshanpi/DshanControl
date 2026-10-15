#include "storage.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <cmath>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <zlib.h>

Failure::Failure(const QString &c, const QString &s, const QString &m)
    : std::runtime_error(m.toStdString()), code(c), stage(s), detail(m) {}
void require(bool ok, const QString &m, const QString &s, const QString &c) {
  if (!ok)
    throw Failure(c, s, m);
}
QByteArray readBytes(const QString &path, qint64 limit) {
  QFile file(path);
  require(file.open(QIODevice::ReadOnly), "Cannot read required file",
          "storage", "file-unavailable");
  require(file.size() <= limit, "File exceeds bounded size", "storage",
          "file-too-large");
  const auto data = file.read(limit + 1);
  require(data.size() <= limit && file.error() == QFile::NoError,
          "Incomplete file read", "storage");
  return data;
}
namespace {
// Qt's JSON parser accepts duplicate keys. Reject them before decoding
// identities.
class JsonReader {
  const QByteArray &data;
  int pos = 0;
  void space() {
    while (pos < data.size() && QByteArray(" \t\r\n").contains(data[pos]))
      ++pos;
  }
  void take(char c) {
    space();
    require(pos < data.size() && data[pos++] == c, "Invalid JSON syntax");
  }
  QString string() {
    space();
    const int begin = pos;
    take('"');
    bool escaped = false;
    while (pos < data.size()) {
      char c = data[pos++];
      if (escaped) {
        escaped = false;
        continue;
      }
      if (c == '\\') {
        escaped = true;
        continue;
      }
      if (c == '"') {
        QJsonParseError error;
        auto doc = QJsonDocument::fromJson(
            "[" + data.mid(begin, pos - begin) + "]", &error);
        require(error.error == QJsonParseError::NoError && doc.isArray(),
                "Invalid JSON string");
        return doc.array().first().toString();
      }
    }
    throw Failure("invalid-json", "verification", "Unterminated JSON string");
  }
  QJsonValue value(int depth) {
    require(depth <= 64, "JSON nesting limit exceeded");
    space();
    require(pos < data.size(), "Incomplete JSON");
    if (data[pos] == '{') {
      ++pos;
      space();
      QJsonObject object;
      QSet<QString> keys;
      if (pos < data.size() && data[pos] == '}') {
        ++pos;
        return object;
      }
      for (;;) {
        QString key = string();
        require(!keys.contains(key), "Duplicate JSON key: " + key);
        keys.insert(key);
        take(':');
        object.insert(key, value(depth + 1));
        space();
        require(pos < data.size(), "Incomplete JSON object");
        if (data[pos] == '}') {
          ++pos;
          return object;
        }
        take(',');
      }
    }
    if (data[pos] == '[') {
      ++pos;
      space();
      QJsonArray array;
      if (pos < data.size() && data[pos] == ']') {
        ++pos;
        return array;
      }
      for (;;) {
        array.append(value(depth + 1));
        space();
        require(pos < data.size(), "Incomplete JSON array");
        if (data[pos] == ']') {
          ++pos;
          return array;
        }
        take(',');
      }
    }
    if (data[pos] == '"')
      return string();
    const int begin = pos;
    while (pos < data.size() && !QByteArray(" \t\r\n,]}").contains(data[pos]))
      ++pos;
    QJsonParseError error;
    auto doc = QJsonDocument::fromJson("[" + data.mid(begin, pos - begin) + "]",
                                       &error);
    require(error.error == QJsonParseError::NoError && doc.isArray() &&
                doc.array().size() == 1,
            "Invalid JSON value");
    return doc.array().first();
  }

public:
  explicit JsonReader(const QByteArray &input) : data(input) {}
  QJsonValue read() {
    auto result = value(0);
    space();
    require(pos == data.size(), "Trailing JSON data");
    return result;
  }
};
QByteArray gzipRead(gzFile file, int count) {
  QByteArray out(count, 0);
  int read = 0;
  while (read < count) {
    int n = gzread(file, out.data() + read, count - read);
    require(n > 0, "Truncated gzip/tar payload", "extraction");
    read += n;
  }
  return out;
}
qint64 octal(const QByteArray &field) {
  QByteArray text = field;
  while (!text.isEmpty() && (text.back() == '\0' || text.back() == ' '))
    text.chop(1);
  text = text.trimmed();
  if (text.isEmpty())
    return 0;
  require(QRegularExpression("^[0-7]+$")
              .match(QString::fromLatin1(text))
              .hasMatch(),
          "Invalid tar numeric field", "extraction");
  bool valid = false;
  auto n = text.toLongLong(&valid, 8);
  require(valid && n >= 0, "Tar size overflow", "extraction");
  return n;
}
QString tarString(const QByteArray &bytes) {
  return QString::fromUtf8(bytes.left(
      bytes.indexOf('\0') >= 0 ? bytes.indexOf('\0') : bytes.size()));
}
QString safePayload(QString name) {
  if (name.endsWith('/'))
    name.chop(1);
  auto parts = name.split('/');
  require(!name.isEmpty() && !name.startsWith('/') && !name.contains('\\') &&
              !name.contains(QChar(0)) &&
              (parts.first() == "bin" || parts.first() == "lib" ||
               parts.first() == "share"),
          "Payload outside allowed roots", "extraction");
  for (const auto &part : parts)
    require(!part.isEmpty() && part != "." && part != "..",
            "Unsafe payload path", "extraction");
  return name;
}
} // namespace
QJsonValue strictJson(const QByteArray &bytes) {
  return JsonReader(bytes).read();
}
QJsonObject readJson(const QString &path) {
  auto value = strictJson(readBytes(path, 1024 * 1024));
  require(value.isObject(), "JSON object required");
  return value.toObject();
}
void syncDirectory(const QString &path) {
  int fd = ::open(QFile::encodeName(path).constData(),
                  O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  require(fd >= 0, "Cannot open directory for durability", "storage");
  int result = ::fsync(fd);
  ::close(fd);
  require(result == 0, "Directory fsync failed", "storage");
}
void durableJson(const QString &path, const QJsonObject &value) {
  QSaveFile file(path);
  file.setDirectWriteFallback(false);
  require(file.open(QIODevice::WriteOnly), "Cannot create durable state",
          "storage");
  auto data = QJsonDocument(value).toJson();
  require(file.write(data) == data.size() && file.flush(), "State write failed",
          "storage");
  require(::fchmod(file.handle(), 0600) == 0 && ::fsync(file.handle()) == 0,
          "State fsync failed", "storage");
  require(file.commit(), "Atomic state commit failed", "storage");
  syncDirectory(QFileInfo(path).absolutePath());
}
QString hashBytes(const QByteArray &bytes) {
  return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
}
QString hashFile(const QString &path) {
  QFile file(path);
  require(file.open(QIODevice::ReadOnly), "Cannot hash payload",
          "verification");
  QCryptographicHash hash(QCryptographicHash::Sha256);
  while (!file.atEnd()) {
    auto block = file.read(65536);
    require(file.error() == QFile::NoError, "Hash read failed");
    hash.addData(block);
  }
  return hash.result().toHex();
}
bool identifier(const QString &v) {
  return QRegularExpression("^[A-Za-z0-9][A-Za-z0-9._+-]{0,100}$")
      .match(v)
      .hasMatch();
}
bool sha256(const QJsonValue &v) {
  return v.isString() &&
         QRegularExpression("^[a-f0-9]{64}$").match(v.toString()).hasMatch();
}
bool positiveInteger(const QJsonValue &v, bool zero) {
  double n = v.toDouble(-1);
  return v.isDouble() && std::isfinite(n) && n >= (zero ? 0 : 1) &&
         n <= 9007199254740991.0 && std::floor(n) == n;
}
FileLock::FileLock(const QString &path, bool exclusive) {
  fd = ::open(QFile::encodeName(path).constData(),
              O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);
  require(fd >= 0, "Cannot open resource lock", "resource-lock");
  if (::flock(fd, (exclusive ? LOCK_EX : LOCK_SH) | LOCK_NB) != 0) {
    ::close(fd);
    fd = -1;
    throw Failure("resource-busy", "resource-lock",
                  "Another task or hardware operation owns the resource");
  }
}
FileLock::~FileLock() {
  if (fd >= 0)
    ::close(fd);
}
void extractPackage(const QString &archive, const QString &directory,
                    qint64 expected) {
  gzFile file = gzopen(QFile::encodeName(archive).constData(), "rb");
  require(file, "Cannot read gzip package", "extraction");
  QSet<QString> seen;
  qint64 total = 0;
  int entries = 0;
  QMap<QString, QString> pax;
  try {
    for (;;) {
      auto header = gzipRead(file, 512);
      if (header == QByteArray(512, 0)) {
        require(gzipRead(file, 512) == QByteArray(512, 0),
                "Missing tar end marker", "extraction");
        break;
      }
      require(++entries <= 50000, "Too many archive entries", "extraction");
      qint64 checksum = 0;
      for (int i = 0; i < 512; ++i)
        checksum +=
            (i >= 148 && i < 156) ? 32 : static_cast<unsigned char>(header[i]);
      require(checksum == octal(header.mid(148, 8)), "Tar checksum rejected",
              "extraction");
      qint64 size = octal(header.mid(124, 12));
      char type = header[156];
      require(size <= expected + 65536 &&
                  (octal(header.mid(100, 8)) & 07000) == 0,
              "Invalid tar size/permissions", "extraction");
      if (type == 'x') {
        require(size <= 65536 && pax.isEmpty(),
                "Unsupported repeated PAX metadata", "extraction");
        auto metadata = gzipRead(file, static_cast<int>(size));
        int pos = 0;
        while (pos < metadata.size()) {
          int space = metadata.indexOf(' ', pos);
          require(space > pos, "Invalid PAX record", "extraction");
          bool valid;
          int length = metadata.mid(pos, space - pos).toInt(&valid);
          require(valid && length > space - pos + 2 &&
                      pos + length <= metadata.size() &&
                      metadata[pos + length - 1] == '\n',
                  "Invalid PAX length", "extraction");
          auto record = metadata.mid(space + 1, pos + length - space - 2);
          int equal = record.indexOf('=');
          require(equal > 0, "Invalid PAX field", "extraction");
          auto key = QString::fromUtf8(record.left(equal));
          require(!pax.contains(key) && (key == "path" || key == "mtime" ||
                                         key == "atime" || key == "ctime"),
                  "Unsupported PAX field", "extraction");
          pax.insert(key, QString::fromUtf8(record.mid(equal + 1)));
          pos += length;
        }
        if (size % 512)
          gzipRead(file, 512 - static_cast<int>(size % 512));
        continue;
      }
      require(type == '0' || type == '\0' || type == '5',
              "Links and special files forbidden", "extraction");
      QString name = tarString(header.left(100));
      QString prefix = tarString(header.mid(345, 155));
      if (!prefix.isEmpty())
        name = prefix + '/' + name;
      name = safePayload(pax.value("path", name));
      pax.clear();
      require(!seen.contains(name), "Duplicate archive entry", "extraction");
      seen.insert(name);
      QString target = directory + '/' + name;
      if (type == '5') {
        require(size == 0 && QDir().mkpath(target), "Invalid archive directory",
                "extraction");
        continue;
      }
      total += size;
      require(total <= expected &&
                  QDir().mkpath(QFileInfo(target).absolutePath()),
              "Expanded payload exceeds declared size", "extraction");
      int fd = ::open(QFile::encodeName(target).constData(),
                      O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC,
                      (octal(header.mid(100, 8)) & 0111) ? 0755 : 0644);
      require(fd >= 0, "Cannot stage payload file", "extraction");
      try {
        qint64 remaining = size;
        while (remaining) {
          auto bytes =
              gzipRead(file, static_cast<int>(qMin<qint64>(65536, remaining)));
          qint64 done = 0;
          while (done < bytes.size()) {
            auto n = ::write(fd, bytes.constData() + done, bytes.size() - done);
            require(n > 0, "Payload write failed", "extraction");
            done += n;
          }
          remaining -= bytes.size();
        }
        require(::fsync(fd) == 0, "Payload fsync failed", "extraction");
        ::close(fd);
        fd = -1;
      } catch (...) {
        if (fd >= 0)
          ::close(fd);
        throw;
      }
      if (size % 512)
        gzipRead(file, 512 - static_cast<int>(size % 512));
    }
    require(pax.isEmpty() && total == expected, "Expanded size mismatch",
            "extraction");
    // Consume remaining zero padding to validate the gzip trailer/CRC too.
    char tail[4096];
    int n;
    while ((n = gzread(file, tail, sizeof(tail))) > 0)
      for (int i = 0; i < n; ++i)
        require(tail[i] == 0, "Trailing nonzero archive data", "extraction");
    require(n == 0 && gzeof(file), "Gzip integrity rejected", "extraction");
    const QStringList required = {"bin/omnigate-hmi",
                                  "lib/web/app.py",
                                  "lib/web/hmi_api.py",
                                  "lib/core/omnigate_core/__init__.py",
                                  "lib/supervisor/supervisor.py",
                                  "share/web/index.html"};
    for (const auto &path : required)
      require(QFileInfo(directory + '/' + path).isFile(),
              "Incomplete application payload", "extraction");
    require(QFileInfo(directory + "/bin/omnigate-hmi").isExecutable(),
            "HMI is not executable", "extraction");
    QStringList folders{directory};
    for (int i = 0; i < folders.size(); ++i) {
      QDir dir(folders[i]);
      for (const auto &child :
           dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
        folders << child.absoluteFilePath();
    }
    for (int i = folders.size() - 1; i >= 0; --i)
      syncDirectory(folders[i]);
    int closeResult = gzclose(file);
    file = nullptr;
    require(closeResult == Z_OK, "Gzip close failed", "extraction");
  } catch (...) {
    if (file)
      gzclose(file);
    throw;
  }
}
