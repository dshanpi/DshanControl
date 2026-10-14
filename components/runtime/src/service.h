#pragma once
#include "updater.h"
#include <QLocalServer>
#include <memory>
class RuntimeService : public QLocalServer {
  QJsonObject config;
  QString taskRoot, updaterConfig;
  std::unique_ptr<FileLock> serviceOwnership;
  QJsonObject task(const QString &id) const;
  QJsonObject startTask(const QJsonObject &params);
  QJsonValue dispatch(const QJsonObject &request);

public:
  explicit RuntimeService(const QJsonObject &configuration);
  ~RuntimeService() override;
};
QJsonValue platformRequest(const QString &socket, const QString &method,
                           const QJsonObject &params);
