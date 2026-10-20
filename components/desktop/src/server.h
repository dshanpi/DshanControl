#pragma once
#include "providers.h"
#include <QLocalServer>
#include <memory>

class DesktopServer : public QLocalServer
{
  public:
    DesktopServer(const QString &path, const QJsonObject &configuration);
    ~DesktopServer() override;

  private:
    DesktopProviders providers;
    QString socketPath;
    std::unique_ptr<FileLock> ownership;
    int clients = 0;
};
