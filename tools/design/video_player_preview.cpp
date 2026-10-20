// Native Qt design preview only. No media-file, multimedia-device or network access.
#include "../../components/hmi/src/visuals.h"
#include <QApplication>
#include <QButtonGroup>
#include <QComboBox>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPainterPath>
#include <QResizeEvent>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace
{
QLabel *text(const QString &value, const QString &name = QString())
{
    auto *label = new QLabel(value);
    label->setObjectName(name);
    label->setTextFormat(Qt::PlainText);
    return label;
}
QPushButton *button(const QString &value, bool secondary = true)
{
    auto *b = new QPushButton(value);
    b->setMinimumHeight(44);
    b->setProperty("secondary", secondary);
    return b;
}
QFrame *panel(QVBoxLayout **layout)
{
    auto *p = new QFrame;
    p->setObjectName("panel");
    *layout = new PageLayout(p);
    (*layout)->setContentsMargins(12, 12, 12, 12);
    (*layout)->setSpacing(8);
    return p;
}
void rule(QVBoxLayout *layout)
{
    auto *line = new QFrame;
    line->setObjectName("rule");
    line->setFixedHeight(1);
    layout->addWidget(line);
}
void field(QVBoxLayout *layout, const QString &name, const QString &value)
{
    auto *row = new QHBoxLayout;
    row->addWidget(text(name, "muted"));
    row->addStretch();
    row->addWidget(text(value));
    layout->addLayout(row);
}
QString timeText(int seconds)
{
    return QString("%1:%2")
        .arg(seconds / 60, 2, 10, QChar('0'))
        .arg(seconds % 60, 2, 10, QChar('0'));
}

class ToolButton : public QPushButton
{
  public:
    QString glyph;
    ToolButton(const QString &name, const QString &icon, bool primary = false) : glyph(icon)
    {
        setAccessibleName(name);
        setToolTip(name);
        setFixedSize(primary ? 56 : 44, 44);
        setProperty("secondary", !primary);
    }

  protected:
    void paintEvent(QPaintEvent *event) override
    {
        QPushButton::paintEvent(event);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.translate(width() / 2.0 - 12, height() / 2.0 - 12);
        QColor color = property("secondary").toBool() ? QColor("#0062ff") : Qt::white;
        if (!isEnabled())
            color = QColor("#8190a5");
        p.setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        if (glyph == "play")
        {
            p.setBrush(color);
            QPolygonF triangle;
            triangle << QPointF(7, 3) << QPointF(21, 12) << QPointF(7, 21);
            p.drawPolygon(triangle);
        }
        else if (glyph == "pause")
        {
            p.fillRect(QRectF(6, 4, 4, 16), color);
            p.fillRect(QRectF(15, 4, 4, 16), color);
        }
        else if (glyph == "previous" || glyph == "next")
        {
            if (glyph == "previous")
            {
                p.translate(24, 0);
                p.scale(-1, 1);
            }
            p.drawLine(19, 4, 19, 20);
            QPolygonF triangle;
            triangle << QPointF(4, 4) << QPointF(16, 12) << QPointF(4, 20);
            p.setBrush(color);
            p.drawPolygon(triangle);
        }
        else if (glyph == "fullscreen")
        {
            for (int i = 0; i < 4; ++i)
            {
                p.drawLine(3, 8, 3, 3);
                p.drawLine(3, 3, 8, 3);
                p.translate(24, 0);
                p.rotate(90);
            }
        }
        else if (glyph == "volume" || glyph == "muted")
        {
            QPolygonF speaker;
            speaker << QPointF(3, 9) << QPointF(7, 9) << QPointF(12, 5) << QPointF(12, 19)
                    << QPointF(7, 15) << QPointF(3, 15);
            p.drawPolygon(speaker);
            if (glyph == "muted")
            {
                p.drawLine(16, 9, 22, 15);
                p.drawLine(16, 15, 22, 9);
            }
            else
            {
                p.drawArc(QRectF(10, 6, 9, 12), -65 * 16, 130 * 16);
                p.drawArc(QRectF(10, 2, 14, 20), -65 * 16, 130 * 16);
            }
        }
    }
};

class VideoSurface : public QWidget
{
  public:
    QString state = "playing";
    explicit VideoSurface(QWidget *parent = nullptr) : QWidget(parent)
    {
        setMinimumSize(160, 130);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

  protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), QColor("#172535"));
        if (state == "empty" || state == "error")
        {
            const QRectF box(0, height() / 2.0 - 68, width(), 140);
            Visuals::icon(p, QRectF(width() / 2.0 - 20, box.y(), 40, 40),
                          state == "error" ? "warning" : "document", QColor("#b6c7dd"));
            Visuals::text(p, QRectF(12, box.y() + 54, width() - 24, 32),
                          state == "error" ? "这个视频暂时无法播放" : "选择一个视频开始播放", 21,
                          Qt::white, true, Qt::AlignCenter);
            Visuals::text(p, QRectF(12, box.y() + 96, width() - 24, 24),
                          state == "error" ? "解码器未就绪 · 未自动切换或重试"
                                           : "支持列表由设备实际解码能力提供",
                          13, QColor("#b6c7dd"), false, Qt::AlignCenter);
            return;
        }
        // Deliberately drawn tutorial slate, not a decoded frame or a board photo.
        QSizeF size = QSizeF(960, 540);
        size.scale(this->size(), Qt::KeepAspectRatio);
        p.translate((width() - size.width()) / 2, (height() - size.height()) / 2);
        p.scale(size.width() / 960, size.height() / 540);
        p.fillRect(QRectF(0, 0, 960, 540), QColor("#e9edf0"));
        p.fillRect(QRectF(0, 0, 960, 70), QColor("#f8fafb"));
        Visuals::text(p, QRectF(30, 13, 700, 42), "连接 RGB50 显示屏", 28, QColor("#18324f"), true);
        Visuals::text(p, QRectF(724, 20, 215, 30), "接线示意 · 设计占位", 17, QColor("#61758b"));
        p.setPen(QPen(QColor("#c9d2da"), 2));
        p.setBrush(QColor("#dfe6ea"));
        p.drawRoundedRect(QRectF(525, 131, 352, 252), 7, 7);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#324b5c"));
        p.drawRect(QRectF(543, 149, 316, 211));
        p.setBrush(QColor("#d8e5ec"));
        p.drawRect(QRectF(556, 162, 290, 185));
        // Screen calibration bars make the illustrative scene immediately recognizable.
        const QList<QColor> colors = {QColor("#e5e9ed"), QColor("#dbbe62"), QColor("#74b2b4"),
                                      QColor("#75a891"), QColor("#a891b5"), QColor("#bf8981")};
        for (int i = 0; i < colors.size(); ++i)
            p.fillRect(QRectF(556 + i * 48.4, 162, 48.4, 130), colors[i]);
        p.fillRect(QRectF(556, 292, 290, 55), QColor("#18324f"));
        Visuals::text(p, QRectF(570, 302, 260, 30), "RGB50  /  1024 × 600", 17, Qt::white, false,
                      Qt::AlignCenter);
        p.setPen(QPen(QColor("#274838"), 3));
        p.setBrush(QColor("#3f725b"));
        p.drawRoundedRect(QRectF(87, 160, 292, 205), 10, 10);
        p.setBrush(QColor("#26323b"));
        p.drawRoundedRect(QRectF(180, 211, 78, 78), 2, 2);
        Visuals::text(p, QRectF(182, 234, 74, 30), "T113", 20, Qt::white, true, Qt::AlignCenter);
        p.setBrush(QColor("#b1b7b9"));
        p.setPen(Qt::NoPen);
        p.drawRect(QRectF(74, 185, 38, 55));
        p.drawRect(QRectF(74, 263, 38, 68));
        p.fillRect(QRectF(322, 198, 45, 120), QColor("#e3d7b5"));
        for (int i = 0; i < 15; ++i)
        {
            p.fillRect(QRectF(148 + i * 11, 177, 5, 15), QColor("#cab475"));
            p.fillRect(QRectF(148 + i * 11, 339, 5, 15), QColor("#cab475"));
        }
        for (QPointF point :
             {QPointF(101, 174), QPointF(365, 174), QPointF(101, 350), QPointF(365, 350)})
        {
            p.setPen(QPen(QColor("#d0bd72"), 3));
            p.setBrush(QColor("#e9edf0"));
            p.drawEllipse(point, 5, 5);
        }
        QPainterPath ribbon;
        ribbon.moveTo(366, 218);
        ribbon.cubicTo(453, 218, 429, 395, 567, 395);
        ribbon.lineTo(602, 395);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor("#d0b78f"), 28));
        p.drawPath(ribbon);
        p.setPen(QPen(QColor("#ad926d"), 1));
        for (int i = -8; i <= 8; i += 4)
        {
            p.save();
            p.translate(0, i);
            p.drawPath(ribbon);
            p.restore();
        }
        Visuals::text(p, QRectF(91, 393, 270, 35), "开发板", 20, QColor("#61758b"), false,
                      Qt::AlignCenter);
        Visuals::text(p, QRectF(603, 423, 200, 35), "RGB50 显示屏", 20, QColor("#61758b"), false,
                      Qt::AlignCenter);
        p.fillRect(QRectF(0, 486, 960, 54), QColor("#f8fafb"));
        Visuals::text(p, QRectF(28, 495, 910, 32), "连接前请断电，并核对排线方向与接口标记。", 21,
                      QColor("#18324f"), false, Qt::AlignCenter);
        if (state == "paused")
        {
            p.fillRect(QRectF(0, 0, 960, 540), QColor(13, 27, 45, 70));
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(13, 27, 45, 200));
            p.drawRoundedRect(QRectF(393, 222, 174, 84), 6, 6);
            Visuals::text(p, QRectF(393, 222, 174, 84), "已暂停", 27, Qt::white, true,
                          Qt::AlignCenter);
        }
    }
};
} // namespace

