#include "cloudpanel.h"
#include "desktop/desktoppoints.h"
#include <QSet>
#include <QDateTime>
#include "visuals.h"
#include <QComboBox>
#include <QFrame>
#include <QHeaderView>
#include <QJsonArray>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>

namespace
{
QLabel *label(const QString &value, const QString &name = QString())
{
    auto *w = new QLabel(value);
    w->setObjectName(name);
    w->setTextFormat(Qt::PlainText);
    w->setWordWrap(true);
    return w;
}
QFrame *panel(QVBoxLayout **layout)
{
    auto *frame = new QFrame;
    frame->setObjectName("panel");
    *layout = new PageLayout(frame);
    (*layout)->setContentsMargins(14, 10, 14, 10);
    (*layout)->setSpacing(6);
    return frame;
}
void field(QVBoxLayout *layout, const QString &name, QLabel *value)
{
    auto *row = new QHBoxLayout;
    row->addWidget(label(name, "caption"));
    row->addStretch();
    row->addWidget(value);
    layout->addLayout(row);
}
} // namespace
void CloudPanel::selectSection(int index)
{
    if (auto *tabs = findChild<SectionTabs *>("cloudTabs"))
        if (index >= 0 && index < tabs->count()) tabs->setCurrentIndex(index);
}
CloudPanel::CloudPanel(DesktopClient *service, QWidget *parent) : QWidget(parent), client(service)
{
    setObjectName("cloudPage");
    setStyleSheet("QLabel#pairingCode{font-size:38px;font-weight:700;color:#0062ff;} "
                  "QLabel#cloudTitle{font-size:25px;font-weight:700;}");
    auto *root = new PageLayout(this);
    root->setContentsMargins(14, 6, 14, 8);
    root->setSpacing(6);
    // The shared application header identifies the route.
    auto *tabs = new SectionTabs;
    tabs->setObjectName("cloudTabs");
    root->addWidget(tabs, 1);
    auto *binding = new LayoutPage;
    auto *columns = new PageRowLayout(binding);
    columns->setContentsMargins(0, 8, 0, 0);
    columns->setSpacing(12);
    QVBoxLayout *left, *right;
    columns->addWidget(panel(&left), 3);
    auto *pair = panel(&right);
    columns->addWidget(pair, 2);
    left->addWidget(label("设备身份与连接", "panelTitle"));
    left->addWidget(label("在官网设备管理中输入本机绑定码，将设备加入你的账号。", "caption"));
    identity = label("待设备登记");
    field(left, "设备编号", identity);
    connection = label("尚未配置");
    field(left, "云端连接", connection);
    ownership = label("待确认");
    field(left, "账号归属", ownership);
    left->addStretch();
    auto *steps = new QPushButton("绑定步骤");
    steps->setProperty("kind", "secondary");
    left->addWidget(steps);
    connect(steps, &QPushButton::clicked, this, [this]() {
        QMessageBox::information(this, "官网绑定",
                                 "在电脑或手机打开 "
                                 "www.100ask."
                                 "net，登录你的账号，在设备管理中输入本机显示的六位绑定码。\n\n已绑"
                                 "定的设备无需重新登记。涂鸦家庭账号需另外授权。");
    });
    right->addWidget(label("绑定步骤", "panelTitle"));
    right->addWidget(label("1  查询绑定码与当前归属", "panelTitle"));
    code = label("—  —  —  —  —  —", "pairingCode");
    code->setAlignment(Qt::AlignCenter);
    right->addWidget(code, 1);
    expiry = label("登记完成后可查询绑定状态", "caption");
    expiry->setAlignment(Qt::AlignCenter);
    right->addWidget(expiry);
    refresh = new QPushButton("查询 / 获取绑定码");
    right->addWidget(refresh);
    right->addWidget(label("2  在官网设备管理输入有效绑定码", "panelTitle"));
    right->addWidget(label("在 www.100ask.net 登录账号后完成绑定。", "caption"));
    right->addWidget(label("3  再次查询，核对账号归属", "panelTitle"));
    right->addWidget(label("只有服务返回最新归属才算确认；过期码需重新获取。", "caption"));
    connect(refresh, &QPushButton::clicked, this, [this]() { client->request("cloud.refresh"); });
    tabs->addTab(binding, "官网绑定");
    auto *reporting = new LayoutPage;
    auto *row = new PageRowLayout(reporting);
    row->setContentsMargins(0, 8, 0, 0);
    row->setSpacing(12);
    QVBoxLayout *readings, *options;
    row->addWidget(panel(&readings), 3);
    auto *settings = panel(&options);
    settings->setMinimumWidth(240);
    row->addWidget(settings, 2);
    readings->addWidget(label("点位上报", "panelTitle"));
    reportState = label("接入服务未连接", "caption");
    readings->addWidget(reportState);
    points = new DataTable(4);
    points->setObjectName("cloudPoints");
    points->setHorizontalHeaderLabels({"点位", "读数", "来源", "质量"});
    points->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    points->verticalHeader()->hide();
    points->verticalHeader()->setDefaultSectionSize(44);
    points->setMinimumHeight(100);
    static_cast<DataTable *>(points)->setEmptyText("尚未收到可上报的点位。连接接入服务后查看。");
    points->setEditTriggers(QAbstractItemView::NoEditTriggers);
    points->setSelectionBehavior(QAbstractItemView::SelectRows);
    readings->addWidget(points, 1);
    readings->addWidget(label("缺测和过期读数不填零；本地采集持续运行。", "caption"));
    options->addWidget(label("自动上报", "panelTitle"));
    enabled = new ToggleSwitch("启用上报");
    enabled->setObjectName("reportingEnabled");
    options->addWidget(enabled);
    options->addWidget(label("上报周期", "caption"));
    period = new QComboBox;
    period->setObjectName("reportingPeriod");
    period->addItem("每 30 秒", 30);
    period->addItem("每 60 秒", 60);
    period->addItem("每 5 分钟", 300);
    options->addWidget(period);
    options->addWidget(label("开关与周期修改立即提交；仅在服务可用时生效。", "caption"));
    options->addWidget(label("最近代理确认", "caption"));
    lastAck = label("--");
    options->addWidget(lastAck);
    options->addStretch();
    send = new QPushButton("立即上报一次");
    options->addWidget(send);
    connect(send, &QPushButton::clicked, this, [this]() { client->request("reporting.send"); });
    auto configure = [this]() {
        if (!loading && available)
            client->request("reporting.configure",
                            {{"enabled", enabled->isChecked()},
                             {"period_seconds", period->currentData().toInt()}});
    };
    connect(enabled, &QCheckBox::toggled, this, configure);
    connect(period, QOverload<int>::of(&QComboBox::activated), this,
            [configure](int) { configure(); });
    tabs->addTab(reporting, "数据上报");
    note = label("接入服务尚未连接。", "infoNote");
    root->addWidget(note);
    auto *operationFeedback = label("", "operationFeedback");
    operationFeedback->setStyleSheet("background:#edf5ff;color:#393939;border-left:3px solid #0f62fe;padding:8px;font-size:14px;");
    operationFeedback->hide();
    root->addWidget(operationFeedback);
    connect(client, &DesktopClient::snapshotChanged, this, &CloudPanel::apply);
    connect(client, &DesktopClient::feedback, this,
            [operationFeedback](const QString &message) {
                operationFeedback->setText("最近服务反馈：" + message);
                operationFeedback->setVisible(!message.isEmpty());
            });
    countdown.setInterval(1000);
    connect(&countdown, &QTimer::timeout, this, &CloudPanel::showRemaining);
    countdown.start();
    apply({});
}
void CloudPanel::showRemaining()
{
    if (!remaining)
        return;
    const int value = qMax(0, remaining - int(since.elapsed() / 1000));
    expiry->setText(value ? QString("有效时间  %1:%2")
                                .arg(value / 60, 2, 10, QChar('0'))
                                .arg(value % 60, 2, 10, QChar('0'))
                          : "绑定码已过期，请重新获取");
    if (!value)
    {
        code->setText("—  —  —  —  —  —");
        remaining = 0;
    }
}
void CloudPanel::apply(const QJsonObject &state)
{
    available = state["service_connected"] == true;
    const auto c = state["cloud"].toObject(), p = c["pairing"].toObject(),
               r = state["reporting"].toObject();
    identity->setText(available && !p["device_id"].toString().isEmpty() ? p["device_id"].toString()
                                                                        : "待设备登记");
    const auto mqtt = c["mqtt"].toString();
    connection->setText(!available ? "接入服务未连接"
                                   : mqtt == "connected"
                                         ? "已连接"
                                         : mqtt == "connecting"
                                               ? "正在连接"
                                               : mqtt == "offline" ? "未连接" : "尚未配置");
    const auto owner = p["ownership"].toString();
    ownership->setText(!available
                           ? "待确认"
                           : owner == "bound"
                                 ? (p["fresh"] == true ? "已绑定" : "上次已绑定 · 待查询")
                                 : owner == "unbound" && p["fresh"] == true ? "未绑定" : "待确认");
    refresh->setEnabled(available && c["configured"] == true && c["busy"] != true);
    remaining = available ? p["expires_in"].toInt() : 0;
    since.restart();
    const auto token = p["code"].toString();
    QStringList digits;
    for (const auto &digit : token)
        digits << QString(digit);
    code->setText(available && remaining > 0 && token.size() == 6
                      ? digits.join("  ")
                      : owner == "bound" && available ? "已绑定" : "—  —  —  —  —  —");
    expiry->setText(!available
                        ? "等待接入服务连接"
                        : c["busy"] == true ? "正在查询服务端归属"
                                            : owner == "bound" ? "设备身份保持不变"
                                                               : "点击查询，确认归属与绑定码");
    showRemaining();
    loading = true;
    enabled->setChecked(available && r["enabled"] == true);
    const int index = period->findData(r["period_seconds"].toInt(30));
    period->setCurrentIndex(qMax(0, index));
    loading = false;
    enabled->setEnabled(available && r["persistent"] == true);
    period->setEnabled(enabled->isEnabled());
    points->setRowCount(0);
    bool valid = false;
    QSet<QString> seen, duplicate;
    for(const auto &entry:r["points"].toArray()) {
        const auto id=entry.toObject()["point_id"].toString();
        if(seen.contains(id))duplicate.insert(id);
        seen.insert(id);
    }
    if (available && r["points"].toArray().size()<=128)
        for (const auto &entry : r["points"].toArray())
        {
            const auto item = entry.toObject();
            int row = points->rowCount();
            points->insertRow(row);
            const bool good = !duplicate.contains(item["point_id"].toString()) &&
                DesktopV1::DesktopPointHistory::validSample(item,QDateTime::currentMSecsSinceEpoch());
            valid |= good;
            const QStringList values{
                item["name"].toString(),
                good ? Visuals::numberText(item["value"], 1) + ' ' + item["unit"].toString() : "--",
                item["source"].toString(), good ? "有效" : "已过期"};
            for (int col = 0; col < values.size(); ++col)
                points->setItem(row, col, new QTableWidgetItem(values[col]));
        }
    send->setEnabled(available && mqtt == "connected" && r["inflight"] != true && valid);
    reportState->setText(!available
                             ? "接入服务未连接"
                             : r["inflight"] == true
                                   ? "等待 MQTT 代理确认"
                                   : mqtt == "connected" ? "云端代理已连接" : "云端代理未连接");
    const auto ack=QDateTime::fromString(r["last_broker_ack"].toString(),Qt::ISODateWithMs);
    lastAck->setText(available && ack.isValid()?ack.toLocalTime().toString("HH:mm:ss"):"--");
    note->setText(
        !available ? "接入服务未连接，云端操作暂不可用。"
                   : (!c["error"].toString().isEmpty() || !c["pairing_error"].toString().isEmpty())
                         ? "云端查询或连接未完成，请检查网络与设备登记。"
                         : !r["error"].toString().isEmpty()
                               ? "最近一次上报未确认，请查看连接与点位质量。"
                               : "MQTT 代理确认表示消息已接收；官网入库结果需在设备管理中核对。");
}
