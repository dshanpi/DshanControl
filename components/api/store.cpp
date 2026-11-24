#include "store.h"
#include <memory>
using namespace Native;
Store::Store(const QString &path, bool edge) {
  check(QDir::isAbsolutePath(path) && !QFileInfo(path).isSymLink(),
        "Invalid database path");
  check(QDir().mkpath(QFileInfo(path).absolutePath()),
        "Cannot create database directory");
  check(sqlite3_open_v2(path.toUtf8().constData(), &db,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                            SQLITE_OPEN_FULLMUTEX,
                        nullptr) == SQLITE_OK,
        "Cannot open database");
  sqlite3_busy_timeout(db, 5000);
  exec("PRAGMA journal_mode=WAL");
  exec("PRAGMA synchronous=FULL");
  exec("PRAGMA foreign_keys=ON");
  if (edge) {
    const QStringList schema{
        "CREATE TABLE IF NOT EXISTS metadata(key TEXT PRIMARY KEY,value TEXT "
        "NOT NULL)",
        "CREATE TABLE IF NOT EXISTS devices(id TEXT PRIMARY KEY,name TEXT NOT "
        "NULL,template_id TEXT NOT NULL,channel TEXT NOT NULL,address TEXT NOT "
        "NULL,enabled INTEGER NOT NULL DEFAULT 1,config_json TEXT NOT NULL "
        "DEFAULT '{}',revision INTEGER NOT NULL DEFAULT 1,created_at INTEGER "
        "NOT NULL,updated_at INTEGER NOT NULL)",
        "CREATE TABLE IF NOT EXISTS points(id TEXT PRIMARY KEY,device_id TEXT "
        "NOT NULL,name TEXT NOT NULL,data_type TEXT NOT NULL,unit TEXT NOT "
        "NULL DEFAULT '',access TEXT NOT NULL DEFAULT 'r',address TEXT NOT "
        "NULL,scale REAL NOT NULL DEFAULT 1.0,enabled INTEGER NOT NULL DEFAULT "
        "1,FOREIGN KEY(device_id) REFERENCES devices(id) ON DELETE CASCADE)",
        "CREATE INDEX IF NOT EXISTS points_device ON points(device_id)",
        "CREATE TABLE IF NOT EXISTS audit(id INTEGER PRIMARY KEY "
        "AUTOINCREMENT,ts INTEGER NOT NULL,actor TEXT NOT NULL,action TEXT NOT "
        "NULL,target TEXT NOT NULL,result TEXT NOT NULL,detail_json TEXT NOT "
        "NULL DEFAULT '{}')",
        "CREATE INDEX IF NOT EXISTS edge_audit_ts ON audit(ts DESC)"};
    for (const auto &s : schema)
      exec(s);
    auto previous =
        query("SELECT value FROM metadata WHERE key='schema_version'");
    check(previous.isEmpty() || previous.first().toObject()["value"] == "1",
          "Unsupported database schema");
    exec("INSERT OR IGNORE INTO metadata(key,value) "
         "VALUES('schema_version','1')");
  } else {
    const QStringList schema{
        "CREATE TABLE IF NOT EXISTS alarms(id INTEGER PRIMARY KEY "
        "AUTOINCREMENT,ts INTEGER NOT NULL,severity TEXT NOT NULL,source TEXT "
        "NOT NULL,code TEXT NOT NULL,message TEXT NOT NULL,state TEXT NOT NULL "
        "DEFAULT 'active',acknowledged_at INTEGER,acknowledged_by TEXT)",
        "CREATE INDEX IF NOT EXISTS alarms_ts ON alarms(ts DESC)",
        "CREATE TABLE IF NOT EXISTS audit(id INTEGER PRIMARY KEY "
        "AUTOINCREMENT,ts INTEGER NOT NULL,action TEXT NOT NULL,target "
        "TEXT,request_json TEXT NOT NULL,result TEXT NOT NULL,detail TEXT)",
        "CREATE INDEX IF NOT EXISTS audit_ts ON audit(ts DESC)",
        "CREATE TABLE IF NOT EXISTS telemetry(bucket INTEGER PRIMARY KEY,cpu "
        "REAL,memory REAL,temperature REAL,storage REAL)"};
    for (const auto &s : schema)
      exec(s);
  }
}
Store::~Store() {
  if (db)
    sqlite3_close(db);
}
QJsonArray Store::query(const QString &sql, const QJsonArray &params, int maxRows) {
  check(maxRows > 0 && maxRows <= 50000, "Invalid database result bound");
  QMutexLocker lock(&mutex);
  sqlite3_stmt *raw = nullptr;
  check(sqlite3_prepare_v2(db, sql.toUtf8().constData(), -1, &raw, nullptr) ==
            SQLITE_OK,
        "Database statement rejected");
  std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> stmt(
      raw, sqlite3_finalize);
  check(sqlite3_bind_parameter_count(raw) == params.size(),
        "Database parameter mismatch");
  for (int i = 0; i < params.size(); ++i) {
    const auto v = params[i];
    int rc;
    if (v.isNull() || v.isUndefined())
      rc = sqlite3_bind_null(raw, i + 1);
    else if (v.isBool())
      rc = sqlite3_bind_int(raw, i + 1, v.toBool());
    else if (v.isDouble())
      rc = sqlite3_bind_double(raw, i + 1, v.toDouble());
    else {
      auto text = v.toString().toUtf8();
      rc = sqlite3_bind_text(raw, i + 1, text.constData(), text.size(),
                             SQLITE_TRANSIENT);
    }
    check(rc == SQLITE_OK, "Database bind failed");
  }
  QJsonArray rows;
  int rc;
  while ((rc = sqlite3_step(raw)) == SQLITE_ROW) {
    check(rows.size() < maxRows, "Database result too large");
    QJsonObject row;
    for (int i = 0; i < sqlite3_column_count(raw); ++i) {
      auto key = QString::fromUtf8(sqlite3_column_name(raw, i));
      QJsonValue v;
      switch (sqlite3_column_type(raw, i)) {
      case SQLITE_INTEGER:
        v = double(sqlite3_column_int64(raw, i));
        break;
      case SQLITE_FLOAT:
        v = sqlite3_column_double(raw, i);
        break;
      case SQLITE_TEXT:
        v = QString::fromUtf8(
            reinterpret_cast<const char *>(sqlite3_column_text(raw, i)),
            sqlite3_column_bytes(raw, i));
        break;
      default:
        v = QJsonValue(QJsonValue::Null);
      }
      if (key == "enabled")
        v = v.toInt() != 0;
      if (key == "config_json" || key == "detail_json") {
        key = key == "config_json" ? "config" : "detail";
        v = object(v.toString().toUtf8());
      }
      row.insert(key, v);
    }
    rows.append(row);
  }
  check(rc == SQLITE_DONE, "Database operation failed");
  return rows;
}
void Store::exec(const QString &sql, const QJsonArray &params) {
  query(sql, params);
}
