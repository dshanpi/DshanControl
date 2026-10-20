// Local design review only. Synthetic fixtures; no network or credential access.
#include "../../components/hmi/src/visuals.h"
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSlider>
#include <QStackedWidget>
#include <QTabBar>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

static QLabel *label(const QString &text, const char *name = "")
{
    auto *w = new QLabel(text);
    w->setObjectName(name);
    w->setWordWrap(text.contains('\n') || QString(name) == "note" || QString(name) == "bubble" ||
                   QString(name) == "answer" || text.size() > 36);
    w->setTextFormat(Qt::PlainText);
    return w;
}
static QPushButton *button(const QString &text, bool secondary = false)
{
    auto *b = new QPushButton(text);
    b->setMinimumHeight(44);
    if (secondary)
        b->setProperty("secondary", true);
    return b;
}
class Panel : public QFrame
{
  public:
    bool hasHeightForWidth() const override { return false; }
};
static QFrame *panel(QVBoxLayout **layout)
{
    auto *p = new Panel;
    p->setObjectName("panel");
    *layout = new PageLayout(p);
    (*layout)->setContentsMargins(18, 14, 18, 14);
    (*layout)->setSpacing(8);
    return p;
}
static void rule(QVBoxLayout *layout)
{
    auto *r = new QFrame;
    r->setFixedHeight(1);
    r->setObjectName("rule");
    layout->addWidget(r);
}
static void field(QVBoxLayout *layout, const QString &name, const QString &value)
{
    auto *row = new QHBoxLayout;
    row->addWidget(label(name, "muted"));
    row->addStretch();
    row->addWidget(label(value, "fieldValue"));
    layout->addLayout(row);
}
static QTableWidget *table(const QStringList &headers, const QList<QStringList> &rows)
{
    auto *t = new QTableWidget(rows.size(), headers.size());
    t->setHorizontalHeaderLabels(headers);
    t->verticalHeader()->hide();
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    t->horizontalHeader()->setFixedHeight(38);
    t->setShowGrid(false);
    t->setAlternatingRowColors(true);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    for (int r = 0; r < rows.size(); ++r)
    {
        t->setRowHeight(r, 54);
        for (int c = 0; c < headers.size(); ++c)
        {
            auto *i = new QTableWidgetItem(rows[r].value(c));
            t->setItem(r, c, i);
        }
    }
    return t;
}
class Preview : public QWidget
{
  public:
    QStackedWidget *pages;
    QTabBar *tabs;
    QLabel *title, *subtitle, *feedback;
    QLabel *geometryLabel = nullptr;
    QList<NavigationButton *> nav;
    int tab = 0;
    QString ownership = "unbound", home = "devices", room = "全部房间";
    Preview()
    {
        setWindowTitle("DshanControl 云服务与家庭接入 · Qt设计原型");
        resize(1024, 600);
        Visuals::initializeFonts();
        setStyleSheet(R"(
            * { font-family:"Dshan Sans"; font-size:14px; color:#092650; }
            QWidget#root,QStackedWidget { background:#f2f7fd; }
            QWidget#historySurface { background:white; }
            QFrame#header,QFrame#footer { background:white; }
            QFrame#header { border-bottom:1px solid #d9e3ef; }
            QFrame#footer { border-top:1px solid #d9e3ef; }
            QFrame#nav { background:#e8f3ff; border-right:1px solid #d9e3ef; }
            QFrame#panel { background:white; border:1px solid #d9e3ef; border-radius:5px; }
            QFrame#rule { background:#d9e3ef; }
            QLabel#brand { font-size:20px; font-weight:700; }
            QLabel#brandMark { color:white; background:#0062ff; border-radius:5px; font-size:23px; font-weight:700; }
            QLabel#title { font-size:25px; font-weight:700; }
            QLabel#panelTitle { font-size:18px; font-weight:700; }
            QLabel#muted,QLabel#subtitle { color:#61758b; font-size:13px; }
            QLabel#success { color:#168366; }
            QLabel#warning { color:#a3650c; }
            QLabel#note { background:#eaf3ff; color:#395784; border-radius:4px; padding:12px; }
            QLabel#code { font-size:38px; font-weight:700; color:#0062ff; padding:16px 0; }
            QLabel#emptyTitle { font-size:23px; font-weight:700; }
            QLabel#bubble { background:#edf4fe; border-radius:5px; padding:13px 16px; font-size:17px; }
            QLabel#answer { background:#f5f8fc; border-radius:5px; padding:13px 16px; font-size:17px; }
            QPushButton { color:white; background:#0062ff; border:1px solid #0062ff; border-radius:4px; padding:0 16px; min-height:44px; }
            QPushButton[secondary="true"] { color:#0062ff; background:white; border-color:#bbcde4; }
            QPushButton:disabled { color:#8190a5; background:#edf1f6; border-color:#d9e3ef; }
            QPushButton:focus,QComboBox:focus,QLineEdit:focus { border:2px solid #0062ff; }
            QPushButton#navButton { background:transparent; border:1px solid transparent; padding:0 4px; }
            QTabBar::tab { min-height:44px; padding:0 24px; color:#61758b; background:transparent; border-bottom:1px solid #d9e3ef; }
            QTabBar::tab:selected { color:#0062ff; border-bottom:3px solid #0062ff; font-weight:700; }
            QComboBox,QLineEdit { min-height:42px; background:white; border:1px solid #bbcde4; border-radius:4px; padding:0 10px; }
            QTableWidget { border:1px solid #d9e3ef; background:white; alternate-background-color:#f6f9fd; gridline-color:#d9e3ef; }
            QTableWidget::item { padding:8px; }
            QHeaderView::section { background:#edf3fb; color:#49627d; border:none; padding:8px; }
            QScrollArea { border:none; background:transparent; }
            QScrollBar:vertical { width:10px; background:#edf3fb; }
            QSlider { min-height:40px; }
            QSlider::groove:horizontal { height:5px; background:#d9e3ef; border-radius:2px; }
            QSlider::handle:horizontal { background:white; border:2px solid #0062ff; width:22px; margin:-9px 0; border-radius:11px; }
        )");
        setObjectName("root");
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);
        auto *header = new QFrame;
        header->setObjectName("header");
        header->setFixedHeight(40);
        auto *hr = new QHBoxLayout(header);
        hr->setContentsMargins(16, 0, 18, 0);
        auto *mark = label("D", "brandMark");
        mark->setFixedSize(27, 27);
        mark->setAlignment(Qt::AlignCenter);
        hr->addWidget(mark);
        hr->addWidget(label("DshanControl", "brand"));
        hr->addWidget(label("工业控制桌面", "muted"));
        hr->addStretch();
        hr->addWidget(label("●  本地界面预览", "muted"));
        hr->addSpacing(18);
        hr->addWidget(label("14:28", "muted"));
        root->addWidget(header);
        auto *body = new QHBoxLayout;
        body->setContentsMargins(0, 0, 0, 0);
        body->setSpacing(0);
        auto *side = new QFrame;
        side->setObjectName("nav");
        side->setFixedWidth(140);
        auto *nl = new QVBoxLayout(side);
        nl->setContentsMargins(5, 11, 5, 12);
        nl->setSpacing(3);
        QStringList names = {"运行总览", "现场总线", "网络通信",   "设备监控",
                             "告警记录", "云服务",   "家庭与对话", "系统设置"};
        QStringList icons = {"bars", "topology", "globe", "monitor",
                             "bell", "cloud",    "sun",   "settings"};
        for (int i = 0; i < names.size(); ++i)
        {
            auto *b = new NavigationButton(names[i], icons[i]);
            b->setObjectName("navButton");
            b->setCheckable(true);
            b->setMinimumHeight(44);
            nl->addWidget(b);
            nav << b;
            connect(b, &QPushButton::clicked, this, [this, i]() {
                if (i == 5)
                    select(0);
                else if (i == 6)
                    select(2);
                else
                    feedback->setText("原有页面保持现状；本原型展示新增接入页面");
            });
        }
        nl->addStretch();
        body->addWidget(side);
        auto *area = new QWidget;
        auto *al = new QVBoxLayout(area);
        al->setContentsMargins(18, 15, 18, 14);
        al->setSpacing(10);
        title = label("云服务", "title");
        subtitle = label("官网账号、设备归属与数据上报", "subtitle");
        al->addWidget(title);
        al->addWidget(subtitle);
        tabs = new QTabBar;
        tabs->setExpanding(false);
        al->addWidget(tabs);
        pages = new QStackedWidget;
        al->addWidget(pages, 1);
        body->addWidget(area, 1);
        root->addLayout(body, 1);
        auto *footer = new QFrame;
        footer->setObjectName("footer");
        footer->setFixedHeight(28);
        auto *fr = new QHBoxLayout(footer);
        fr->setContentsMargins(14, 0, 14, 0);
        feedback = label("Qt交互设计稿 · 全部账号、设备与读数为示例", "muted");
        feedback->setWordWrap(false);
        fr->addWidget(feedback);
        fr->addStretch();
        geometryLabel = label("1024 × 600", "muted");
        fr->addWidget(geometryLabel);
        root->addWidget(footer);
        connect(tabs, &QTabBar::currentChanged, this, [this](int i) {
            if (i >= 0)
            {
                tab = (tab < 2 ? 0 : 2) + i;
                render();
            }
        });
        select(0);
    }
    void resizeEvent(QResizeEvent *e) override
    {
        QWidget::resizeEvent(e);
        if (geometryLabel)
            geometryLabel->setText(QString("%1 × %2").arg(width()).arg(height()));
    }
    void select(int n)
    {
        tab = n;
        tabs->blockSignals(true);
        while (tabs->count())
            tabs->removeTab(0);
        QStringList names =
            n < 2 ? QStringList{"官网绑定", "数据上报"} : QStringList{"语音对话", "家庭设备"};
        for (auto &s : names)
            tabs->addTab(s);
        tabs->setCurrentIndex(n % 2);
        tabs->blockSignals(false);
        title->setText(n < 2 ? "云服务" : "家庭与对话");
        subtitle->setText(n < 2 ? "官网账号、设备归属与数据上报" : "涂鸦语音服务与已授权家庭设备");
        for (int i = 0; i < nav.size(); ++i)
            nav[i]->setChecked(i == (n < 2 ? 5 : 6));
        render();
    }
    void render()
    {
        while (pages->count())
        {
            auto *w = pages->widget(0);
            pages->removeWidget(w);
            delete w;
        }
        auto *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        auto *w = new LayoutPage;
        auto *row = new PageRowLayout(w);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(14);
        if (tab == 0)
            pairing(row);
        else if (tab == 1)
            reporting(row);
        else if (tab == 2)
            voice(row);
        else
            household(row);
        scroll->setWidget(w);
        pages->addWidget(scroll);
    }
    void pairing(QHBoxLayout *row)
    {
        QVBoxLayout *l, *r;
        auto *left = panel(&l);
        auto *right = panel(&r);
        row->addWidget(left, 1);
        row->addWidget(right, 1);
        l->addWidget(label("100ask.net", "panelTitle"));
        rule(l);
        field(l, "设备名称", "桌面控制器 01");
        field(l, "设备登记", "已登记（示例）");
        field(l, "官网连接", "已连接（示例）");
        field(l, "账号归属",
              ownership == "bound" ? "已绑定" : ownership == "unknown" ? "待确认" : "尚未绑定");
        rule(l);
        l->addWidget(label("在官网「我的设备」中输入右侧绑定码，完成账号绑定。", "muted"));
        l->addStretch();
        auto *refresh = button("重新查询设备归属", true);
        l->addWidget(refresh);
        connect(refresh, &QPushButton::clicked, this, [this]() {
            ownership = "bound";
            render();
            feedback->setText("演示交互：收到明确的已绑定状态；未调用官网");
        });
        r->addWidget(label(ownership == "bound"
                               ? "已绑定官网账户"
                               : ownership == "unknown"
                                     ? "归属状态待确认"
                                     : ownership == "expired" ? "绑定码已过期" : "本次绑定码",
                           "panelTitle"));
        r->addStretch();
        if (ownership == "bound")
        {
            auto *icon = label("");
            icon->setPixmap(Visuals::icon("check", QColor("#168366")).pixmap(64, 64));
            icon->setAlignment(Qt::AlignCenter);
            r->addWidget(icon);
            auto *t = label("设备已加入你的账号", "emptyTitle");
            t->setAlignment(Qt::AlignCenter);
            r->addWidget(t);
            r->addWidget(label("可在官网查看上报数据与管理设备归属。", "muted"));
        }
        else if (ownership == "unknown")
            r->addWidget(label(
                "暂时无法确认设备属于哪个账号。\n请重新查询；已有身份和绑定保持不变。", "note"));
        else
        {
            auto *code =
                label(ownership == "expired" ? "—  —  —  —  —  —" : "4  8  2  7  1  9", "code");
            code->setAlignment(Qt::AlignCenter);
            r->addWidget(code);
            auto *hint = label(ownership == "expired" ? "请重新获取，再到官网输入。"
                                                      : "有效时间  04:32\n仅用于本次账号绑定",
                               "muted");
            hint->setAlignment(Qt::AlignCenter);
            r->addWidget(hint);
        }
        r->addStretch();
        auto *renew =
            button(ownership == "bound" ? "查看设备管理"
                                        : ownership == "unknown" ? "重新查询" : "获取新绑定码",
                   true);
        r->addWidget(renew);
        connect(renew, &QPushButton::clicked, this, [this]() {
            if (ownership == "bound")
            {
                feedback->setText("演示交互：官网管理入口，不在本机自动解绑");
                return;
            }
            ownership = "unbound";
            render();
            feedback->setText("演示交互：申请了新的示例码；没有实际发出请求");
        });
    }
    void reporting(QHBoxLayout *row)
    {
        QVBoxLayout *l, *r;
        auto *left = panel(&l);
        auto *right = panel(&r);
        right->setMaximumWidth(272);
        row->addWidget(left, 3);
        row->addWidget(right, 2);
        auto *head = new QHBoxLayout;
        head->addWidget(label("点位上报", "panelTitle"));
        head->addStretch();
        head->addWidget(label("●  服务已连接（示例）", "success"));
        l->addLayout(head);
        l->addWidget(label("本地采集持续运行，按所选周期上报有效读数。", "muted"));
        l->addWidget(table({"点位", "读数", "来源", "上报状态"},
                           {{"环境温度", "24.3 °C", "I²C / 示例", "已送达"},
                            {"环境湿度", "52.6 %", "I²C / 示例", "已送达"},
                            {"车间压力", "--", "RS485 / 示例", "缺少数据"},
                            {"门磁状态", "关闭", "MQTT / 示例", "等待确认"}}),
                     1);
        l->addWidget(label("缺测和过期点位保留状态，不用 0 代替。", "muted"));
        r->addWidget(label("自动上报", "panelTitle"));
        auto *enabled = new ToggleSwitch("启用上报");
        enabled->setChecked(true);
        r->addWidget(enabled);
        rule(r);
        r->addWidget(label("上报周期", "muted"));
        auto *period = new QComboBox;
        period->addItems({"每 30 秒", "每 60 秒", "每 5 分钟"});
        r->addWidget(period);
        field(r, "最近送达", "14:28:00");
        field(r, "待发点位", "1 项");
        r->addStretch();
        auto *send = button("立即上报一次");
        r->addWidget(send);
        connect(send, &QPushButton::clicked, this, [this]() {
            feedback->setText("演示交互：请求上报；真实成功需由云端接收结果确认");
        });
        connect(enabled, &QCheckBox::toggled, this, [this](bool on) {
            feedback->setText(on ? "演示：自动上报已开启，本地采集保持运行"
                                 : "演示：自动上报已暂停，本地采集保持运行");
        });
    }
    void voice(QHBoxLayout *row)
    {
        QVBoxLayout *l, *r;
        auto *left = panel(&l);
        auto *right = panel(&r);
        right->setMaximumWidth(264);
        row->addWidget(left, 3);
        row->addWidget(right, 2);
        auto *head = new QHBoxLayout;
        head->addWidget(label("对话记录", "panelTitle"));
        head->addStretch();
        head->addWidget(label("●  等待说话", "success"));
        l->addLayout(head);
        rule(l);
        auto *history = new QScrollArea;
        history->setWidgetResizable(true);
        history->setMinimumHeight(140);
        auto *historyBody = new LayoutPage;
        historyBody->setObjectName("historySurface");
        auto *messages = new PageLayout(historyBody);
        messages->setContentsMargins(0, 0, 4, 0);
        messages->setSpacing(8);
        messages->addWidget(label("你  ·  14:26", "muted"));
        messages->addWidget(label("现在客厅的温度是多少？", "bubble"));
        messages->addWidget(label("涂鸦助手  ·  14:26", "muted"));
        messages->addWidget(label("客厅温度是 24.3 摄氏度。", "answer"));
        messages->addWidget(label("你  ·  14:27", "muted"));
        messages->addWidget(label("打开书房台灯。", "bubble"));
        messages->addWidget(label("涂鸦助手  ·  14:27", "muted"));
        messages->addWidget(label("书房台灯已打开。", "answer"));
        messages->addStretch();
        history->setWidget(historyBody);
        l->addWidget(history, 1);
        auto *actions = new QHBoxLayout;
        auto *talk = button("按住说话");
        auto *stop = button("结束对话", true);
        actions->addWidget(talk, 2);
        actions->addWidget(stop);
        l->addLayout(actions);
        connect(talk, &QPushButton::pressed, this,
                [this]() { feedback->setText("演示交互：正在听；本原型不打开麦克风"); });
        connect(talk, &QPushButton::released, this,
                [this]() { feedback->setText("演示交互：说话结束，等待对话服务返回"); });
        connect(stop, &QPushButton::clicked, this,
                [this]() { feedback->setText("演示交互：结束本轮对话，不关闭家庭设备服务"); });
        r->addWidget(label("对话设置", "panelTitle"));
        field(r, "涂鸦服务", "已激活（示例）");
        rule(r);
        r->addWidget(label("对话模式", "muted"));
        auto *mode = new QComboBox;
        mode->addItems({"按住说话", "连续对话"});
        r->addWidget(mode);
        r->addWidget(label("播放音量  50%", "muted"));
        auto *volume = new TouchSlider;
        volume->setRange(0, 100);
        volume->setValue(50);
        r->addWidget(volume);
        r->addStretch();
        auto *privacy = button("麦克风与隐私", true);
        r->addWidget(privacy);
        connect(privacy, &QPushButton::clicked, this, [this]() {
            feedback->setText("原型说明：语音送涂鸦处理；麦克风默认关闭，录音权限单独启用");
        });
    }
    void household(QHBoxLayout *row)
    {
        QVBoxLayout *l;
        auto *p = panel(&l);
        l->setSpacing(6);
        row->addWidget(p);
        if (home == "empty" || home == "expired")
        {
            l->addStretch();
            auto *icon = label("");
            icon->setAlignment(Qt::AlignCenter);
            icon->setPixmap(Visuals::icon("globe").pixmap(56, 56));
            l->addWidget(icon);
            auto *t = label(home == "empty" ? "连接你的智能家庭" : "家庭授权已失效", "emptyTitle");
            t->setAlignment(Qt::AlignCenter);
            l->addWidget(t);
            auto *note = label(
                home == "empty" ? "在官网关联智能生活账号，再同步家中的设备。\n官网设备绑定与家庭账"
                                  "号授权分别管理。"
                                : "设备状态已停止更新。请重新授权后同步，\n当前不能发送控制指令。",
                "muted");
            note->setAlignment(Qt::AlignCenter);
            l->addWidget(note);
            auto *go = button(home == "empty" ? "查看连接步骤" : "重新授权");
            go->setMaximumWidth(220);
            l->addWidget(go, 0, Qt::AlignHCenter);
            l->addStretch();
            connect(go, &QPushButton::clicked, this, [this]() {
                feedback->setText("演示交互：打开账号授权流程；未读写实际账号或密钥");
            });
            return;
        }
        auto *head = new QHBoxLayout;
        head->addWidget(label("我的家庭", "panelTitle"));
        head->addWidget(label("4 个设备 · 示例", "muted"));
        head->addStretch();
        auto *refresh = button("同步设备", true);
        head->addWidget(refresh);
        l->addLayout(head);
        auto *rooms = new QHBoxLayout;
        for (auto text : {"全部房间", "客厅", "书房"})
        {
            auto *b = button(text, true);
            rooms->addWidget(b);
            connect(b, &QPushButton::clicked, this, [this, text]() {
                room = text;
                render();
                feedback->setText(QString("演示交互：按%1筛选设备").arg(text));
            });
        }
        rooms->addStretch();
        l->addLayout(rooms);
        rule(l);
        QStringList names = {"书房台灯", "客厅落地灯", "空气净化器", "玄关插座"};
        QStringList detail = {"书房  ·  亮度 60%", "客厅  ·  关闭", "客厅  ·  等待设备回执",
                              "玄关  ·  12 分钟前离线"};
        for (int i = 0; i < 4; ++i)
        {
            if (room != "全部房间" && room != (i == 0 ? "书房" : i == 3 ? "玄关" : "客厅"))
                continue;
            auto *device = new QHBoxLayout;
            auto *icon = label("");
            icon->setPixmap(Visuals::icon(i < 2 ? "sun" : "server").pixmap(28, 28));
            device->addWidget(icon);
            device->addSpacing(10);
            auto *text = new QVBoxLayout;
            text->setSpacing(4);
            text->addWidget(label(names[i], "panelTitle"));
            text->addWidget(label(detail[i], "muted"));
            device->addLayout(text, 1);
            auto *deviceState = label(i == 2 ? "结果待确认" : i == 3 ? "离线" : "在线",
                                      i == 2 ? "warning" : i == 3 ? "muted" : "success");
            device->addWidget(deviceState);
            if (i < 2)
            {
                auto *toggle = new ToggleSwitch;
                toggle->setObjectName(QString("homeSwitch%1").arg(i));
                const bool reported = i == 0;
                toggle->setChecked(reported);
                device->addWidget(toggle);
                connect(toggle, &QCheckBox::toggled, this,
                        [this, toggle, reported, deviceState](bool) {
                            deviceState->setText("等待回执");
                            deviceState->setStyleSheet("color:#a3650c;");
                            toggle->blockSignals(true);
                            toggle->setChecked(reported);
                            toggle->blockSignals(false);
                            toggle->setEnabled(false);
                            feedback->setText(
                                "演示：指令已受理，保留上次设备状态；等待回执，不自动重发");
                        });
            }
            else
            {
                auto *unknown = label("—", "muted");
                unknown->setFixedWidth(108);
                unknown->setAlignment(Qt::AlignCenter);
                device->addWidget(unknown);
            }
            l->addLayout(device);
            if (i < 3)
                rule(l);
        }
        l->addStretch();
        l->addWidget(label("控制按设备能力提供。指令受理后仍需等待设备状态确认。", "muted"));
        connect(refresh, &QPushButton::clicked, this,
                [this]() { feedback->setText("演示交互：同步已授权设备列表与状态，未实际联网"); });
    }
};
int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    Preview w;
    QStringList args = app.arguments();
    QString dir;
    if (args.contains("--render"))
    {
        int i = args.indexOf("--render");
        dir = args.value(i + 1);
        if (dir.isEmpty())
            return 2;
        QDir().mkpath(dir);
    }
    w.show();
    app.processEvents();
    if (!dir.isEmpty())
    {
        struct Shot
        {
            QString file;
            int page;
            QString state;
        };
        QList<Shot> shots = {
            {"01-cloud-pairing", 0, "unbound"},   {"02-sensor-reporting", 1, ""},
            {"03-tuya-conversation", 2, ""},      {"04-smart-home", 3, "devices"},
            {"05-cloud-bound", 0, "bound"},       {"06-cloud-unknown", 0, "unknown"},
            {"07-pairing-expired", 0, "expired"}, {"08-home-unlinked", 3, "empty"},
            {"09-home-expired", 3, "expired"}};
        QJsonArray results;
        for (auto shot : shots)
        {
            w.ownership = shot.page == 0 ? shot.state : "unbound";
            w.home = shot.page == 3 ? shot.state : "devices";
            w.select(shot.page);
            w.setFocus(Qt::OtherFocusReason);
            app.processEvents();
            QString file = dir + "/" + shot.file + ".png";
            if (!w.grab().save(file))
                return 3;
            results.append(QJsonObject{{"file", shot.file + ".png"},
                                       {"width", w.width()},
                                       {"height", w.height()},
                                       {"native_qt", true},
                                       {"data", "synthetic-design-fixture"}});
        }
        w.home = "devices";
        w.select(3);
        w.resize(800, 480);
        app.processEvents();
        if (w.size() != QSize(800, 480))
            return 5;
        if (!w.grab().save(dir + "/10-smart-home-800x480.png"))
            return 3;
        w.resize(1280, 720);
        w.select(1);
        app.processEvents();
        if (!w.grab().save(dir + "/11-reporting-1280x720.png"))
            return 3;
        results.append(QJsonObject{{"file", "10-smart-home-800x480.png"},
                                   {"width", 800},
                                   {"height", 480},
                                   {"native_qt", true},
                                   {"data", "synthetic-design-fixture"}});
        results.append(QJsonObject{{"file", "11-reporting-1280x720.png"},
                                   {"width", 1280},
                                   {"height", 720},
                                   {"native_qt", true},
                                   {"data", "synthetic-design-fixture"}});
        w.resize(1024, 600);
        w.room = "全部房间";
        w.home = "devices";
        w.select(3);
        app.processEvents();
        auto *toggle = w.findChild<QCheckBox *>("homeSwitch0");
        if (!toggle || !toggle->isChecked())
            return 6;
        toggle->click();
        app.processEvents();
        if (!toggle->isChecked() || toggle->isEnabled() || !w.feedback->text().contains("等待回执"))
            return 7;
        if (!w.grab().save(dir + "/12-home-command-pending.png"))
            return 3;
        results.append(QJsonObject{{"file", "12-home-command-pending.png"},
                                   {"width", 1024},
                                   {"height", 600},
                                   {"native_qt", true},
                                   {"data", "synthetic-design-fixture"},
                                   {"pending_preserves_reported_state", true}});
        QFile file(dir + "/render-manifest.json");
        if (!file.open(QIODevice::WriteOnly))
            return 4;
        file.write(QJsonDocument(results).toJson());
        return 0;
    }
    return app.exec();
}
