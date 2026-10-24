#pragma once
#include "storage.h"
#include <QMap>
#include <QStringList>

class ApplicationProcesses {
  QString stage, pidRoot;
  QMap<QString, QStringList> commands;
  QJsonObject configuration;
  bool receiptsRequired = false, nativeSupervisor = false, fixture = false;

public:
  explicit ApplicationProcesses(const QJsonObject &configuration,
                                const QString &stage);
  QJsonObject health(const QString &currentCommit = QString()) const;
  QJsonObject stop(const QString &role = QString()) const;
  QString pidDirectory() const { return pidRoot; }
  QMap<QString, QStringList> launchCommands() const;
  void verifyLaunchPeers(const QString &role, const QString &currentCommit) const;
};
