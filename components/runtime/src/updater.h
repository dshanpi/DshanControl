#pragma once
#include "protocol.h"
#include <functional>

class NativeUpdater {
  QJsonObject config, contract;
  QString root, releases;
  QString hookFailureCode;
  void recoverInternal();
  void rollback(const QJsonObject &transaction);
  void switchTo(const QString &name);
  bool hook(const QString &operation, bool checked = true);
  bool healthy();

public:
  std::function<void(const QString &)> progress = [](const QString &) {};
  explicit NativeUpdater(const QJsonObject &configuration);
  QString current() const;
  QJsonObject state() const;
  QJsonObject systemContract() const { return contract; }
  QJsonObject installed() const {
    return readJson(current() + "/installed.json");
  }
  QString applicationRoot() const { return root; }
  QString controlLock() const;
  void recover();
  QString install(const QString &manifest, const QString &signature,
                  const QString &package);
  QJsonObject check();
  QJsonObject download(const QJsonObject &candidate, const QString &directory);
};
