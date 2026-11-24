#include "api.h"
#include <QNetworkInterface>
#include <QStorageInfo>
#include <sys/statvfs.h>
using namespace Native;
namespace {
QString text(const QString &path) {
  return QString::fromUtf8(optional(path)).trimmed();
}
double round1(double value) { return std::round(value * 10) / 10; }
} // namespace
QJsonObject Api::systemInfo() {
  QMutexLocker lock(&mutex);
  QJsonValue cpu, mem, temp, storage, uptime, slot;
  auto line =
      optional("/proc/stat").split('\n').first().simplified().split(' ');
  QVector<quint64> current;
  bool valid = line.size() >= 5 && line[0] == "cpu";
  for (int i = 1; i < qMin(9, line.size()); ++i) {
    bool ok;
    auto n = line[i].toULongLong(&ok);
    valid = valid && ok;
    current << n;
  }
  if (valid && current.size() == previousCpu.size()) {
    quint64 total = 0, idle = 0;
    for (int i = 0; i < current.size(); ++i) {
      if (current[i] < previousCpu[i]) {
        valid = false;
        break;
      }
      auto delta = current[i] - previousCpu[i];
      total += delta;
      if (i == 3 || i == 4)
        idle += delta;
    }
    if (valid && total > 0)
      cpu = round1(100.0 * (total - idle) / total);
  }
  previousCpu = valid ? current : QVector<quint64>{};
  QMap<QString, quint64> memory;
  for (auto row : optional("/proc/meminfo").split('\n')) {
    auto fields = row.simplified().split(' ');
    if (fields.size() != 3 || fields[2] != "kB")
      continue;
    bool ok;
    auto value = fields[1].toULongLong(&ok);
    if (ok)
      memory[QString::fromLatin1(fields[0])] = value;
  }
  if (memory.value("MemTotal:") > 0 && memory.contains("MemAvailable:") &&
      memory["MemAvailable:"] <= memory["MemTotal:"])
    mem = round1(100.0 * (memory["MemTotal:"] - memory["MemAvailable:"]) /
                 memory["MemTotal:"]);
  const auto zones =
      QDir("/sys/class/thermal").entryList({"thermal_zone*"}, QDir::Dirs);
  for (auto zone : zones) {
    bool ok;
    auto raw = text("/sys/class/thermal/" + zone + "/temp").toLongLong(&ok);
    if (ok && raw > -20000 && raw < 150000) {
      temp = round1(raw / 1000.0);
      break;
    }
  }
  struct statvfs fs{};
  if (::statvfs("/", &fs) == 0 && fs.f_blocks > 0)
    storage = round1(100.0 * (fs.f_blocks - fs.f_bfree) / fs.f_blocks);
  bool ok;
  double up = QString::fromLatin1(optional("/proc/uptime").split(' ').first())
                  .toDouble(&ok);
  if (ok && std::isfinite(up) && up >= 0)
    uptime = std::floor(up);
  QSet<QString> rootSlots;
  for (auto token : text("/proc/cmdline").split(QRegularExpression("\\s+")))
    if (token == "rootfsA" || token == "rootfsB")
      rootSlots.insert(token);
  if (rootSlots.size() == 1)
    slot = *rootSlots.begin();
  return {{"cpu", cpu},
          {"memory", mem},
          {"temperature", temp},
          {"temperature_source", temp.isNull() ? "unavailable" : "real"},
          {"storage", storage},
          {"uptime_seconds", uptime},
          {"hostname", text("/etc/hostname")},
          {"kernel", text("/proc/sys/kernel/osrelease")},
          {"active_slot", slot}};
}
QJsonObject Api::alarmSummary(bool demo) {
  auto filter = demo ? QString{} : QString(" AND source!='demo'");
  auto since = double(QDateTime::currentSecsSinceEpoch() - 86400);
  return {{"active",
           hmi.query("SELECT COUNT(*) AS n FROM alarms WHERE state='active'" +
                     filter)
               .first()
               .toObject()["n"]},
          {"today",
           hmi.query("SELECT COUNT(*) AS n FROM alarms WHERE ts>=?" + filter,
                     {since})
               .first()
               .toObject()["n"]},
          {"recent", hmi.query("SELECT * FROM alarms WHERE ts>=?" + filter +
                                   " ORDER BY ts DESC LIMIT 3",
                               {since})}};
}
void Api::demoAlarms() {
  Transaction tx(hmi);
  if (hmi.query("SELECT COUNT(*) AS n FROM alarms WHERE source='demo'")
          .first()
          .toObject()["n"]
          .toInt() == 0) {
    const auto now = double(QDateTime::currentSecsSinceEpoch());
    hmi.exec("INSERT INTO alarms(ts,severity,source,code,message,state) "
             "VALUES(?,?,?,?,?,?)",
             {now - 120, "warning", "demo", "TEMP_HIGH", "设备07温度偏高",
              "active"});
    hmi.exec("INSERT INTO alarms(ts,severity,source,code,message,state) "
             "VALUES(?,?,?,?,?,?)",
             {now - 1680, "info", "demo", "CAN_RECOVERED", "CAN1错误帧已恢复",
              "cleared"});
    hmi.exec("INSERT INTO alarms(ts,severity,source,code,message,state) "
             "VALUES(?,?,?,?,?,?)",
             {now - 6420, "info", "demo", "CELL_RECONNECT", "4G-A网络已重连",
              "cleared"});
  }
  tx.commit();
}
QJsonObject Api::snapshot() {
  const auto system = systemInfo(), cfg = settings();
  const bool demo = cfg["demo_mode"].toBool();
  auto now = QDateTime::currentSecsSinceEpoch();
  {
    QMutexLocker lock(&mutex);
    auto bucket = now / 60 * 60;
    if (bucket != telemetryBucket) {
      Transaction tx(hmi);
      hmi.exec(
          "INSERT OR REPLACE INTO "
          "telemetry(bucket,cpu,memory,temperature,storage) VALUES(?,?,?,?,?)",
          {double(bucket), system["cpu"], system["memory"],
           system["temperature"], system["storage"]});
      hmi.exec("DELETE FROM telemetry WHERE bucket<?",
               {double(bucket - 30 * 86400)});
      tx.commit();
      telemetryBucket = bucket;
    }
  }
  QJsonArray buses, networks, sensors;
  const auto channels = manifest["channels"].toObject();
  for (auto i = channels.begin(); i != channels.end(); ++i) {
    auto c = i.value().toObject();
    auto kind = c["kind"].toString(), name = c["device"].toString();
    if (kind == "can") {
      QString state = "DOWN";
      int bitrate = 0;
      bool online = false;
      if (QFileInfo::exists("/sys/class/net/" + name)) {
        const auto r =
            run("/sbin/ip", {"-details", "-statistics", "link", "show", name},
                3000);
        auto info = QString::fromUtf8(r.output);
        auto s = QRegularExpression("can state ([A-Z-]+)").match(info),
             b = QRegularExpression("bitrate (\\d+)").match(info);
        online = info.contains("state UP");
        state = s.hasMatch() ? s.captured(1) : (online ? "UP" : "DOWN");
        if (b.hasMatch())
          bitrate = b.captured(1).toInt();
      }
      buses.append(QJsonObject{{"name", name.toUpper()},
                               {"online", online},
                               {"state", state},
                               {"bitrate", bitrate},
                               {"source", "real"}});
    } else if (kind == "ethernet" || kind == "wifi") {
      auto state = text("/sys/class/net/" + name + "/operstate");
      QString address;
      for (const auto &entry :
           QNetworkInterface::interfaceFromName(name).addressEntries())
        if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
          address = entry.ip().toString() + "/" +
                    QString::number(entry.prefixLength());
          break;
        }
      networks.append(QJsonObject{{"name", name},
                                  {"online", state == "up"},
                                  {"state", state.isEmpty() ? "未发现" : state},
                                  {"address", address},
                                  {"source", "real"},
                                  {"kind", kind}});
    }
  }
  for (auto name : QStringList{"RS485", "EtherCAT"})
    buses.append(QJsonObject{
        {"name", name},
        {"online", false},
        {"state", demo ? (name == "RS485" ? "演示 6台" : "演示 OP 4站")
                       : "未检测到从站"},
        {"load", demo ? (name == "RS485" ? 26 : 41) : 0},
        {"source", demo ? "demo" : "unavailable"}});
  if (demo) {
    double phase = now / 8.0;
    sensors = {QJsonObject{{"id", "temperature"},
                           {"name", "温度"},
                           {"value", round1(55 + 5 * std::sin(phase))},
                           {"unit", "°C"},
                           {"source", "demo"},
                           {"state", "正常"}},
               QJsonObject{{"id", "pressure"},
                           {"name", "压力"},
                           {"value", round1(38 + 3 * std::sin(phase * .7))},
                           {"unit", "kPa"},
                           {"source", "demo"},
                           {"state", "正常"}},
               QJsonObject{{"id", "flow"},
                           {"name", "流量"},
                           {"value", round1(20 + 4 * std::sin(phase * 1.3))},
                           {"unit", "L/min"},
                           {"source", "demo"},
                           {"state", "正常"}}};
  }
  QJsonObject services;
  const QMap<QString, QString> scripts{
      {"thingsboard", "/etc/init.d/S70thingsboard-gateway"},
      {"bluetooth", "/usr/bin/bt-speaker"},
      {"ntp", "/etc/init.d/S49ntp"}};
  for (auto i = scripts.begin(); i != scripts.end(); ++i) {
    bool available = QFileInfo(i.value()).isExecutable() &&
                     (i.key() != "bluetooth" ||
                      manifest["capabilities"].toArray().contains("bluetooth"));
    QJsonObject value{
        {"name", i.key()},
        {"available", available},
        {"running", QJsonValue::Null},
        {"detail", available ? "未提供可验证的运行状态接口" : "服务不可用"}};
    if (available && i.key() == "thingsboard") {
      auto r = run(i.value(), {"status"}, 3000);
      value["detail"] = QString::fromUtf8(r.output);
      if (r.code == 0 || r.code == 1)
        value["running"] = r.code == 0;
    }
    services[i.key()] = value;
  }
  const auto alarms = alarmSummary(demo);
  QJsonObject platform;
  for (auto key : QStringList{"board_id", "soc", "capabilities", "channels"})
    platform[key] = manifest[key];
  return {{"ok", true},
          {"timestamp", double(now)},
          {"system", system},
          {"platform", platform},
          {"summary",
           QJsonObject{{"status", alarms["active"].toInt() > 0
                                      ? "有告警"
                                      : (demo ? "演示" : "未验证")},
                       {"online_devices",
                        demo ? QJsonValue(12) : QJsonValue(QJsonValue::Null)},
                       {"today_alarms", alarms["today"]}}},
          {"buses", buses},
          {"networks", networks},
          {"cellular", QJsonObject{}},
          {"sensors", sensors},
          {"alarms", alarms},
          {"services", services},
          {"settings", cfg}};
}
