#include "api.h"
#include <QUrlQuery>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <sys/statvfs.h>
using namespace Native;
namespace {
QString json(const QJsonObject &v) {
  return QString::fromUtf8(QJsonDocument(v).toJson(QJsonDocument::Compact));
}
QJsonObject body(const Request &r, const QSet<QString> &allowed = {},
                 const QSet<QString> &required = {}, bool limited = false) {
  check(
      r.headers.value("content-type").toLower().startsWith("application/json"),
      "Content-Type must be application/json");
  auto value = object(r.body);
  if (limited)
    fields(value, allowed, required);
  return value;
}
QString random(int bytes) {
  QByteArray data(bytes, 0);
  check(RAND_bytes(reinterpret_cast<unsigned char *>(data.data()), bytes) == 1,
        "Random source unavailable");
  return QString::fromLatin1(data.toHex());
}
QString passwordHash(const QString &password, const QString &salt) {
  QByteArray p = password.toUtf8(), s = salt.toUtf8(), out(32, 0);
  check(
      PKCS5_PBKDF2_HMAC(p.constData(), p.size(),
                        reinterpret_cast<const unsigned char *>(s.constData()),
                        s.size(), 120000, EVP_sha256(), out.size(),
                        reinterpret_cast<unsigned char *>(out.data())) == 1,
      "Password hashing failed");
  return QString::fromLatin1(out.toHex());
}
bool equal(const QString &a, const QString &b) {
  auto x = a.toUtf8(), y = b.toUtf8();
  return x.size() == y.size() &&
         CRYPTO_memcmp(x.constData(), y.constData(), x.size()) == 0;
}
void statusCheck(bool ok, const QString &message, int code) {
  if (!ok)
    throw Failure(QString::number(code), "http", message);
}
QJsonObject readOr(const QString &path, const QJsonObject &fallback = {}) {
  return QFileInfo::exists(path) ? readJson(path) : fallback;
}
QString identifierValue(const QJsonValue &value) {
  check(value.isString() &&
            QRegularExpression("^[a-zA-Z0-9][a-zA-Z0-9_.:-]{0,63}$")
                .match(value.toString())
                .hasMatch(),
        "Invalid identifier");
  return value.toString();
}
QString stringValue(const QJsonValue &value, const QString &name,
                    int max = 256) {
  check(value.isString() && !value.toString().isEmpty() &&
            value.toString().size() <= max,
        name + ": invalid text");
  return value.toString();
}
QString method(const Request &r) {
  return r.method == "HEAD" ? QString("GET") : QString::fromLatin1(r.method);
}
} // namespace
Api::Api()
    : hmi(env("OMNIGATE_HMI_DB", "/data/dshancontrol/web/hmi.db"), false),
      edge(env("OMNIGATE_EDGE_DB", "/data/dshancontrol/web/edge.db"), true),
      settingsPath(
          env("OMNIGATE_HMI_SETTINGS", "/data/dshancontrol/web/hmi.json")),
      configPath(
          env("OMNIGATE_WEB_CONFIG", "/data/dshancontrol/web/config.json")),
      assetsPath(env("OMNIGATE_WEB_ASSETS", "/usr/share/dshancontrol/web")),
      platformSocket(env("DSHANCONTROL_NATIVE_PLATFORM_SOCKET",
                         "/run/dshancontrol/platform.sock")),
      manifest(readJson(env("OMNIGATE_PLATFORM_MANIFEST",
                            "/usr/share/dshancontrol/platform.json"))) {
  check(manifest["channels"].isObject() && manifest["capabilities"].isArray(),
        "Invalid platform manifest");
  elapsed.start();
  if (settings()["demo_mode"] == true)
    demoAlarms();
}
QJsonObject Api::settings() {
  QMutexLocker lock(&mutex);
  QJsonObject value{{"demo_mode", false},
                    {"brightness", 220},
                    {"refresh_ms", 1000},
                    {"page_cycle_seconds", 0}};
  auto stored = readOr(settingsPath);
  for (auto i = stored.begin(); i != stored.end(); ++i)
    value[i.key()] = i.value();
  check(value["demo_mode"].isBool(), "Invalid saved demo mode");
  return value;
}
QJsonObject Api::configuration() {
  QMutexLocker lock(&mutex);
  return readOr(configPath);
}
void Api::audit(const QString &name, const QJsonObject &params,
                const QString &result, const QString &detail) {
  QString target;
  for (auto key : QStringList{"interface", "service", "profile"})
    if (params[key].isString()) {
      target = params[key].toString();
      break;
    }
  hmi.exec("INSERT INTO audit(ts,action,target,request_json,result,detail) "
           "VALUES(?,?,?,?,?,?)",
           {double(QDateTime::currentSecsSinceEpoch()), name, target,
            json(params), result, detail.left(4000)});
}
QJsonObject Api::action(const QString &name, const QJsonObject &params) {
  if (name == "brightness") {
    fields(params, {"value"}, {"value"});
    int value = integer(params["value"], "brightness", 10, 255);
    auto response = rpc(
        platformSocket, "display.brightness.set",
        {{"channel", "display0"}, {"value", value}, {"confirmed", true}}, 5000);
    check(response["channel"] == "display0", "Backlight response mismatch");
    value = integer(response["brightness"], "brightness", 1, 255);
    QMutexLocker lock(&mutex);
    auto config = settings();
    config["brightness"] = value;
    durableJson(settingsPath, config);
    return {{"brightness", value}};
  }
  const QSet<QString> supported{
      "can_apply",          "can_receive",          "can_send",
      "canopen_nmt",        "canopen_scan",         "canopen_node",
      "canopen_pdo_read",   "canopen_pdo_transmit", "canopen_sdo_read",
      "canopen_sdo_write",  "rs485_config",         "rs485_read",
      "rs485_write",        "ethercat_scan",        "ethercat_sdo_read",
      "ethercat_sdo_write", "ethercat_cycle",       "service"};
  check(supported.contains(name), "Unsupported action on this platform");
  return rpc(platformSocket, "fieldbus.execute",
             {{"action", name}, {"params", params}, {"confirmed", true}});
}
Response Api::hmiRoute(const Request &r) {
  auto path = r.url.path(), verb = method(r);
  QUrlQuery query(r.url);
  if (path == "/api/hmi/v1/snapshot" && verb == "GET")
    return snapshot();
  if (path == "/api/hmi/v1/history" && verb == "GET") {
    auto metric = query.hasQueryItem("metric") ? query.queryItemValue("metric")
                                               : QString("temperature");
    check(
        QStringList{"cpu", "memory", "temperature", "storage"}.contains(metric),
        "Invalid history metric");
    int hours = integer(query.hasQueryItem("hours")
                            ? QJsonValue(query.queryItemValue("hours"))
                            : QJsonValue(24),
                        "hours", 1, 720);
    return QJsonObject{
        {"ok", true},
        {"metric", metric},
        {"points",
         hmi.query(
             "SELECT bucket," + metric +
                 " AS value FROM telemetry WHERE bucket>=? ORDER BY bucket",
             {double(QDateTime::currentSecsSinceEpoch() - hours * 3600)}, 43201)}};
  }
  if (path == "/api/hmi/v1/alarms" && verb == "GET") {
    auto limit = integer(query.hasQueryItem("limit")
                             ? QJsonValue(query.queryItemValue("limit"))
                             : QJsonValue(100),
                         "limit", 1, 500);
    return QJsonObject{
        {"ok", true},
        {"alarms", hmi.query("SELECT * FROM alarms" +
                                 QString(settings()["demo_mode"].toBool()
                                             ? ""
                                             : " WHERE source!='demo'") +
                                 " ORDER BY ts DESC LIMIT ?",
                             {double(limit)})}};
  }
  auto ack =
      QRegularExpression("^/api/hmi/v1/alarms/([0-9]+)/ack$").match(path);
  if (ack.hasMatch() && verb == "POST") {
    check(r.body.size() <= 4096, "Acknowledgement request too large");
    if (!r.body.isEmpty())
      check(object(r.body).isEmpty(), "Acknowledgement takes no parameters");
    auto id = integer(ack.captured(1), "alarm id", 1, 9007199254740991LL);
    bool demo = settings()["demo_mode"].toBool();
    Transaction tx(hmi);
    auto rows =
        hmi.query("SELECT state,source FROM alarms WHERE id=?", {double(id)});
    statusCheck(!rows.isEmpty() &&
                    (demo || rows[0].toObject()["source"] != "demo"),
                "Alarm not found or not visible", 404);
    auto state = rows[0].toObject()["state"].toString();
    if (state == "acknowledged") {
      tx.commit();
      return QJsonObject{
          {"ok", true}, {"id", double(id)}, {"already_acknowledged", true}};
    }
    statusCheck(state == "active",
                "Alarm state does not permit acknowledgement", 409);
    auto now = double(QDateTime::currentSecsSinceEpoch());
    hmi.exec("UPDATE alarms SET "
             "state='acknowledged',acknowledged_at=?,acknowledged_by='local-"
             "hmi' WHERE id=? AND state='active'",
             {now, double(id)});
    hmi.exec("INSERT INTO audit(ts,action,target,request_json,result,detail) "
             "VALUES(?,?,?,?,?,?)",
             {now, "alarm_ack", QString::number(id), json({{"id", double(id)}}),
              "success", "local-hmi"});
    tx.commit();
    return QJsonObject{{"ok", true}, {"id", double(id)}};
  }
  if (path == "/api/hmi/v1/settings") {
    if (verb == "GET")
      return QJsonObject{{"ok", true}, {"settings", settings()}};
    statusCheck(verb == "PUT", "Method not allowed", 405);
    auto p =
        body(r, {"demo_mode", "brightness", "refresh_ms", "page_cycle_seconds"},
             {}, true);
    QMutexLocker lock(&mutex);
    auto value = settings();
    if (p.contains("demo_mode")) {
      check(p["demo_mode"].isBool(), "demo_mode must be boolean");
      value["demo_mode"] = p["demo_mode"];
    }
    if (p.contains("brightness"))
      value["brightness"] =
          int(integer(p["brightness"], "brightness", 10, 255));
    if (p.contains("refresh_ms"))
      value["refresh_ms"] =
          int(integer(p["refresh_ms"], "refresh_ms", 500, 10000));
    if (p.contains("page_cycle_seconds"))
      value["page_cycle_seconds"] =
          int(integer(p["page_cycle_seconds"], "page_cycle_seconds", 0, 300));
    durableJson(settingsPath, value);
    if (value["demo_mode"] == true)
      demoAlarms();
    return QJsonObject{{"ok", true}, {"settings", value}};
  }
  if (path == "/api/hmi/v1/actions" && verb == "POST") {
    auto p = body(r, {"action", "params", "confirmed"},
                  {"action", "params", "confirmed"}, true);
    check(p["action"].isString() &&
              !p["action"].toString().trimmed().isEmpty() &&
              p["params"].isObject() && p["confirmed"] == true,
          "Confirmed action and object parameters required");
    auto name = p["action"].toString();
    auto params = p["params"].toObject();
    try {
      auto result = action(name, params);
      audit(name, params, "success", json(result));
      return QJsonObject{{"ok", true}, {"action", name}, {"result", result}};
    } catch (const Failure &e) {
      audit(name, params, "failed", e.detail);
      throw;
    }
  }
  if (path == "/api/hmi/v1/audit" && verb == "GET")
    return QJsonObject{
        {"ok", true},
        {"audit", hmi.query("SELECT * FROM audit ORDER BY ts DESC LIMIT 200")}};
  return Response({{"ok", false}, {"error", "Unknown HMI route or method"}},
                  404);
}
Api::Session Api::authorize(const Request &r) {
  QMutexLocker lock(&mutex);
  QString key;
  for (auto cookie : r.headers.value("cookie").split(';')) {
    cookie = cookie.trimmed();
    if (cookie.startsWith("dshan_session="))
      key = QString::fromLatin1(cookie.mid(14));
  }
  for (auto i = sessions.begin(); i != sessions.end();)
    if (i->expires < elapsed.elapsed())
      i = sessions.erase(i);
    else
      ++i;
  statusCheck(!key.isEmpty() && sessions.contains(key), "请先登录", 401);
  auto session = sessions[key];
  if (method(r) != "GET") {
    statusCheck(session.role != "viewer", "当前角色只有只读权限", 403);
    statusCheck(equal(QString::fromLatin1(r.headers.value("x-csrf-token")),
                      session.csrf),
                "CSRF 校验失败", 403);
  }
  return session;
}
Response Api::handle(const Request &r) {
  auto path = r.url.path(), verb = method(r);
  if (path.startsWith("/api/hmi/v1/"))
    return hmiRoute(r);
  if (path == "/api/health" && verb == "GET")
    return QJsonObject{
        {"ok", true},
        {"service", "omnigate-web"},
        {"backend", "native-cpp"},
        {"application_commit",
         qEnvironmentVariable("DSHANCONTROL_APPLICATION_COMMIT")}};
  if ((path == "/" || path.startsWith("/assets/")) && verb == "GET") {
    const auto relative = path == "/" ? QString("index.html") : path.mid(1);
    check(!relative.split('/').contains(".."), "Invalid asset path");
    auto base = QFileInfo(assetsPath).canonicalFilePath(),
         file = QFileInfo(assetsPath + "/" + relative).canonicalFilePath();
    statusCheck(!base.isEmpty() && file.startsWith(base + "/") &&
                    QFileInfo(file).isFile(),
                "Asset not found", 404);
    Response result;
    result.body = readBytes(file, 16 * 1024 * 1024);
    auto suffix = QFileInfo(file).suffix();
    result.type = suffix == "html"  ? "text/html; charset=utf-8"
                  : suffix == "js"  ? "application/javascript"
                  : suffix == "css" ? "text/css"
                  : suffix == "png" ? "image/png"
                  : suffix == "svg" ? "image/svg+xml"
                                    : "application/octet-stream";
    return result;
  }
  if (path == "/api/login" && verb == "POST") {
    check(r.body.size() <= 4096, "Login request too large");
    auto p = body(r);
    check(p["username"].isString() && p["password"].isString() &&
              p["password"].toString().size() <= 1024,
          "Invalid login fields");
    QMutexLocker lock(&mutex);
    while (!failures.isEmpty() &&
           elapsed.elapsed() - failures.first() >= 300000)
      failures.removeFirst();
    statusCheck(failures.size() < 8, "登录失败次数过多，请稍后再试", 429);
    auto auth = configuration()["auth"].toObject();
    auto supplied =
        passwordHash(p["password"].toString(), auth["salt"].toString());
    if (p["username"] != auth["username"] ||
        !auth["password_hash"].isString() ||
        !equal(supplied, auth["password_hash"].toString())) {
      failures.append(elapsed.elapsed());
      return Response({{"error", "用户名或密码错误"}}, 401);
    }
    for (auto i = sessions.begin(); i != sessions.end();)
      if (i->expires < elapsed.elapsed())
        i = sessions.erase(i);
      else
        ++i;
    statusCheck(sessions.size() < 128, "Session limit reached", 429);
    failures.clear();
    auto token = random(32), csrf = random(24);
    sessions[token] = {auth["username"].toString(),
                       auth.value("role").toString("administrator"), csrf,
                       elapsed.elapsed() + 1800000};
    Response result({{"ok", true},
                     {"csrf", csrf},
                     {"must_change", auth["must_change"].toBool()}});
    result.headers["Set-Cookie"] =
        "dshan_session=" + token.toLatin1() +
        "; HttpOnly; SameSite=Strict; Path=/; Max-Age=1800";
    return result;
  }
  auto session = authorize(r);
  if (path == "/api/logout" && verb == "POST") {
    QMutexLocker lock(&mutex);
    for (auto cookie : r.headers.value("cookie").split(';')) {
      cookie = cookie.trimmed();
      if (cookie.startsWith("dshan_session="))
        sessions.remove(QString::fromLatin1(cookie.mid(14)));
    }
    Response result({{"ok", true}});
    result.headers["Set-Cookie"] =
        "dshan_session=; Max-Age=0; HttpOnly; SameSite=Strict; Path=/";
    return result;
  }
  if (path == "/api/password" && verb == "POST") {
    auto p = body(r);
    auto password = stringValue(p["password"], "password", 1024);
    check(password.size() >= 12, "新密码至少 12 位");
    QMutexLocker lock(&mutex);
    auto cfg = configuration(), auth = cfg["auth"].toObject();
    auto salt = random(16);
    auth["salt"] = salt;
    auth["password_hash"] = passwordHash(password, salt);
    auth["must_change"] = false;
    cfg["auth"] = auth;
    durableJson(configPath, cfg);
    sessions.clear();
    return QJsonObject{{"ok", true}};
  }
  if (path.startsWith("/api/v1/"))
    return edgeRoute(r, session);
  return legacyRoute(r, session);
}
Response Api::edgeRoute(const Request &r, const Session &session) {
  auto path = r.url.path(), verb = method(r);
  QUrlQuery q(r.url);
  if (path == "/api/v1/platform" && verb == "GET") {
    auto publicManifest = manifest;
    publicManifest.remove("manufacturing");
    return QJsonObject{{"ok", true}, {"platform", publicManifest}};
  }
  if (path == "/api/v1/channels" && verb == "GET") {
    QJsonArray values;
    auto channels = manifest["channels"].toObject();
    for (auto i = channels.begin(); i != channels.end(); ++i) {
      auto c = i.value().toObject();
      c["name"] = i.key();
      values.append(c);
    }
    return QJsonObject{{"ok", true}, {"channels", values}};
  }
  if (path == "/api/v1/system" && verb == "GET") {
    auto info = systemInfo();
    struct statvfs fs{};
    QJsonObject disk;
    if (::statvfs("/", &fs) == 0)
      disk = {{"total", double(fs.f_blocks) * fs.f_frsize},
              {"used", double(fs.f_blocks - fs.f_bfree) * fs.f_frsize},
              {"free", double(fs.f_bavail) * fs.f_frsize}};
    return QJsonObject{
        {"ok", true},
        {"system",
         QJsonObject{{"time", double(QDateTime::currentSecsSinceEpoch())},
                     {"uptime_seconds", info["uptime_seconds"]},
                     {"storage", disk},
                     {"release", info["kernel"]}}}};
  }
  if (path == "/api/v1/audit" && verb == "GET")
    return QJsonObject{
        {"ok", true},
        {"audit",
         edge.query("SELECT * FROM audit ORDER BY ts DESC LIMIT ?",
                    {double(integer(q.hasQueryItem("limit")
                                        ? QJsonValue(q.queryItemValue("limit"))
                                        : QJsonValue(200),
                                    "limit", 1, 1000))})}};
  auto match =
      QRegularExpression("^/api/v1/(devices|points)(?:/([^/]+))?$").match(path);
  statusCheck(match.hasMatch(), "Unknown management route", 404);
  auto table = match.captured(1), id = match.captured(2);
  bool devices = table == "devices";
  if (verb == "GET") {
    QJsonArray values;
    if (id.isEmpty())
      values =
          !devices && q.hasQueryItem("device_id")
              ? edge.query("SELECT * FROM points WHERE device_id=? ORDER BY id",
                           {q.queryItemValue("device_id")})
              : edge.query("SELECT * FROM " + table + " ORDER BY id");
    else {
      identifierValue(id);
      values = edge.query("SELECT * FROM " + table + " WHERE id=?", {id});
      statusCheck(!values.isEmpty(), "Record not found", 404);
    }
    return QJsonObject{{"ok", true},
                       {id.isEmpty()
                            ? table
                            : (devices ? QString("device") : QString("point")),
                        id.isEmpty() ? QJsonValue(values) : values.first()}};
  }
  check(!id.isEmpty(), "Record ID required");
  identifierValue(id);
  auto recordAudit = [&](const QString &op, const QJsonObject &detail) {
    edge.exec("INSERT INTO audit(ts,actor,action,target,result,detail_json) "
              "VALUES(?,?,?,?,?,?)",
              {double(QDateTime::currentSecsSinceEpoch()), session.username, op,
               id, "success", json(detail)});
  };
  if (verb == "DELETE") {
    Transaction tx(edge);
    auto rows = edge.query("SELECT * FROM " + table + " WHERE id=?", {id});
    statusCheck(!rows.isEmpty(), "Record not found", 404);
    edge.exec("DELETE FROM " + table + " WHERE id=?", {id});
    recordAudit(
        devices ? "device.delete" : "point.delete",
        devices ? QJsonObject{{"revision", rows[0].toObject()["revision"]}}
                : QJsonObject{{"device_id", rows[0].toObject()["device_id"]}});
    tx.commit();
    return QJsonObject{{"ok", true}, {"id", id}};
  }
  statusCheck(verb == "PUT", "Method not allowed", 405);
  auto p = body(r);
  check(!p.contains("id") || p["id"] == id, "Record ID does not match URL");
  auto name = stringValue(p["name"], "name", 128),
       address = stringValue(p["address"], "address", 256);
  bool enabled = boolean(p, "enabled", true);
  Transaction tx(edge);
  if (devices) {
    auto tpl = identifierValue(p["template_id"]),
         channel = stringValue(p["channel"], "channel", 64);
    check(manifest["channels"].toObject().contains(channel),
          "Unknown platform channel");
    check(!p.contains("config") || p["config"].isObject(),
          "Device config must be an object");
    auto rows = edge.query("SELECT revision FROM devices WHERE id=?", {id});
    int revision =
        rows.isEmpty() ? 1 : rows[0].toObject()["revision"].toInt() + 1;
    auto now = double(QDateTime::currentSecsSinceEpoch());
    edge.exec("INSERT INTO "
              "devices(id,name,template_id,channel,address,enabled,config_json,"
              "revision,created_at,updated_at) VALUES(?,?,?,?,?,?,?,?,?,?) ON "
              "CONFLICT(id) DO UPDATE SET "
              "name=excluded.name,template_id=excluded.template_id,channel="
              "excluded.channel,address=excluded.address,enabled=excluded."
              "enabled,config_json=excluded.config_json,revision=excluded."
              "revision,updated_at=excluded.updated_at",
              {id, name, tpl, channel, address, enabled,
               json(p["config"].toObject()), revision, now, now});
    recordAudit("device.put", {{"revision", revision}});
    tx.commit();
    return QJsonObject{{"ok", true}, {"id", id}, {"revision", revision}};
  }
  auto dev = identifierValue(p["device_id"]),
       type = stringValue(p["data_type"], "data_type", 16),
       access = stringValue(p["access"], "access", 2);
  check(QStringList{"bool", "int8", "uint8", "int16", "uint16", "int32",
                    "uint32", "int64", "uint64", "float", "double", "string",
                    "bytes"}
            .contains(type),
        "Unsupported point data type");
  check(QStringList{"r", "w", "rw"}.contains(access), "Invalid point access");
  auto unit = p.value("unit").toString();
  check(unit.size() <= 32, "Point unit too long");
  auto scale = p.contains("scale") ? p["scale"] : QJsonValue(1);
  check(scale.isDouble() && std::isfinite(scale.toDouble()),
        "Point scale must be finite numeric");
  check(!edge.query("SELECT id FROM devices WHERE id=?", {dev}).isEmpty(),
        "Point device does not exist");
  edge.exec(
      "INSERT INTO "
      "points(id,device_id,name,data_type,unit,access,address,scale,enabled) "
      "VALUES(?,?,?,?,?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET "
      "device_id=excluded.device_id,name=excluded.name,data_type=excluded.data_"
      "type,unit=excluded.unit,access=excluded.access,address=excluded.address,"
      "scale=excluded.scale,enabled=excluded.enabled",
      {id, dev, name, type, unit, access, address, scale, enabled});
  recordAudit("point.put", {{"device_id", dev}});
  tx.commit();
  return QJsonObject{{"ok", true}, {"id", id}};
}
Response Api::legacyRoute(const Request &r, const Session &) {
  const auto path = r.url.path(), verb = method(r);
  if ((path == "/api/canopen/eds" && verb == "POST") ||
      path.startsWith("/api/canopen/pdo/"))
    return dictionaryRoute(r);
  if (path == "/api/status" && verb == "GET") {
    auto system = systemInfo(), cfg = configuration();
    return QJsonObject{
        {"hostname", system["hostname"]},
        {"uptime",
         QString::number(system["uptime_seconds"].toDouble()) + " seconds"},
        {"time", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {"addresses", QString::fromUtf8(
                          run("/sbin/ip", {"-brief", "address"}, 3000).output)},
        {"services",
         QJsonObject{
             {"web", QJsonObject{{"running", true}, {"detail", "native-cpp"}}},
             {"thingsboard",
              QJsonObject{{"running", false},
                          {"available", false},
                          {"detail", "not configured on this board"}}}}},
        {"auth",
         QJsonObject{{"must_change",
                      cfg["auth"].toObject()["must_change"].toBool(true)}}}};
  }
  if (path == "/api/config/can" && verb == "GET") {
    QJsonObject state;
    auto channels = manifest["channels"].toObject();
    for (auto i = channels.begin(); i != channels.end(); ++i) {
      auto c = i.value().toObject();
      if (c["kind"] == "can")
        state[c["device"].toString()] = QString::fromUtf8(
            run("/sbin/ip",
                {"-details", "link", "show", c["device"].toString()}, 3000)
                .output);
    }
    return QJsonObject{{"config", configuration()["can"].toObject()},
                       {"state", state}};
  }
  if (path.startsWith("/api/config/can/") && verb == "PUT") {
    auto p = body(r);
    p["interface"] = path.mid(QString("/api/config/can/").size());
    auto result = action("can_apply", p);
    QMutexLocker lock(&mutex);
    auto cfg = configuration(), can = cfg["can"].toObject();
    can[p["interface"].toString()] = p;
    cfg["can"] = can;
    durableJson(configPath, cfg);
    return QJsonObject{{"ok", true}, {"detail", json(result)}};
  }
  if (path == "/api/config/rs485") {
    if (verb == "GET")
      return configuration()["rs485"].toObject();
    statusCheck(verb == "PUT", "Method not allowed", 405);
    auto p = body(r);
    check(!p.contains("bytesize") || p["bytesize"] == 8,
          "Native RTU uses eight data bits");
    QJsonObject request{
        {"port", p["port"]},
        {"baudrate", p["baudrate"]},
        {"unit_id", p.contains("unitId") ? p["unitId"] : QJsonValue(1)},
        {"poll_ms",
         p.contains("pollPeriod") ? p["pollPeriod"] : QJsonValue(1000)}};
    for (auto key : QStringList{"parity", "stopbits"})
      if (p.contains(key))
        request[key] = p[key];
    auto result = action("rs485_config", request);
    QMutexLocker lock(&mutex);
    auto cfg = configuration();
    cfg["rs485"] = p;
    durableJson(configPath, cfg);
    return QJsonObject{{"ok", true}, {"config", p}, {"result", result}};
  }
  if (path.startsWith("/api/ethercat/") && verb == "POST") {
    auto operation =
        path.mid(QString("/api/ethercat/").size()).replace('/', '_');
    check(QStringList{"scan", "sdo_read", "sdo_write", "cycle"}.contains(
              operation),
          "Unknown EtherCAT operation");
    auto p = r.body.isEmpty() ? QJsonObject{} : body(r);
    if (p.contains("max_bytes")) {
      integer(p.take("max_bytes"), "max_bytes", 1, 4096);
    }
    if (operation == "sdo_write") {
      auto text = p["value"].toString();
      text.remove(QRegularExpression(
          "0x|[\\s:-]", QRegularExpression::CaseInsensitiveOption));
      check(QRegularExpression("^(?:[0-9a-fA-F]{2}){1,32}$")
                .match(text)
                .hasMatch(),
            "Expected 1..32 hex bytes");
      p["value"] =
          QString::fromLatin1(QByteArray::fromHex(text.toLatin1()).toHex(' '));
    }
    auto result = action("ethercat_" + operation, p);
    result["returncode"] = 0;
    return result;
  }
  if ((path == "/api/canopen/scan" || path == "/api/canopen/node") &&
      verb == "POST")
    return action("canopen_" + path.section('/', -1), body(r));
  if (path == "/api/canopen/nmt" && verb == "POST") {
    auto p = body(r);
    p["state"] = p["state"].toString().toUpper();
    if (!p.contains("node_id"))
      p["node_id"] = 0;
    auto result = action("canopen_nmt", p);
    result["ok"] = true;
    return result;
  }
  if (path == "/api/canopen/sdo/read" && verb == "POST") {
    auto result = action("canopen_sdo_read", body(r));
    auto bytes = QByteArray::fromHex(result["data"].toString().toLatin1());
    QJsonValue u, s;
    if (bytes.size() <= 6) {
      quint64 v = 0;
      for (int i = 0; i < bytes.size(); ++i)
        v |= quint64(quint8(bytes[i])) << (i * 8);
      u = double(v);
      s = double(!bytes.isEmpty() && (quint8(bytes.back()) & 128)
                     ? qint64(v) - (qint64(1) << (bytes.size() * 8))
                     : qint64(v));
    }
    return QJsonObject{{"ok", true},
                       {"hex", QString::fromLatin1(bytes.toHex(' '))},
                       {"length", bytes.size()},
                       {"unsigned", u},
                       {"signed", s},
                       {"text", QString::fromUtf8(bytes)}};
  }
  if (path == "/api/canopen/sdo/write" && verb == "POST") {
    auto p = body(r);
    auto encoding = p.take("encoding").toString("hex");
    bool signedValue = boolean(p, "signed");
    p.remove("signed");
    QByteArray bytes;
    if (encoding == "hex") {
      auto raw = p["value"].toString();
      raw.remove("0x");
      raw.remove(' ');
      check(
          raw.size() <= 8192 &&
              QRegularExpression("^(?:[0-9A-Fa-f]{2})+$").match(raw).hasMatch(),
          "Expected 1..4096 SDO bytes");
      bytes = QByteArray::fromHex(raw.toLatin1());
    } else if (encoding == "text") {
      check(p["value"].isString(), "SDO text required");
      bytes = p["value"].toString().toUtf8();
    } else {
      check(encoding == "integer", "Unknown SDO encoding");
      int size =
          integer(p.contains("size") ? p["size"] : QJsonValue(4), "size", 1, 8);
      QString raw;
      if (p["value"].isString())
        raw = p["value"].toString();
      else
        raw = QString::number(integer(p["value"], "value", -9007199254740991LL,
                                      9007199254740991LL));
      check(QRegularExpression("^-?(?:0[xX][0-9A-Fa-f]+|[0-9]+)$")
                .match(raw)
                .hasMatch(),
            "Invalid integer text");
      bool ok = false;
      quint64 n = 0;
      int base = raw.contains('x', Qt::CaseInsensitive) ? 16 : 10;
      if (signedValue) {
        auto v = raw.toLongLong(&ok, base);
        check(ok && (size == 8 || (v >= -(qint64(1) << (size * 8 - 1)) &&
                                   v < (qint64(1) << (size * 8 - 1)))),
              "Signed SDO integer out of range");
        n = quint64(v);
      } else {
        n = raw.toULongLong(&ok, base);
        check(ok && !raw.startsWith('-') &&
                  (size == 8 || n < (quint64(1) << (size * 8))),
              "Unsigned SDO integer out of range");
      }
      for (int i = 0; i < size; ++i)
        bytes += char((n >> (i * 8)) & 255);
    }
    check(bytes.size() >= 1 && bytes.size() <= 4096,
          "SDO payload must contain 1..4096 bytes");
    p.remove("value");
    p.remove("size");
    p["data"] = QString::fromLatin1(bytes.toHex());
    auto result = action("canopen_sdo_write", p);
    return QJsonObject{{"ok", true},
                       {"written", bytes.size()},
                       {"hex", QString::fromLatin1(bytes.toHex(' '))},
                       {"device_acknowledged", result["device_acknowledged"]}};
  }
  if (path == "/api/canopen/eds" && verb == "GET") {
    auto directory = env("OMNIGATE_EDS_DIR", "/data/dshancontrol/web/eds");
    QJsonArray files;
    for (auto name :
         QDir(directory).entryList({"*.eds", "*.dcf"}, QDir::Files, QDir::Name))
      files.append(name);
    return QJsonObject{
        {"files", files},
        {"nodes", configuration()["canopen"].toObject()["nodes"].toObject()}};
  }
  if (path == "/api/ec20/profiles" && verb == "GET")
    return QJsonObject{{"profiles", QJsonObject{}}, {"available", false}};
  auto service =
      QRegularExpression("^/api/services/([^/]+)/([^/]+)$").match(path);
  if (service.hasMatch() && verb == "POST") {
    auto result = action("service", {{"service", service.captured(1)},
                                     {"operation", service.captured(2)}});
    result["ok"] = true;
    return result;
  }
  if (path == "/api/ec20/status" && verb == "GET")
    return QJsonObject{{"available", false}, {"profiles", QJsonObject{}}};
  if (path.startsWith("/api/logs/") && verb == "GET") {
    auto name = path.mid(QString("/api/logs/").size());
    const QMap<QString, QString> files{
        {"web", env("OMNIGATE_WEB_LOG", "/run/dshancontrol/web/web.log")},
        {"thingsboard", "/var/log/thingsboard-gateway.log"},
        {"messages", "/var/log/messages"}};
    check(files.contains(name), "Unsupported log");
    QUrlQuery q(r.url);
    int lines =
        integer(q.hasQueryItem("lines") ? QJsonValue(q.queryItemValue("lines"))
                                        : QJsonValue(200),
                "lines", 10, 1000);
    QFile file(files[name]);
    if (!file.open(QIODevice::ReadOnly))
      return QJsonObject{{"output", ""}, {"missing", true}};
    file.seek(qMax(qint64(0), file.size() - 32768));
    auto text = file.read(32768);
    auto rows = text.split('\n');
    while (rows.size() > lines)
      rows.removeFirst();
    return QJsonObject{{"output", QString::fromUtf8(rows.join('\n'))}};
  }
  return Response({{"ok", false},
                   {"error", "Operation is not configured for this board"},
                   {"code", "not-supported"}},
                  501);
}
