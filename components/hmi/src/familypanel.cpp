#include "familypanel.h"
#include "visuals.h"
#include <QComboBox>
#include <QApplication>
#include <QEvent>
#include <QFrame>
#include <QHeaderView>
#include <QHideEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTableWidget>
#include <QUuid>
#include <cmath>

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
    (*layout)->setContentsMargins(14, 12, 14, 12);
    (*layout)->setSpacing(8);
    return frame;
}
QJsonObject device(const QJsonObject &state, const QString &id)
{
    for (const auto &item : state["home"].toObject()["devices"].toArray())
        if (item.toObject()["id"] == id)
            return item.toObject();
    return {};
}
QJsonValue reportedValue(const QJsonObject &state, const QString &id, const QString &code)
{
    for (const auto &entry : device(state, id)["status"].toArray())
        if (entry.toObject()["code"] == code)
            return entry.toObject()["value"];
    return QJsonValue(QJsonValue::Undefined);
}
QString valueText(const QJsonValue &value)
{
    if (value.isBool())
        return value.toBool() ? "开启" : "关闭";
    if (value.isDouble())
        return Visuals::numberText(value, 0);
    return value.isString() ? value.toString() : "尚未上报";
}
} // namespace
void FamilyPanel::selectSection(int index)
{
    if (auto *tabs = findChild<SectionTabs *>("familyTabs"))
        if (index >= 0 && index < tabs->count()) tabs->setCurrentIndex(index);
}
FamilyPanel::FamilyPanel(DesktopClient *service, QWidget *parent) : QWidget(parent), client(service)
{
    auto *root = new PageLayout(this);
    root->setContentsMargins(14, 6, 14, 8);
    root->setSpacing(6);
    auto *title = label("家庭与对话", "sectionTitle");
    title->setStyleSheet("font-size:25px;");
    root->addWidget(title);
    auto *tabs = new SectionTabs;
    tabs->setObjectName("familyTabs");
    root->addWidget(tabs, 1);
    auto *voicePage = new LayoutPage;
    auto *columns = new PageRowLayout(voicePage);
    columns->setContentsMargins(0, 8, 0, 0);
    columns->setSpacing(12);
    QVBoxLayout *left, *right;
    columns->addWidget(panel(&left), 3);
    auto *settings = panel(&right);
    settings->setMaximumWidth(260);
    columns->addWidget(settings, 2);
    auto *head = new QHBoxLayout;
    head->addWidget(label("对话记录", "panelTitle"));
    head->addStretch();
    voiceState = label("服务尚未接入", "caption");
    head->addWidget(voiceState);
    left->addLayout(head);
    transcript = new QPlainTextEdit;
    transcript->setReadOnly(true);
    transcript->setObjectName("voiceHistory");
    transcript->setMinimumSize(120, 120);
    transcript->setMaximumBlockCount(200);
    transcript->setStyleSheet(
        "QPlainTextEdit{background:white;border:1px solid #d9e3ef;padding:10px;font-size:16px;}");
    transcript->setPlaceholderText("按住下方按钮说话，识别与回复会显示在这里。");
    left->addWidget(transcript, 1);
    auto *buttons = new QHBoxLayout;
    talk = new QPushButton("按住说话");
    talk->setObjectName("voiceTalk");
    stop = new QPushButton("结束对话");
    stop->setProperty("kind", "secondary");
    buttons->addWidget(talk, 2);
    buttons->addWidget(stop);
    left->addLayout(buttons);
    connect(talk, &QPushButton::pressed, this, [this]() {
        if (voiceHoldActive) return;
        voiceHoldActive = true;
        client->request("voice.command", {{"command", "start_listen"}});
    });
    connect(talk, &QPushButton::released, this, [this]() {
        if (!voiceHoldActive) return;
        voiceHoldActive = false;
        client->request("voice.command", {{"command", "stop_listen"}});
    });
    connect(stop, &QPushButton::clicked, this, [this]() { stopVoiceInteraction(true); });
    right->addWidget(label("对话设置", "panelTitle"));
    activation = label("涂鸦服务尚未接入", "caption");
    right->addWidget(activation);
    right->addWidget(label("对话模式", "caption"));
    mode = new QComboBox;
    mode->setObjectName("voiceMode");
    mode->addItems({"按住说话", "连续对话"});
    right->addWidget(mode);
    connect(mode, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        voiceFreeChatActive = index == 1;
        client->request("voice.command",
                        {{"command", index == 1 ? "enter_free_chat" : "exit_free_chat"}});
    });
    auto *privacy = label("语音发送到已配置的涂鸦服务。对话记录只保留在本次会话内。", "infoNote");
    right->addWidget(privacy);
    right->addStretch();
    auto *clear = new QPushButton("清空对话记录");
    clear->setProperty("kind", "secondary");
    right->addWidget(clear);
    connect(clear, &QPushButton::clicked, this, [this]() { client->request("voice.clear"); });
    tabs->addTab(voicePage, "语音对话");
    auto *homePage = new LayoutPage;
    auto *homeLayout = new PageLayout(homePage);
    homeLayout->setContentsMargins(0, 8, 0, 0);
    homeLayout->setSpacing(6);
    auto *toolbar = new QHBoxLayout;
    rooms = new QComboBox;
    rooms->addItem("全部房间");
    rooms->setMinimumWidth(130);
    toolbar->addWidget(rooms);
    homeState = label("家庭账号尚未授权", "caption");
    toolbar->addWidget(homeState, 1);
    sync = new QPushButton("同步设备");
    toolbar->addWidget(sync);
    homeLayout->addLayout(toolbar);
    connect(sync, &QPushButton::clicked, this, [this]() { client->request("home.refresh"); });
    connect(rooms, QOverload<int>::of(&QComboBox::activated), this,
            [this](int) { apply(current); });
    auto *body = new QHBoxLayout;
    body->setSpacing(12);
    homeLayout->addLayout(body, 1);
    devices = new DataTable(3);
    devices->setObjectName("homeDevices");
    devices->setHorizontalHeaderLabels({"设备", "房间", "状态"});
    devices->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    devices->verticalHeader()->hide();
    devices->verticalHeader()->setDefaultSectionSize(54);
    devices->setMinimumSize(180, 130);
    devices->setSelectionBehavior(QAbstractItemView::SelectRows);
    devices->setSelectionMode(QAbstractItemView::SingleSelection);
    devices->setEditTriggers(QAbstractItemView::NoEditTriggers);
    body->addWidget(devices, 3);
    QVBoxLayout *controls;
    auto *detail = panel(&controls);
    detail->setMinimumWidth(190);
    detail->setMaximumWidth(310);
    body->addWidget(detail, 2);
    detailTitle = label("设备控制", "panelTitle");
    controls->addWidget(detailTitle);
    controlScroll = new QScrollArea;
    controlScroll->setWidgetResizable(true);
    controlScroll->setMinimumHeight(100);
    controls->addWidget(controlScroll, 1);
    connect(devices, &QTableWidget::currentCellChanged, this, [this](int, int, int, int) {
        if (!loading)
            selectDevice();
    });
    tabs->addTab(homePage, "家庭设备");
    note = label("接入服务未连接，家庭与语音操作暂不可用。", "infoNote");
    root->addWidget(note);
    connect(client, &DesktopClient::snapshotChanged, this, &FamilyPanel::apply);
    connect(client, &DesktopClient::feedback, this,
            [this](const QString &message) { note->setText(message); });
    apply({});
    qApp->installEventFilter(this);
}
bool FamilyPanel::selectDeviceById(const QString &id)
{
    if(id.isEmpty())return false;
    selectSection(1);
    selectedDevice=id;
    rooms->setCurrentIndex(0);
    apply(current);
    for(int row=0;row<devices->rowCount();++row) {
        if(devices->item(row,0)->data(Qt::UserRole).toString()==id) {
            devices->setCurrentCell(row,0);devices->scrollToItem(devices->item(row,0));
            selectDevice();return true;
        }
    }
    note->setText("绑定设备暂不可用，请同步家庭状态；没有改选其他设备。");
    return false;
}
void FamilyPanel::selectDevice()
{
    const int row = devices->currentRow();
    if (row < 0 || !devices->item(row, 0))
        return;
    selectedDevice = devices->item(row, 0)->data(Qt::UserRole).toString();
    functionsIdentity.clear();
    buildControls();
    if (current["service_connected"] == true && current["home"].toObject()["fresh"] == true)
        client->request("home.functions", {{"device_id", selectedDevice}});
}
bool FamilyPanel::canControl(const QString &id, const QString &code) const
{
    const auto home = current["home"].toObject();
    if (current["service_connected"] != true || home["authorization"] != "authorized" ||
        home["fresh"] != true || home["busy"] == true || device(current, id)["online"] != true ||
        home["pending"].toObject().contains(id + '/' + code))
        return false;
    for (const auto &entry : home["functions"].toObject()[id].toArray())
        if (entry.toObject()["code"] == code)
            return entry.toObject()["writable"] == true;
    return false;
}
void FamilyPanel::control(const QString &id, const QString &code, const QJsonValue &value)
{
    if (!canControl(id, code))
        return;
    const auto name = device(current, id)["name"].toString();
    QMessageBox dialog(QMessageBox::Question, "控制设备", QString(),
                       QMessageBox::Yes | QMessageBox::No, this);
    dialog.setTextFormat(Qt::PlainText);
    dialog.setText(name + "\n" + code + "：" + valueText(value) + "\n\n发送后等待设备状态确认。");
    dialog.setDefaultButton(QMessageBox::No);
    if (dialog.exec() != QMessageBox::Yes || !canControl(id, code))
        return;
    client->request("home.command", {{"device_id", id},
                                     {"code", code},
                                     {"value", value},
                                     {"command_id", QUuid::createUuid().toString(QUuid::Id128)},
                                     {"revision", current["home"].toObject()["revision"]},
                                     {"confirmed", true}});
}
void FamilyPanel::buildControls()
{
    auto *old = controlScroll->takeWidget();
    if (old)
        old->deleteLater();
    actions.clear();
    boolInputs.clear();
    reported.clear();
    controlBody = new LayoutPage;
    auto *layout = new PageLayout(controlBody);
    layout->setContentsMargins(2, 4, 6, 4);
    layout->setSpacing(9);
    const auto data = device(current, selectedDevice);
    detailTitle->setText(data["name"].toString("设备控制"));
    const auto list = current["home"].toObject()["functions"].toObject()[selectedDevice].toArray();
    if (selectedDevice.isEmpty() || list.isEmpty())
        layout->addWidget(label(selectedDevice.isEmpty()
                                    ? "同步设备后，选择一台查看可用控制。"
                                    : "正在读取控制能力，或当前设备暂无可用控制。",
                                "caption"));
    for (const auto &entry : list)
    {
        const auto spec = entry.toObject();
        const auto code = spec["code"].toString(), type = spec["type"].toString();
        const auto schema = spec["values"].toObject();
        layout->addWidget(label(spec["name"].toString(code), "panelTitle"));
        auto *value =
            label("设备：" + valueText(reportedValue(current, selectedDevice, code)), "caption");
        value->setProperty("code", code);
        layout->addWidget(value);
        reported << value;
        const auto id = selectedDevice;
        if (type == "Boolean")
        {
            auto *toggle = new ToggleSwitch;
            toggle->setProperty("code", code);
            layout->addWidget(toggle);
            boolInputs << toggle;
            actions << toggle;
            connect(toggle, &QCheckBox::toggled, this, [this, toggle, id, code](bool target) {
                {
                    QSignalBlocker block(toggle);
                    toggle->setChecked(reportedValue(current, id, code).toBool());
                }
                control(id, code, target);
            });
        }
        else if (type == "Integer" && schema["min"].isDouble() && schema["max"].isDouble() &&
                 schema["step"].toInt() > 0 && schema["min"].toDouble() >= -2147483648.0 &&
                 schema["max"].toDouble() <= 2147483647.0 &&
                 schema["min"].toDouble() <= schema["max"].toDouble())
        {
            auto *input = new QSpinBox;
            input->setRange(int(schema["min"].toDouble()), int(schema["max"].toDouble()));
            input->setSingleStep(schema["step"].toInt());
            const auto actual = reportedValue(current, id, code);
            if (actual.isDouble())
                input->setValue(int(actual.toDouble()));
            layout->addWidget(input);
            auto *send = new QPushButton("设置此项");
            send->setProperty("code", code);
            layout->addWidget(send);
            actions << send;
            connect(send, &QPushButton::clicked, this,
                    [this, input, id, code]() { control(id, code, input->value()); });
        }
        else if (type == "Enum" && schema["range"].isArray() &&
                 schema["range"].toArray().size() <= 64)
        {
            auto *input = new QComboBox;
            for (const auto &option : schema["range"].toArray())
                if (option.isString() && option.toString().size() <= 128)
                    input->addItem(option.toString());
            input->setCurrentText(reportedValue(current, id, code).toString());
            layout->addWidget(input);
            auto *send = new QPushButton("设置此项");
            send->setProperty("code", code);
            layout->addWidget(send);
            actions << send;
            connect(send, &QPushButton::clicked, this,
                    [this, input, id, code]() { control(id, code, input->currentText()); });
        }
        else
            layout->addWidget(label("此控制类型暂不可用", "caption"));
    }
    layout->addStretch();
    controlScroll->setWidget(controlBody);
}
void FamilyPanel::apply(const QJsonObject &state)
{
    current = state;
    const bool connected = state["service_connected"] == true;
    const auto voice = state["voice"].toObject(), home = state["home"].toObject();
    const bool ready = connected && voice["ready"] == true;
    if (!ready) stopVoiceInteraction();
    const auto status = voice["state"].toString();
    static const QMap<QString, QString> states = {{"idle", "等待说话"},
                                                  {"listening", "正在听"},
                                                  {"thinking", "正在处理"},
                                                  {"speaking", "正在回复"},
                                                  {"error", "对话服务异常"}};
    voiceState->setText(
        !connected ? "服务未连接" : !ready ? "语音尚未就绪" : states.value(status, "等待服务状态"));
    activation->setText(ready ? "激活与录放音已就绪"
                              : voice["configured"] == true ? "等待激活与录放音就绪"
                                                            : "涂鸦运行时尚未接入");
    talk->setEnabled(ready && (talk->isDown() || status == "idle" || status == "speaking"));
    stop->setEnabled(ready);
    mode->setEnabled(ready);
    const auto encoded = QJsonDocument(voice["history"].toArray()).toJson(QJsonDocument::Compact);
    if (encoded != historyIdentity)
    {
        historyIdentity = encoded;
        QStringList lines;
        for (const auto &item : voice["history"].toArray())
        {
            const auto entry = item.toObject();
            lines << (entry["speaker"] == "user" ? "你" : "涂鸦助手") + QString("\n") +
                         entry["text"].toString() + QString("\n");
        }
        transcript->setPlainText(lines.join('\n'));
    }
    sync->setEnabled(connected && home["configured"] == true && home["busy"] != true);
    homeState->setText(!connected
                           ? "接入服务未连接"
                           : home["busy"] == true
                                 ? "正在同步"
                                 : home["authorization"] == "expired"
                                       ? "家庭授权已失效"
                                       : home["authorization"] == "authorized"
                                             ? (home["fresh"] == true ? "设备状态已同步"
                                                                      : "设备状态已过期，请同步")
                                             : "请先在官网关联家庭账号");
    loading = true;
    {
        QSignalBlocker block(devices);
        QSignalBlocker roomBlock(rooms);
        const auto room = rooms->currentText();
        QStringList names{"全部房间"};
        for (const auto &entry : home["devices"].toArray())
        {
            const auto value = entry.toObject()["room"].toString();
            if (!value.isEmpty() && !names.contains(value))
                names << value;
        }
        rooms->clear();
        rooms->addItems(names);
        rooms->setCurrentIndex(qMax(0, rooms->findText(room)));
        devices->setRowCount(0);
        if (connected)
            for (const auto &entry : home["devices"].toArray())
            {
                const auto d = entry.toObject();
                if (rooms->currentIndex() > 0 && d["room"].toString() != rooms->currentText())
                    continue;
                const int row = devices->rowCount();
                devices->insertRow(row);
                QStringList values{d["name"].toString(), d["room"].toString(),
                                   home["fresh"] != true ? "状态过期"
                                                         : d["online"] == true ? "在线" : "离线"};
                for (int c = 0; c < values.size(); ++c)
                    devices->setItem(row, c, new QTableWidgetItem(values[c]));
                devices->item(row, 0)->setData(Qt::UserRole, d["id"]);
                if (d["id"] == selectedDevice)
                    devices->setCurrentCell(row, 0);
            }
    }
    loading = false;
    const auto signature = selectedDevice.toUtf8() +
                           QJsonDocument(home["functions"].toObject()[selectedDevice].toArray())
                               .toJson(QJsonDocument::Compact);
    if (signature != functionsIdentity || !controlBody)
    {
        functionsIdentity = signature;
        buildControls();
    }
    for (auto *action : actions)
    {
        const auto code = action->property("code").toString();
        action->setEnabled(canControl(selectedDevice, code));
    }
    for (auto *toggle : boolInputs)
    {
        const auto value =
            reportedValue(state, selectedDevice, toggle->property("code").toString());
        QSignalBlocker block(toggle);
        toggle->setChecked(value.toBool());
        toggle->setEnabled(toggle->isEnabled() && value.isBool());
    }
    for (auto *value : reported)
    {
        const auto code = value->property("code").toString();
        const auto pending = home["pending"].toObject()[selectedDevice + '/' + code].toObject();
        value->setText("设备：" + valueText(reportedValue(state, selectedDevice, code)) +
                       (pending.isEmpty() ? QString() : "\n结果待确认，请同步状态"));
    }
    note->setText(!connected
                      ? "接入服务未连接，家庭与语音操作暂不可用。"
                      : !home["error"].toString().isEmpty()
                            ? "家庭请求未完成，请检查账号授权并重新同步。"
                            : "家庭控制按设备能力提供；指令受理后保留上次状态，等待设备确认。");
}
void FamilyPanel::stopVoiceInteraction(bool explicitStop)
{
    if (!explicitStop && !voiceHoldActive && !voiceFreeChatActive) return;
    // Clear intent before sending: cancellation and a delayed button release may overlap.
    voiceHoldActive = false;
    voiceFreeChatActive = false;
    talk->setDown(false);
    const QSignalBlocker blocker(mode);
    mode->setCurrentIndex(0);
    client->request("voice.command", {{"command", "stop_listen"}});
    client->request("voice.command", {{"command", "exit_free_chat"}});
}
bool FamilyPanel::eventFilter(QObject *watched, QEvent *event)
{
    if (voiceHoldActive || voiceFreeChatActive) {
        const auto *widget = qobject_cast<QWidget *>(watched);
        const bool ownWidget = widget && (widget == this || isAncestorOf(widget) || widget == window());
        if (event->type() == QEvent::ApplicationDeactivate ||
            (ownWidget && (event->type() == QEvent::TouchCancel || event->type() == QEvent::WindowDeactivate)) ||
            (watched == talk && event->type() == QEvent::Hide))
            stopVoiceInteraction();
    }
    return QWidget::eventFilter(watched, event);
}
void FamilyPanel::hideEvent(QHideEvent *event)
{
    stopVoiceInteraction();
    QWidget::hideEvent(event);
}
