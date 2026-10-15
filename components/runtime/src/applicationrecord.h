#pragma once
#include "applicationcredentials.h"
#include "storage.h"
#include <QStringList>
#include <memory>
#include <sys/stat.h>

// Holds the original file/parent descriptors until a compare-and-remove.
class PrivateProcessFile {
  int parent = -1, file = -1;
  QString leaf, phase;
  struct stat identity;
  QByteArray bytes;

public:
  PrivateProcessFile(const QString &directory, const QString &name,
                     const QString &phase, bool optional = false,
                     bool strictlyPrivate = true);
  ~PrivateProcessFile();
  PrivateProcessFile(const PrivateProcessFile &) = delete;
  PrivateProcessFile &operator=(const PrivateProcessFile &) = delete;
  bool exists() const { return file >= 0; }
  QByteArray data() const { return bytes; }
  QJsonObject json() const;
  void removeUnchanged();
};
QString applicationStartTicks(pid_t pid);
void publishPrivateProcessFile(const QString &directory, const QString &name,
                               const QByteArray &bytes);
void validateApplicationRecord(const QJsonObject &record, const QString &role,
                               const QString &phase);
QJsonObject plannedApplicationRecord(pid_t pid, const QString &role,
                                     const QStringList &argv,
                                     const ApplicationCredentials &credentials,
                                     const QString &commit);
void verifyApplicationRecord(const QJsonObject &record, pid_t pid,
                             const QString &role, const QString &phase,
                             const QString &currentCommit = QString());
