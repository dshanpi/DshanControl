#pragma once
#include "storage.h"
#include <QMap>
#include <QStringList>

class ApplicationProcesses {
  QString stage, pidRoot;
  QMap<QString, QStringList> commands;

public:
  explicit ApplicationProcesses(const QJsonObject &configuration,
                                const QString &stage);
  QJsonObject health() const;
  QJsonObject stop(const QString &role = QString()) const;
};