class PlayerPreview : public QWidget
{
  public:
    QString page = "playing";
    QString selected = "RGB50 接线演示.mp4";
    bool fullscreen = false, muted = false;
    int position = 96, volume = 60;
    QStackedWidget *body;
    QWidget *content = nullptr;
    QFrame *nav, *header, *footer;
    QScrollArea *navScroll;
    QLabel *feedback;
    QList<NavigationButton *> navButtons;
    VideoSurface *surface = nullptr;
    TouchSlider *seek = nullptr;
    ToolButton *play = nullptr;

    PlayerPreview()
    {
        Visuals::initializeFonts();
        resize(1024, 600);
        setWindowTitle("DshanControl 视频播放 · Qt设计预览");
        setObjectName("root");
        setStyleSheet(R"(
          * { font-family:"Dshan Sans"; font-size:14px; color:#092650; }
          QWidget#root,QStackedWidget,QWidget#surface { background:#f2f7fd; }
          QFrame#header,QFrame#footer { background:white; border-bottom:1px solid #d9e3ef; }
          QFrame#footer { border-top:1px solid #d9e3ef; }
          QFrame#nav { background:#e8f3ff; border-right:1px solid #d9e3ef; }
          QWidget#navSurface { background:#e8f3ff; }
          QFrame#panel { background:white; border:1px solid #d9e3ef; border-radius:5px; }
          QFrame#rule { background:#d9e3ef; }
          QLabel#brand { font-size:20px; font-weight:700; }
          QLabel#mark { background:#0062ff; color:white; font-size:23px; font-weight:700; border-radius:5px; }
          QLabel#title { font-size:25px; font-weight:700; }
          QLabel#panelTitle { font-size:17px; font-weight:700; }
          QLabel#muted,QLabel#feedback { font-size:13px; color:#61758b; }
          QLabel#notice { background:#eaf3ff; padding:10px; color:#395784; border-radius:4px; }
          QLabel#preview { background:#fff1d7; color:#935a09; padding:4px 8px; border-radius:4px; font-size:12px; }
          QPushButton { background:#0062ff; color:white; border:1px solid #0062ff; border-radius:4px; min-height:44px; padding:0 12px; }
          QPushButton[secondary="true"] { background:white; color:#0062ff; border-color:#bbcde4; }
          QPushButton:disabled { color:#8190a5; background:#edf1f6; border-color:#d9e3ef; }
          QPushButton:focus,QComboBox:focus { border:2px solid #0062ff; }
          QPushButton#navButton { background:transparent; border:1px solid transparent; padding:0 4px; }
          QPushButton#playlistItem { background:white; color:#092650; text-align:left; border:none; padding:4px 10px; min-height:44px; }
          QPushButton#playlistItem:checked { background:#edf4ff; color:#0062ff; border-left:3px solid #0062ff; }
          QComboBox { background:white; border:1px solid #bbcde4; border-radius:4px; min-height:42px; padding:0 10px; }
          QCheckBox { min-height:44px; }
          QSlider { min-height:44px; }
          QSlider::groove:horizontal { height:5px; background:#d9e3ef; border-radius:2px; }
          QSlider::sub-page:horizontal { background:#0062ff; }
          QSlider::handle:horizontal { background:white; border:2px solid #0062ff; width:22px; margin:-9px 0; border-radius:11px; }
          QScrollArea { border:none; background:transparent; }
          QScrollBar:vertical { width:10px; background:#edf3fb; }
          QScrollBar::handle:vertical { background:#a6bbd6; min-height:44px; }
        )");
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);
        header = new QFrame;
        header->setObjectName("header");
        header->setFixedHeight(40);
        auto *heading = new QHBoxLayout(header);
        heading->setContentsMargins(16, 0, 16, 0);
        auto *mark = text("D", "mark");
        mark->setFixedSize(27, 27);
        mark->setAlignment(Qt::AlignCenter);
        heading->addWidget(mark);
        heading->addWidget(text("DshanControl", "brand"));
        heading->addWidget(text("工业控制桌面", "muted"));
        heading->addStretch();
        heading->addWidget(text("界面预览", "preview"));
        root->addWidget(header);
        auto *row = new QHBoxLayout;
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(0);
        nav = new QFrame;
        nav->setObjectName("nav");
        nav->setFixedWidth(140);
        auto *navShell = new QVBoxLayout(nav);
        navShell->setContentsMargins(0, 0, 0, 0);
        navScroll = new QScrollArea;
        navScroll->setWidgetResizable(true);
        navScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto *navList = new QWidget;
        navList->setObjectName("navSurface");
        auto *navigation = new QVBoxLayout(navList);
        navigation->setContentsMargins(5, 8, 5, 8);
        navigation->setSpacing(3);
        const QStringList names = {"运行总览", "现场总线", "网络通信",   "设备监控", "告警记录",
                                   "系统设置", "云服务",   "家庭与对话", "视频播放"};
        const QStringList icons = {"bars",     "topology", "globe",  "monitor", "bell",
                                   "settings", "cloud",    "server", "monitor"};
        for (int i = 0; i < names.size(); ++i)
        {
            auto *b = new NavigationButton(names[i], icons[i]);
            b->setObjectName("navButton");
            b->setMinimumHeight(44);
            b->setCheckable(true);
            b->setChecked(i == 8);
            b->setAccessibleName(names[i]);
            navigation->addWidget(b);
            navButtons << b;
            connect(b, &QPushButton::clicked, this, [this, b]() {
                b->setChecked(b == navButtons.last());
                feedback->setText("这是播放器独立预览，其余桌面入口在正式桌面中保留。");
            });
        }
        navigation->addStretch();
        navScroll->setWidget(navList);
        navShell->addWidget(navScroll);
        row->addWidget(nav);
        body = new QStackedWidget;
        row->addWidget(body, 1);
        root->addLayout(row, 1);
        footer = new QFrame;
        footer->setObjectName("footer");
        footer->setFixedHeight(28);
        auto *foot = new QHBoxLayout(footer);
        foot->setContentsMargins(14, 0, 14, 0);
        feedback = text("Qt 实际渲染 · 示例画面与媒体，尚未调用板端解码器", "feedback");
        foot->addWidget(feedback);
        root->addWidget(footer);
        render();
    }

    void setPage(const QString &value)
    {
        page = value;
        render();
    }
    void render()
    {
        while (body->count())
        {
            auto *old = body->widget(0);
            body->removeWidget(old);
            old->deleteLater();
        }
        surface = nullptr;
        seek = nullptr;
        play = nullptr;
        content = new LayoutPage;
        content->setObjectName("surface");
        auto *layout = new PageLayout(content);
        layout->setContentsMargins(fullscreen ? 0 : 14, fullscreen ? 0 : 12, fullscreen ? 0 : 14,
                                   fullscreen ? 0 : 12);
        layout->setSpacing(10);
        nav->setVisible(!fullscreen);
        header->setVisible(!fullscreen);
        footer->setVisible(!fullscreen);
        if (!fullscreen)
        {
            auto *head = new QHBoxLayout;
            auto *titles = new QVBoxLayout;
            titles->setSpacing(2);
            titles->addWidget(
                text(page == "library" ? "本地视频" : page == "settings" ? "播放设置" : "视频播放",
                     "title"));
            titles->addWidget(text(page == "library"
                                       ? "按文件选择，随时继续观看"
                                       : page == "settings"
                                             ? "画面、声音与解码状态"
                                             : page == "empty" ? "从本地存储选择视频" : selected,
                                   "muted"));
            head->addLayout(titles, 1);
            const QList<QPair<QString, QString>> links = {
                {"正在播放", "playing"}, {"本地视频", "library"}, {"播放设置", "settings"}};
            for (const auto &link : links)
            {
                if (page == link.second ||
                    ((page == "paused" || page == "error" || page == "empty") &&
                     link.second == "playing"))
                    continue;
                auto *b = button(link.first);
                head->addWidget(b);
                connect(b, &QPushButton::clicked, this, [this, link]() { setPage(link.second); });
            }
            layout->addLayout(head);
        }
        if (page == "library")
            library(layout);
        else if (page == "settings")
            settings(layout);
        else
            player(layout);
        body->addWidget(content);
        body->setCurrentWidget(content);
        QTimer::singleShot(0, this,
                           [this]() { navScroll->ensureWidgetVisible(navButtons.last()); });
    }

    void player(QVBoxLayout *layout)
    {
        auto *columns = new QHBoxLayout;
        columns->setSpacing(12);
        layout->addLayout(columns, 1);
        QVBoxLayout *left;
        auto *video = panel(&left);
        if (fullscreen)
        {
            left->setContentsMargins(8, 8, 8, 8);
            video->setObjectName("fullscreenSurface");
        }
        left->setSpacing(4);
        columns->addWidget(video, 1);
        surface = new VideoSurface;
        surface->state = page;
        left->addWidget(surface, 1);
        auto *timeline = new QHBoxLayout;
        timeline->setSpacing(10);
        auto *current =
            text(page == "empty" || page == "error" ? "--:--" : timeText(position), "muted");
        current->setMinimumWidth(44);
        timeline->addWidget(current);
        seek = new TouchSlider;
        seek->setObjectName("videoSeek");
        seek->setRange(0, 312);
        seek->setValue(position);
        seek->setAccessibleName("播放进度");
        timeline->addWidget(seek, 1);
        timeline->addWidget(text(page == "empty" || page == "error" ? "--:--" : "05:12", "muted"));
        seek->setEnabled(page != "empty" && page != "error");
        connect(seek, &QSlider::valueChanged, this, [this, current](int value) {
            position = value;
            current->setText(timeText(value));
            feedback->setText("预览：拖动到 " + timeText(value) + "，没有执行真实解码。");
        });
        left->addLayout(timeline);
        auto *controls = new QHBoxLayout;
        controls->setSpacing(6);
        auto *previous = new ToolButton("上一个视频", "previous");
        play = new ToolButton(page == "paused" ? "播放" : "暂停",
                              page == "paused" ? "play" : "pause", true);
        play->setObjectName("videoPlayPause");
        auto *next = new ToolButton("下一个视频", "next");
        for (auto *b : {previous, play, next})
        {
            controls->addWidget(b);
            b->setEnabled(page != "empty" && page != "error");
        }
        connect(play, &QPushButton::clicked, this, [this]() {
            const bool wasPaused = page == "paused";
            page = wasPaused ? "playing" : "paused";
            surface->state = page;
            surface->update();
            play->glyph = wasPaused ? "pause" : "play";
            play->setAccessibleName(wasPaused ? "暂停" : "播放");
            play->setToolTip(play->accessibleName());
            play->update();
            feedback->setText(wasPaused ? "预览：继续播放" : "预览：已暂停");
        });
        connect(previous, &QPushButton::clicked, this, [this]() {
            selected = "接口与外设介绍.mp4";
            position = 0;
            setPage("paused");
        });
        connect(next, &QPushButton::clicked, this, [this]() {
            selected = "屏幕触摸演示.mp4";
            position = 0;
            setPage("paused");
        });
        controls->addStretch();
        auto *mute = new ToolButton("静音 / 恢复声音", muted ? "muted" : "volume");
        controls->addWidget(mute);
        auto *volumeSlider = new TouchSlider;
        volumeSlider->setRange(0, 100);
        volumeSlider->setValue(volume);
        volumeSlider->setFixedWidth(88);
        volumeSlider->setAccessibleName("播放音量");
        controls->addWidget(volumeSlider);
        connect(mute, &QPushButton::clicked, this, [this, mute]() {
            muted = !muted;
            mute->glyph = muted ? "muted" : "volume";
            mute->update();
            feedback->setText(muted ? "预览：已静音" : "预览：声音已恢复");
        });
        connect(volumeSlider, &QSlider::valueChanged, this, [this](int value) { volume = value; });
        auto *expand = new ToolButton(fullscreen ? "退出全屏" : "全屏播放", "fullscreen");
        expand->setObjectName("videoFullscreen");
        controls->addWidget(expand);
        connect(expand, &QPushButton::clicked, this, [this]() {
            fullscreen = !fullscreen;
            render();
        });
        left->addLayout(controls);
        if (fullscreen || width() < 900)
            return;
        QVBoxLayout *right;
        auto *sidebar = panel(&right);
        sidebar->setFixedWidth(width() < 900 ? 190 : 228);
        right->setSpacing(5);
        columns->addWidget(sidebar);
        right->addWidget(text("播放列表", "panelTitle"));
        right->addWidget(text("3 个示例视频", "muted"));
        rule(right);
        auto *group = new QButtonGroup(sidebar);
        group->setExclusive(true);
        const QStringList names = {"RGB50 接线演示.mp4", "接口与外设介绍.mp4", "屏幕触摸演示.mp4"};
        const QStringList durations = {"05:12", "03:46", "01:28"};
        for (int i = 0; i < names.size(); ++i)
        {
            auto *item = button(names[i].chopped(4) + "\n" + durations[i] + "  ·  本地示例");
            item->setObjectName("playlistItem");
            item->setCheckable(true);
            item->setChecked(selected == names[i]);
            group->addButton(item);
            right->addWidget(item);
            connect(item, &QPushButton::clicked, this, [this, names, i]() {
                selected = names[i];
                position = 0;
                setPage("paused");
            });
        }
        right->addStretch();
        rule(right);
        field(right, "视频编码", "H.264（示例）");
        field(right, "画面尺寸", "1280 × 720");
        field(right, "硬件解码", "待验证");
        auto *open = button("打开视频", false);
        right->addWidget(open);
        connect(open, &QPushButton::clicked, this, [this]() { setPage("library"); });
    }

    void library(QVBoxLayout *layout)
    {
        auto *columns = new QHBoxLayout;
        columns->setSpacing(12);
        layout->addLayout(columns, 1);
        QVBoxLayout *list;
        auto *files = panel(&list);
        columns->addWidget(files, 1);
        auto *head = new QHBoxLayout;
        head->addWidget(text("视频文件", "panelTitle"));
        head->addStretch();
        head->addWidget(text("示例列表 · 尚未扫描存储", "muted"));
        list->addLayout(head);
        rule(list);
        const QStringList names = {"RGB50 接线演示.mp4", "接口与外设介绍.mp4", "屏幕触摸演示.mp4",
                                   "测试片段.mkv"};
        const QStringList meta = {"05:12  ·  1280 × 720  ·  46.2 MB",
                                  "03:46  ·  1280 × 720  ·  32.8 MB",
                                  "01:28  ·  1024 × 600  ·  12.6 MB", "未读取媒体信息"};
        for (int i = 0; i < names.size(); ++i)
        {
            auto *row = new QHBoxLayout;
            auto *icon = text("");
            icon->setPixmap(Visuals::icon("document", QColor("#0062ff")).pixmap(28, 28));
            row->addWidget(icon);
            row->addSpacing(6);
            auto *info = new QVBoxLayout;
            info->setSpacing(3);
            info->addWidget(text(names[i], "panelTitle"));
            info->addWidget(text(meta[i], "muted"));
            row->addLayout(info, 1);
            auto *open = button("播放");
            row->addWidget(open);
            list->addLayout(row);
            rule(list);
            connect(open, &QPushButton::clicked, this, [this, names, i]() {
                selected = names[i];
                position = 0;
                setPage(i == 3 ? "error" : "paused");
            });
        }
        list->addStretch();
        auto *hint = text("文件名、大小和时长均为预览示例。正式列表读取本地存储。", "muted");
        hint->setWordWrap(true);
        list->addWidget(hint);
        QVBoxLayout *right;
        auto *sources = panel(&right);
        sources->setFixedWidth(width() < 900 ? 190 : 228);
        columns->addWidget(sources);
        right->addWidget(text("文件位置", "panelTitle"));
        rule(right);
        right->addWidget(text("本地存储"));
        right->addWidget(text("等待读取", "muted"));
        rule(right);
        right->addWidget(text("USB 存储"));
        right->addWidget(text("未检测 · 设计状态", "muted"));
        rule(right);
        right->addStretch();
        auto *note = text("只显示可访问的视频文件。移除存储后保留提示，停止读取。", "notice");
        note->setWordWrap(true);
        right->addWidget(note);
    }

    void settings(QVBoxLayout *layout)
    {
        auto *columns = new QHBoxLayout;
        columns->setSpacing(12);
        layout->addLayout(columns, 1);
        QVBoxLayout *left;
        auto *options = panel(&left);
        columns->addWidget(options, 1);
        left->addWidget(text("画面与声音", "panelTitle"));
        rule(left);
        left->addWidget(text("画面比例", "muted"));
        auto *aspect = new QComboBox;
        aspect->addItems({"保持原始比例", "适应显示区域"});
        left->addWidget(aspect);
        auto *loop = new ToggleSwitch("单个视频循环");
        left->addWidget(loop);
        rule(left);
        left->addWidget(text("播放音量", "muted"));
        auto *level = new TouchSlider;
        level->setRange(0, 100);
        level->setValue(volume);
        left->addWidget(level);
        connect(level, &QSlider::valueChanged, this, [this](int value) { volume = value; });
        left->addStretch();
        auto *note = text("这里的调整仅演示界面交互，尚未更改播放器或系统音频设置。", "notice");
        note->setWordWrap(true);
        left->addWidget(note);
        QVBoxLayout *right;
        auto *engine = panel(&right);
        columns->addWidget(engine, 1);
        right->addWidget(text("解码状态", "panelTitle"));
        rule(right);
        field(right, "播放服务", "尚未接入");
        field(right, "硬件解码", "待板端验证");
        field(right, "当前解码器", "未运行");
        field(right, "解码帧率", "--");
        field(right, "丢帧", "--");
        rule(right);
        auto *explanation =
            text("播放后显示实际编码格式、解码器和画面信息。不能解码时说明原因，避免反复加载。",
                 "muted");
        explanation->setWordWrap(true);
        right->addWidget(explanation);
        right->addStretch();
        auto *back = button("返回播放", false);
        right->addWidget(back);
        connect(back, &QPushButton::clicked, this, [this]() { setPage("playing"); });
    }

  protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QWidget::resizeEvent(event);
        const bool compact = width() < 900;
        nav->setFixedWidth(compact ? 68 : 140);
        const QStringList full = {"运行总览", "现场总线", "网络通信",   "设备监控", "告警记录",
                                  "系统设置", "云服务",   "家庭与对话", "视频播放"};
        const QStringList brief = {"总览", "总线", "网络", "监控", "告警",
                                   "设置", "云端", "家庭", "视频"};
        for (int i = 0; i < navButtons.size(); ++i)
            navButtons[i]->setText(compact ? brief[i] : full[i]);
        if (content)
            render();
    }
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    PlayerPreview preview;
    preview.show();
    app.processEvents();
    const auto args = app.arguments();
    if (!args.contains("--render"))
        return app.exec();
    const auto path = args.value(args.indexOf("--render") + 1);
    if (path.isEmpty() || !QDir().mkpath(path))
        return 2;
    // Native Qt interaction check: pause and seek must update this actual view.
    preview.play->click();
    if (preview.page != "paused" || preview.surface->state != "paused")
        return 3;
    preview.seek->setValue(121);
    if (preview.position != 121)
        return 4;
    preview.content->findChild<QPushButton *>("videoFullscreen")->click();
    if (!preview.fullscreen || preview.nav->isVisible())
        return 11;
    preview.content->findChild<QPushButton *>("videoFullscreen")->click();
    if (preview.fullscreen || !preview.nav->isVisible())
        return 12;
    preview.position = 96;
    QJsonArray manifest;
    struct Shot
    {
        QString name, page;
        QSize size;
        bool fullscreen;
    };
    const QList<Shot> shots = {{"01-playing", "playing", QSize(1024, 600), false},
                               {"02-library", "library", QSize(1024, 600), false},
                               {"03-settings", "settings", QSize(1024, 600), false},
                               {"04-error", "error", QSize(1024, 600), false},
                               {"05-empty", "empty", QSize(1024, 600), false},
                               {"06-paused", "paused", QSize(1024, 600), false},
                               {"07-fullscreen", "playing", QSize(1024, 600), true},
                               {"08-playing-800x480", "playing", QSize(800, 480), false},
                               {"09-playing-1280x720", "playing", QSize(1280, 720), false}};
    for (const auto &shot : shots)
    {
        preview.fullscreen = shot.fullscreen;
        preview.resize(shot.size);
        preview.setPage(shot.page);
        preview.feedback->setText("Qt 实际渲染 · 示例画面与媒体，尚未调用板端解码器");
        app.processEvents();
        app.sendPostedEvents(nullptr, QEvent::DeferredDelete);
        if (preview.size() != shot.size)
        {
            qWarning() << "Geometry mismatch" << shot.name << preview.size() << "requested"
                       << shot.size << "minimum" << preview.minimumSizeHint();
            return 5;
        }
        for (auto *widget : preview.findChildren<QPushButton *>())
        {
            if (widget->isVisible() && (widget->height() < 44 || widget->width() < 44))
                return 6;
            if (widget->isVisible() && preview.content->isAncestorOf(widget) &&
                !preview.rect().contains(
                    QRect(widget->mapTo(&preview, QPoint(0, 0)), widget->size())))
            {
                qWarning() << "Control overflow" << shot.name << widget->accessibleName();
                return 13;
            }
        }
        const QString file = shot.name + ".png";
        if (!preview.grab().save(path + "/" + file))
            return 7;
        manifest.append(QJsonObject{{"file", file},
                                    {"width", shot.size.width()},
                                    {"height", shot.size.height()},
                                    {"state", shot.page},
                                    {"synthetic_media", true},
                                    {"decoder_running", false}});
    }
    QImage sheet(2096, 1328, QImage::Format_RGB32);
    sheet.fill(QColor("#e8eef5"));
    QPainter p(&sheet);
    p.setFont(Visuals::font(21));
    p.setPen(QColor("#092650"));
    const QStringList names = {"01-playing", "02-library", "03-settings", "04-error"};
    const QStringList titles = {"视频播放", "本地视频", "播放设置", "播放失败状态"};
    for (int i = 0; i < names.size(); ++i)
    {
        int x = 16 + (i % 2) * 1040, y = 12 + (i / 2) * 656;
        QImage img(path + "/" + names[i] + ".png");
        if (img.size() != QSize(1024, 600))
            return 8;
        p.drawText(QRect(x, y, 1024, 28), Qt::AlignLeft | Qt::AlignVCenter, titles[i]);
        p.drawImage(x, y + 32, img);
    }
    p.end();
    if (!sheet.save(path + "/00-video-overview.png"))
        return 9;
    QFile output(path + "/render-manifest.json");
    if (!output.open(QIODevice::WriteOnly))
        return 10;
    output.write(QJsonDocument(QJsonObject{{"renderer", "native-qt-widgets"},
                                           {"qt_version", QT_VERSION_STR},
                                           {"interaction_pause_seek", "passed"},
                                           {"interaction_fullscreen_exit", "passed"},
                                           {"shots", manifest}})
                     .toJson());
    return 0;
}
