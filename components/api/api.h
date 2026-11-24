#pragma once
#include "http.h"
#include "store.h"
class Api {
  QMutex mutex{QMutex::Recursive};
  Store hmi, edge;
  QString settingsPath, configPath, assetsPath, platformSocket;
  QJsonObject manifest;
  QVector<quint64> previousCpu;
  qint64 telemetryBucket = -1;
  QElapsedTimer elapsed;
  struct Session {
    QString username, role, csrf;
    qint64 expires;
  };
  QMap<QString, Session> sessions;
  QList<qint64> failures;
  QJsonObject settings();
  QJsonObject configuration();
  QJsonObject systemInfo();
  QJsonObject snapshot();
  QJsonObject alarmSummary(bool demo);
  void demoAlarms();
  void audit(const QString &action, const QJsonObject &params,
             const QString &result, const QString &detail = {});
  QJsonObject action(const QString &name, const QJsonObject &params);
  Response hmiRoute(const Request &request);
  Response edgeRoute(const Request &request, const Session &session);
  Response legacyRoute(const Request &request, const Session &session);
  Response dictionaryRoute(const Request &request);
  Session authorize(const Request &request);

public:
  Api();
  Response handle(const Request &request);
};
