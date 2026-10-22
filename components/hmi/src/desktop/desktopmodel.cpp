#include "desktopcanvas.h"
#include <cmath>

namespace DesktopV1
{
QList<AppItem> catalog()
{
    return ApplicationRegistry::apps();
}

DisplayModel DisplayModel::example()
{
    DisplayModel m;
    m.fixture = true;
    m.online = true;
    m.networkOnline = m.wiredOnline = m.wirelessOnline = m.cloudOnline = true;
    m.clock = "14:28";
    m.date = "10月16日  星期五";
    m.footer = "Qt5 设计预览 · 数据均为示例";
    m.connection = "网络已连接";
    m.badge = "示例预览";
    m.temperature = "24.3";
    m.humidity = "52.6";
    m.collection = "采集中";
    m.mediaTitle = "RGB50 接线演示";
    m.mediaTime = "01:36 / 05:12 · 示例画面";
    m.cloudState = "自动上报已启用";
    m.cloudPeriod = "每 30 秒 · 示例";
    m.controlCloud = "自动上报 · 每 30 秒";
    m.pointCount = "12";
    m.lastSend = "14:28";
    m.busStates = QStringList{"在线", "1 告警", "在线"};
    m.alarmCount = "02";
    m.alarmState = "待确认";
    m.alarmText = "Modbus 从站响应超时";
    m.alarmTime = "14:26  ·  查看事件  ›";
    m.homeName = "书房台灯";
    m.homeStatus = "亮度 60% · 状态示例";
    m.homeCount = "4 台设备 · 打开家庭  ›";
    m.voiceTitle = "今天，有什么需要帮忙？";
    m.voiceState = "按住说话  ·  设计入口";
    m.controlDate = "2026年10月16日  星期五";
    m.wired = m.wireless = "已连接 · 状态示例";
    m.sound = "已开启";
    m.notificationCount = "2";
    return m;
}

DisplayModel DisplayModel::unavailable()
{
    DisplayModel m;
    m.clock = "--:--";
    m.date = "正在读取本机时间";
    m.footer = "Qt5 原生桌面 · 数据服务未连接";
    m.connection = "状态待获取";
    m.badge = "本地界面";
    m.temperature = m.humidity = "--";
    m.collection = "未绑定";
    m.mediaTitle = "选择本地视频";
    m.mediaTime = "尚未选择文件";
    m.cloudState = "接入服务未连接";
    m.cloudPeriod = "打开上报页查看配置";
    m.controlCloud = m.cloudPeriod;
    m.pointCount = m.lastSend = "--";
    m.busStates = QStringList{"未获取", "未获取", "未获取"};
    m.alarmCount = "--";
    m.alarmState = "待获取";
    m.alarmText = "尚未获取告警";
    m.alarmTime = "打开告警记录  ›";
    m.homeName = "家庭设备";
    m.homeStatus = "尚未连接家庭服务";
    m.homeCount = "打开家庭设备  ›";
    m.voiceTitle = "语音服务尚未连接";
    m.voiceState = "打开对话页查看状态";
    m.wired = m.wireless = "待获取 · 打开网络设置";
    m.sound = "音频能力待验证";
    m.notificationCount = "--";
    return m;
}

void DesktopCanvas::applyHmi(const QJsonObject &snapshot)
{
    if (model.fixture)
        return;
    const auto before = model;
    const int oldBrightness = brightness;
    model.online = !snapshot.isEmpty();
    model.footer = model.online ? "Qt5 原生桌面 · 本地数据已连接" : "Qt5 原生桌面 · 数据服务未连接";
    model.networkOnline = model.wiredOnline = model.wirelessOnline = false;
    model.connection = model.online ? "链路未连接" : "状态待获取";
    model.wired = model.wireless = model.online ? "未提供接口" : "待获取 · 打开网络设置";
    model.busStates = QStringList{"未配置", "未验证", "未验证"};
    model.alarmCount = model.notificationCount = "--";
    model.alarmState = "待获取";
    model.alarmText = "尚未获取告警";
    model.alarmTime = "打开告警记录  ›";
    const auto networks = snapshot["networks"].toArray();
    QStringList wired, wireless;
    for (const auto &value : networks)
    {
        const auto item = value.toObject();
        const auto kind = item["kind"].toString();
        const bool demo = item["source"] == "demo";
        const bool known = item["source"] == "real" && item["online"].isBool();
        const bool up = known && item["online"] == true;
        const auto address = item["address"].toString();
        const QString text = demo ? "演示数据" : !known ? "状态未验证"
                                   : !up ? "链路未连接"
                                   : address.isEmpty() ? "链路连接 · 地址待获取" : address;
        if (kind == "wifi")
        {
            wireless.append(text);
            model.wirelessOnline |= up;
        }
        else if (kind == "ethernet")
        {
            wired.append(text);
            model.wiredOnline |= up;
        }
        model.networkOnline |= up;
    }
    if (!wired.isEmpty()) model.wired = wired.join(" / ");
    if (!wireless.isEmpty()) model.wireless = wireless.join(" / ");
    if (model.networkOnline) model.connection = "链路已连接";
    for (int group = 0; group < 3; ++group)
    {
        bool found = false, up = false, real = false, demo = false;
        for (const auto &value : snapshot["buses"].toArray())
        {
            const auto bus = value.toObject();
            const auto name = bus["name"].toString();
            if (!((group == 0 && name.startsWith("can")) ||
                  (group == 1 && name == "RS485") || (group == 2 && name == "EtherCAT")))
                continue;
            found = true;
            real |= bus["source"] == "real" && bus["online"].isBool();
            up |= bus["source"] == "real" && bus["online"] == true;
            demo |= bus["source"] == "demo";
        }
        model.busStates[group] = !model.online ? "未获取" : !found ? "未配置" : demo ? "演示数据"
                                  : !real ? "未验证" : up ? "链路在线" : "链路未连接";
    }
    const auto active = snapshot["alarms"].toObject()["active"];
    if (active.isDouble() && std::isfinite(active.toDouble()) && active.toDouble() >= 0 &&
        active.toDouble() <= 1000000 && active.toDouble() == std::floor(active.toDouble()))
    {
        const int count = active.toInt();
        model.alarmCount = model.notificationCount = QString::number(count);
        model.alarmState = count ? "有活动告警" : "无活动告警";
        model.alarmText = count ? "打开记录查看活动告警" : "暂无活动告警";
        for (const auto &value : snapshot["alarms"].toObject()["recent"].toArray())
        {
            const auto alarm = value.toObject();
            if (alarm["state"] != "active" || alarm["message"].toString().isEmpty() ||
                (alarm["source"] == "demo" && snapshot["demo_mode"] != true &&
                 snapshot["settings"].toObject()["demo_mode"] != true))
                continue;
            model.alarmText = alarm["message"].toString();
            if (alarm["source"] == "demo") model.alarmState = "演示告警";
            const auto ts = alarm["ts"];
            if (ts.isDouble() && ts.toDouble() > 0 && ts.toDouble() <= 253402300799.0)
                model.alarmTime = QDateTime::fromSecsSinceEpoch(qint64(ts.toDouble()))
                                      .toString("HH:mm") + "  ·  查看事件  ›";
            break;
        }
    }
    // Until explicit point bindings exist, no fixture point is selected implicitly.
    const auto settings = snapshot["settings"].toObject();
    // Saved settings describe intent only. The native platform reader supplies
    // the actual driver value; stale HTTP snapshots cannot replace that value.
    if (!model.online) cancelBrightnessDrag();
    if (!model.online && !brightnessPending) brightness = -1;
    if (snapshot["demo_mode"] == true || settings["demo_mode"] == true)
        model.footer = "Qt5 原生桌面 · 后端为演示数据";
    if (!(model == before) || brightness != oldBrightness)
        update();
}

void DesktopCanvas::applyServices(const QJsonObject &snapshot)
{
    if (model.fixture)
        return;
    const auto before = model;
    const auto previousHome = homeSnapshot;
    const bool hadPoints = !availablePoints.isEmpty();
    const bool available = snapshot["service_connected"] == true;
    const auto report = snapshot["reporting"].toObject();
    pointsConnected = available;
    availablePoints = available && report["points"].toArray().size() <= 128 ? report["points"].toArray() : QJsonArray();
    pointHistory.ingest(availablePoints, pointsConnected, QDateTime::currentMSecsSinceEpoch(), pointClock.elapsed());
    const auto cloud = snapshot["cloud"].toObject();
    model.cloudOnline = available && cloud["mqtt"] == "connected";
    model.cloudState = !available ? "接入服务未连接"
                       : cloud["configured"] != true ? "设备尚未登记"
                       : report["enabled"] != true ? "自动上报未启用"
                       : !model.cloudOnline ? "上报已启用 · 云端离线" : "自动上报已启用";
    model.cloudPeriod = available
                            ? QString("每 %1 秒 · 本机配置").arg(report["period_seconds"].toInt(30))
                            : "打开上报页查看配置";
    model.controlCloud = model.cloudState;
    const auto pairing=cloud["pairing"].toObject();
    model.pairingNotice=!available || pairing["fresh"]!=true ? QString("官网归属待确认"):
        pairing["ownership"]=="bound" ? QString("官网账号已绑定"):
        pairing["ownership"]=="unbound" ? QString("官网账号尚未绑定"):QString("官网归属待确认");
    QSet<QString> good, seen, duplicate;
    for (const auto &point : availablePoints) {
        const auto p=point.toObject();const auto id=p["point_id"].toString();
        if(seen.contains(id))duplicate.insert(id);
        seen.insert(id);
        if(DesktopPointHistory::validSample(p,QDateTime::currentMSecsSinceEpoch()))good.insert(id);
    }
    good.subtract(duplicate);
    model.pointCount = available ? QString::number(good.size()) : "--";
    const auto ack=QDateTime::fromString(report["last_broker_ack"].toString(),Qt::ISODateWithMs);
    model.lastSend = available && ack.isValid() ? ack.toLocalTime().toString("HH:mm") : "--";
    const auto voice = snapshot["voice"].toObject();
    model.voiceTitle = available && voice["ready"] == true ? "语音服务已就绪" : "语音服务尚未连接";
    const QMap<QString,QString> voiceLabels={{"idle","按下说话，松开发送"},{"listening","正在聆听"},
        {"thinking","正在思考"},{"speaking","正在回答"},{"error","语音异常 · 打开查看"}};
    model.voiceState=available && voice["ready"]==true?voiceLabels.value(voice["state"].toString(),"状态待确认"):QString("打开对话页查看状态");
    model.homeStatus = available && snapshot["home"].toObject()["configured"] == true
                           ? "打开家庭页同步状态"
                           : "尚未连接家庭服务";
    const auto home = snapshot["home"].toObject();
    homeSnapshot = available ? home : QJsonObject();
    model.homeCount = available && home["fresh"] == true
                          ? QString("%1 台设备 · 打开家庭  ›").arg(home["devices"].toArray().size())
                          : "打开家庭设备  ›";
    // Bound point ages/trends must still repaint even if a provider repeats
    // its payload. Empty, unchanged service status does not redraw the desktop.
    if (!(model == before) || previousHome != homeSnapshot || hadPoints || !availablePoints.isEmpty())
        update();
}
} // namespace DesktopV1
