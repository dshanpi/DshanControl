#pragma once
#include "storage.h"
class QFile;
class DevelopmentRelease {
  qint64 releaseId = 0;
  QString credential;
  QByteArray fetch(const QString &path, qint64 maximum, bool asset,
                   QFile *destination = nullptr) const;

public:
  explicit DevelopmentRelease(const QJsonObject &configuration);
  bool enabled() const { return releaseId > 0; }
  qint64 id() const { return releaseId; }
  QJsonObject metadata() const;
  QByteArray asset(qint64 assetId, qint64 maximum,
                   QFile *destination = nullptr) const;
};
