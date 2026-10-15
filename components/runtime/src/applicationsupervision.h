#pragma once
#include "storage.h"
#include <QObject>
#include <QProcess>
#include <QTimer>

// Read-only health observation. Never starts, signals or reinstalls
// applications.
class ApplicationSupervision : public QObject {
  QJsonObject configuration, observation;
  QString updaterConfig, expectedCommit, expectedCurrent;
  QTimer interval, deadline;
  QProcess helper;
  QByteArray output, errorOutput;
  int threshold = 3, timeout = 10000;
  bool running = false, timedOut = false, oversized = false;
  bool idle();
  void probe();
  void complete(int exitCode, QProcess::ExitStatus status);
  void failed(const QString &stage, const QString &code);
  void readOutput();

public:
  explicit ApplicationSupervision(const QJsonObject &service,
                                  const QJsonObject &updater,
                                  QObject *parent = nullptr);
  ~ApplicationSupervision() override;
  QJsonObject snapshot() const {
    auto value = observation;
    value["probe_running"] = running;
    return value;
  }
};
