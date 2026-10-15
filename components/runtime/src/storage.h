#pragma once
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <stdexcept>

class Failure : public std::runtime_error {
public:
  QString code, stage, detail;
  Failure(const QString &code, const QString &stage, const QString &message);
};
void require(bool ok, const QString &message,
             const QString &stage = "verification",
             const QString &code = "rejected");
QByteArray readBytes(const QString &path, qint64 limit);
QJsonValue strictJson(const QByteArray &bytes);
QJsonObject readJson(const QString &path);
void durableJson(const QString &path, const QJsonObject &value);
void syncDirectory(const QString &path);
QString hashFile(const QString &path);
QString hashBytes(const QByteArray &bytes);
bool identifier(const QString &value);
bool sha256(const QJsonValue &value);
bool positiveInteger(const QJsonValue &value, bool zero = false);
void extractPackage(const QString &archive, const QString &directory,
                    qint64 size);
class FileLock {
  int fd = -1;

public:
  explicit FileLock(const QString &path, bool exclusive = true,
                    int inheritedDescriptor = -1);
  ~FileLock();
  FileLock(const FileLock &) = delete;
  FileLock &operator=(const FileLock &) = delete;
  int descriptor() const { return fd; }
};

int inheritedApplicationLease(const char *name);

void verifyPersistentStorage(const QString &root, const QJsonObject &contract);
