#pragma once
#include "../native/common.h"
#include <QMutex>
#include <sqlite3.h>
class Store {
  sqlite3 *db = nullptr;

public:
  QMutex mutex{QMutex::Recursive};
  explicit Store(const QString &path, bool edge);
  ~Store();
  QJsonArray query(const QString &sql, const QJsonArray &params = {}, int maxRows = 10000);
  void exec(const QString &sql, const QJsonArray &params = {});
  Store(const Store &) = delete;
  Store &operator=(const Store &) = delete;
};
struct Transaction {
  Store &store;
  QMutexLocker lock;
  bool committed = false;
  explicit Transaction(Store &s) : store(s), lock(&s.mutex) {
    store.exec("BEGIN IMMEDIATE");
  }
  void commit() {
    store.exec("COMMIT");
    committed = true;
  }
  ~Transaction() {
    if (!committed)
      try {
        store.exec("ROLLBACK");
      } catch (...) {
      }
  }
};
