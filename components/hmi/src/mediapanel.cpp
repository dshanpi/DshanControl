#include "mediapanel.h"
#include "visuals.h"
#include "videofilepicker.h"
#include <QButtonGroup>
#include <QApplication>
#include <QComboBox>
#include <QDesktopWidget>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHeaderView>
#include <QHideEvent>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTimer>
#include <cmath>
#include <limits>

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
QPushButton *button(const QString &name, bool secondary = true)
{
    auto *w = new QPushButton(name);
    w->setProperty("kind", secondary ? "secondary" : "primary");
    return w;
}
QPushButton *tool(const QString &name, const QString &icon, bool primary = false)
{
    auto *w = button(name, !primary);
    w->setObjectName("mediaTool");
    w->setMinimumSize(84, 48);
    w->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    w->setIcon(Visuals::icon(icon, primary ? QColor(Qt::white) : Visuals::blue));
    w->setIconSize(QSize(24, 24));
    w->setAccessibleName(name);
    w->setToolTip(name);
    return w;
}
QFrame *panel(QVBoxLayout **layout)
{
    auto *w = new QFrame;
    w->setObjectName("panel");
    *layout = new PageLayout(w);
    (*layout)->setContentsMargins(12, 12, 12, 12);
    (*layout)->setSpacing(4);
    return w;
}
QString timeText(qint64 milliseconds)
{
    const auto seconds = qMax(qint64(0), milliseconds / 1000);
    return QString("%1:%2")
        .arg(seconds / 60, 2, 10, QChar('0'))
        .arg(seconds % 60, 2, 10, QChar('0'));
}
class VideoStage : public QWidget
{
  public:
    QString message = "选择一个视频开始播放", description = "从本地存储打开视频";
    QRectF videoRect;
    bool playingLayer = false;
    VideoStage()
    {
        setMinimumSize(160, 130);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

  protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(rect(), QColor("#172535"));
        if (playingLayer && !videoRect.isEmpty())
        {
            // Linuxfb preserves ARGB32 alpha. The Cedar layer is below the Qt
            // plane: clear only its clipped viewport, retaining opaque controls
            // and letterboxing. Paused/hidden/error states cover the plane again.
            p.setCompositionMode(QPainter::CompositionMode_Source);
            p.fillRect(videoRect, Qt::transparent);
            return;
        }
        if (message.isEmpty())
            return;
        Visuals::icon(p, QRectF(width() / 2.0 - 17, height() / 2.0 - 58, 34, 34), "monitor",
                      QColor("#b6c7dd"));
        Visuals::text(p, QRectF(8, height() / 2.0 - 10, width() - 16, 32), message, 20, Qt::white,
                      true, Qt::AlignCenter);
        Visuals::text(p, QRectF(8, height() / 2.0 + 30, width() - 16, 26), description, 13,
                      QColor("#b6c7dd"), false, Qt::AlignCenter);
    }
};
} // namespace
MediaPanel::MediaPanel(QWidget *parent) : QWidget(parent), session(this)
{
    setStyleSheet(
        "QPushButton#mediaTool{padding:0 8px;} QLabel#mediaTitle{font-size:22px;font-weight:600;} "
        "QPushButton#mediaSection:checked{background:#d0e2ff;color:#0043ce;border-bottom:3px solid #0f62fe;}"
        "QListWidget{border:none;background:white;} QListWidget::item{min-height:48px;padding:2px "
        "8px;} QListWidget::item:selected{background:#edf4ff;color:#0062ff;}");
    mediaDirectory = qEnvironmentVariable("DSHANCONTROL_MEDIA_DIRECTORY", "/mnt/UDISK/media");
    if (mediaDirectory.trimmed().isEmpty()) mediaDirectory = "/mnt/UDISK/media";
    auto *root = new PageLayout(this);
    root->setContentsMargins(14, 8, 14, 8);
    root->setSpacing(8);
    heading = new QWidget;
    auto *head = new QHBoxLayout(heading);
    head->setContentsMargins(0, 0, 0, 0);
    auto *labels = new QVBoxLayout;
    labels->setSpacing(2);
    title = label("视频播放", "mediaTitle");
    filename = label("从本地存储选择视频", "caption");
    labels->addWidget(title);
    labels->addWidget(filename);
    head->addLayout(labels, 1);
    auto *sections = new QButtonGroup(this);
    sections->setExclusive(true);
    for (int i = 0; i < 3; ++i)
    {
        auto *b = button(QStringList{"播放器", "本地视频", "播放设置"}[i]);
        b->setObjectName("mediaSection");
        b->setProperty("section", i);
        b->setCheckable(true);
        b->setChecked(i == 0);
        sections->addButton(b, i);
        head->addWidget(b);
        connect(b, &QPushButton::clicked, this, [this, i]() { showSection(i); });
    }
    auto *chooseVideo = button("选择视频", false);
    chooseVideo->setObjectName("chooseVideo");
    head->addWidget(chooseVideo);
    connect(chooseVideo, &QPushButton::clicked, this, &MediaPanel::choose);
    root->addWidget(heading);
    pages = new QStackedWidget;
    root->addWidget(pages, 1);
    auto *playing = new LayoutPage;
    auto *columns = new PageRowLayout(playing);
    columns->setContentsMargins(0, 0, 0, 0);
    columns->setSpacing(12);
    QVBoxLayout *video;
    columns->addWidget(panel(&video), 1);
    stage = new VideoStage;
    video->addWidget(stage, 1);
    auto *timeline = new QHBoxLayout;
    timeline->setSpacing(10);
    currentTime = label("--:--", "caption");
    duration = label("--:--", "caption");
    timeline->addWidget(currentTime);
    seek = new TouchSlider;
    seek->setObjectName("mediaSeek");
    seek->setRange(0, 0);
    seek->setAccessibleName("播放进度");
    timeline->addWidget(seek, 1);
    timeline->addWidget(duration);
    video->addLayout(timeline);
    connect(seek, &QSlider::sliderReleased, this, [this]() {
        if (state["seekable"] == true)
            session.command("seek", {{"position_ms", seek->value()}});
    });
    auto *controls = new QHBoxLayout;
    controls->setSpacing(6);
    previous = tool("上一个", "previous");
    play = tool("播放", "play", true);
    next = tool("下一个", "next");
    controls->addWidget(previous);
    controls->addWidget(play);
    controls->addWidget(next);
    controls->addStretch();
    mute = tool("静音", "volume");
    controls->addWidget(mute);
    volume = new TouchSlider;
    volume->setRange(0, 100);
    volume->setValue(60);
    volume->setFixedWidth(88);
    volume->setAccessibleName("播放音量");
    controls->addWidget(volume);
    fullscreen = tool("全屏", "fullscreen");
    controls->addWidget(fullscreen);
    video->addLayout(controls);
    connect(play, &QPushButton::clicked, this, [this]() {
        const auto value = state["state"].toString();
        if (selectionNeedsOpen && !selectedFile.isEmpty())
            open(selectedFile);
        else if (value == "playing" || value == "starting")
            session.command("pause");
        else if (value == "paused" || value == "prepared")
            session.command("play");
        else if (!selectedFile.isEmpty())
            open(selectedFile);
    });
    connect(previous, &QPushButton::clicked, this, [this]() {
        int i = mediaFiles.indexOf(selectedFile);
        if (i > 0)
            open(mediaFiles[i - 1]);
    });
    connect(next, &QPushButton::clicked, this, [this]() {
        int i = mediaFiles.indexOf(selectedFile);
        if (i >= 0 && i + 1 < mediaFiles.size())
            open(mediaFiles[i + 1]);
    });
    connect(mute, &QPushButton::clicked, this, [this]() {
        session.command("mute", {{"value", state["muted"] != true}});
    });
    connect(volume, &QSlider::sliderReleased, this, [this]() {
        session.command("volume", {{"value", volume->value()}});
    });
    connect(fullscreen, &QPushButton::clicked, this, [this]() {
        full = !full;
        fullscreen->setText(full ? "退出全屏" : "全屏");
        fullscreen->setAccessibleName(fullscreen->text());
        heading->setVisible(!full);
        side->setVisible(!full && width() >= 900);
        note->setVisible(!full);
        emit fullscreenRequested(full);
        geometryTimer->start();
    });
    QVBoxLayout *right;
    side = panel(&right);
    side->setFixedWidth(228);
    columns->addWidget(side);
    right->addWidget(label("播放列表", "panelTitle"));
    playlist = new QListWidget;
    playlist->setMinimumHeight(90);
    right->addWidget(playlist, 1);
    decoder = label("播放服务未连接", "caption");
    right->addWidget(decoder);
    format = label("编码：--", "caption");
    right->addWidget(format);
    dimensions = label("画面：--", "caption");
    right->addWidget(dimensions);
    auto *browse = button("选择其他视频");
    right->addWidget(browse);
    connect(browse, &QPushButton::clicked, this, &MediaPanel::choose);
    connect(playlist, &QListWidget::itemClicked, this,
            [this](QListWidgetItem *item) { selectFile(item->data(Qt::UserRole).toString()); });
    connect(playlist, &QListWidget::itemActivated, this,
            [this](QListWidgetItem *item) { selectFile(item->data(Qt::UserRole).toString()); });
    pages->addWidget(playing);
    auto *library = new LayoutPage;
    auto *libraryLayout = new PageLayout(library);
    libraryLayout->setContentsMargins(0, 0, 0, 0);
    libraryLayout->setSpacing(8);
    auto *libraryHead = new QHBoxLayout;
    libraryHead->addWidget(label("本地视频文件", "panelTitle"));
    libraryHead->addStretch();
    auto *refresh = button("刷新列表");
    libraryHead->addWidget(refresh);
    auto *browseFile = button("选择其他位置的视频", false);
    libraryHead->addWidget(browseFile);
    libraryLayout->addLayout(libraryHead);
    connect(refresh, &QPushButton::clicked, this, &MediaPanel::scan);
    connect(browseFile, &QPushButton::clicked, this, &MediaPanel::choose);
    files = new DataTable(3);
    files->setObjectName("mediaFiles");
    files->setHorizontalHeaderLabels({"文件名", "大小", "操作"});
    files->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    files->verticalHeader()->hide();
    files->verticalHeader()->setDefaultSectionSize(54);
    files->setMinimumSize(220, 130);
    files->setEditTriggers(QAbstractItemView::NoEditTriggers);
    libraryLayout->addWidget(files, 1);
    libraryLayout->addWidget(
        label("列表只读取可访问的媒体文件；能否播放取决于实际解码能力。", "caption"));
    pages->addWidget(library);
    auto *settings = new LayoutPage;
    auto *settingsLayout = new PageRowLayout(settings);
    settingsLayout->setContentsMargins(0, 0, 0, 0);
    settingsLayout->setSpacing(12);
    QVBoxLayout *options, *information;
    settingsLayout->addWidget(panel(&options), 1);
    settingsLayout->addWidget(panel(&information), 1);
    options->addWidget(label("画面与声音", "panelTitle"));
    options->addWidget(label("画面比例", "caption"));
    aspect = new QComboBox;
    aspect->addItems({"保持原始比例", "适应显示区域"});
    options->addWidget(aspect);
    connect(aspect, QOverload<int>::of(&QComboBox::activated), this,
            [this](int) { geometryTimer->start(); });
    loop = new ToggleSwitch("单个视频循环");
    options->addWidget(loop);
    connect(loop, &QCheckBox::toggled, this, [this](bool value) {
        if (!loading)
            session.command("loop", {{"value", value}});
    });
    options->addStretch();
    options->addWidget(label("离开播放页面会暂停并隐藏视频，返回后可继续播放。", "infoNote"));
    information->addWidget(label("解码状态", "panelTitle"));
    fps = label("当前解码器：--\n显示帧率：--\n硬件解码：待板端验证", "caption");
    information->addWidget(fps);
    information->addStretch();
    auto *back = button("返回播放", false);
    information->addWidget(back);
    connect(back, &QPushButton::clicked, this, [this]() { showSection(0); });
    pages->addWidget(settings);
    note = label("尚未选择视频。", "infoNote");
    root->addWidget(note);
    geometryTimer = new QTimer(this);
    geometryTimer->setSingleShot(true);
    geometryTimer->setInterval(75);
    connect(geometryTimer, &QTimer::timeout, this, &MediaPanel::geometry);
    connect(&session, &MediaSession::changed, this, &MediaPanel::apply);
    connect(&session, &MediaSession::feedback, this,
            [this](const QString &message) { lastFeedback = message; apply(state); });
    connect(qApp, &QApplication::applicationStateChanged, this, [this](Qt::ApplicationState value) {
        if (value != Qt::ApplicationActive && state["available"] == true)
            session.command("suspend");
        else if (value == Qt::ApplicationActive && isVisible() && pages->currentIndex() == 0)
            geometryTimer->start();
    });
    scan();
    apply(session.snapshot());
}
void MediaPanel::scan()
{
    mediaFiles.clear();
    playlist->clear();
    files->setRowCount(0);
    QDir directory(mediaDirectory);
    static_cast<DataTable *>(files)->setEmptyText(directory.exists()
        ? "这里还没有视频。点击“选择其他位置的视频”，浏览存储中的文件。"
        : "视频文件夹尚不可用。请检查存储，或点击“选择其他位置的视频”。");
    for (const auto &entry :
         directory.entryInfoList({"*.mp4", "*.mkv", "*.avi", "*.mov", "*.ts", "*.webm"},
                                 QDir::Files | QDir::Readable | QDir::NoSymLinks, QDir::Name))
    {
        if (mediaFiles.size() >= 200)
            break;
        const auto path = entry.absoluteFilePath();
        mediaFiles << path;
        auto *item = new QListWidgetItem(entry.fileName());
        item->setData(Qt::UserRole, path);
        playlist->addItem(item);
        const int row = files->rowCount();
        files->insertRow(row);
        files->setItem(row, 0, new QTableWidgetItem(entry.fileName()));
        files->setItem(
            row, 1,
            new QTableWidgetItem(QString::number(entry.size() / 1024.0 / 1024.0, 'f', 1) + " MB"));
        auto *b = button("选择");
        files->setCellWidget(row, 2, b);
        connect(b, &QPushButton::clicked, this, [this, path]() { selectFile(path); });
    }
}
void MediaPanel::choose()
{
    if (state["available"] == true)
        session.command("suspend");
    VideoFilePicker picker(this, mediaDirectory);
    const auto path = picker.exec() == QDialog::Accepted ? picker.selectedFile() : QString();
    if (!path.isEmpty())
        selectFile(QDir::cleanPath(path));
    else if (pages->currentIndex() == 0 && state["first_frame"] == true)
        session.command("visible", {{"value", true}});
}
bool MediaPanel::selectFile(const QString &path)
{
    QFileInfo file(path);
    if (!QDir::isAbsolutePath(path) || QDir::cleanPath(path) != path || !file.isFile() ||
        !file.isReadable() || file.isSymLink())
    {
        if (state["available"] == true)
            session.command("close");
        selectedFile.clear();
        selectionNeedsOpen = false;
        filename->setText("绑定的视频不可用");
        play->setEnabled(false);
        note->setText("绑定的视频不可用或存储已移除，未播放其他文件。");
        return false;
    }
    if (selectedFile != path)
    {
        selectionNeedsOpen = true;
        if (state["available"] == true)
            session.command("close");
        selectedFile = path;
        filename->setText(file.fileName());
    }
    showSection(0);
    session.start(true);
    lastFeedback = "已选择：" + file.fileName() + "。点击“播放”开始。";
    apply(session.snapshot());
    return true;
}
void MediaPanel::open(const QString &path)
{
    QFileInfo file(path);
    if (!file.isFile() || !file.isReadable() || file.isSymLink())
    {
        note->setText("文件不可用或存储已移除，请重新选择。");
        return;
    }
    selectedFile = file.absoluteFilePath();
    selectionNeedsOpen = false;
    filename->setText(file.fileName());
    showSection(0);
    session.start(true);
    geometry();
    session.command("open", {{"path", selectedFile}, {"volume", volume->value()}});
}
void MediaPanel::showSection(int index)
{
    if (index < 0 || index >= pages->count()) return;
    for (auto *tab : findChildren<QPushButton *>("mediaSection"))
        tab->setChecked(tab->property("section").toInt() == index);
    if (index != 0 && state["available"] == true)
        session.command("suspend");
    pages->setCurrentIndex(index);
    title->setText(QStringList{"视频播放", "本地视频", "播放设置"}.value(index));
    if (index == 1)
        scan();
    if (index == 0)
    {
        geometryTimer->start();
        if (state["first_frame"] == true)
            session.command("visible", {{"value", true}});
    }
}
void MediaPanel::apply(const QJsonObject &status)
{
    state = status;
    const auto name = status["state"].toString();
    const bool available = status["available"] == true;
    const bool active = available && !selectionNeedsOpen && (name == "playing" || name == "paused" || name == "starting" ||
                                      name == "completed" || name == "prepared");
    play->setEnabled(available && (active || !selectedFile.isEmpty()));
    play->setIcon(Visuals::icon(!selectionNeedsOpen && name == "playing" ? "pause" : "play", Qt::white));
    play->setText(!selectionNeedsOpen && (name == "playing" || name == "starting") ? "暂停" : "播放");
    play->setAccessibleName(play->text());
    play->setToolTip(play->text());
    const int i = mediaFiles.indexOf(selectedFile);
    previous->setEnabled(available && i > 0);
    next->setEnabled(available && i >= 0 && i + 1 < mediaFiles.size());
    seek->setEnabled(active && status["seekable"] == true && status["seeking"] != true);
    volume->setEnabled(active && status["audio_available"] == true);
    mute->setEnabled(volume->isEnabled());
    loop->setEnabled(active);
    if (!seek->isSliderDown())
    {
        QSignalBlocker block(seek);
        seek->setRange(0, qMin(std::numeric_limits<int>::max(), status["duration_ms"].toInt()));
        seek->setValue(status["position_ms"].toInt());
    }
    if (!volume->isSliderDown() && status["volume"].isDouble())
    {
        QSignalBlocker block(volume);
        volume->setValue(status["volume"].toInt());
    }
    loading = true;
    loop->setChecked(status["loop"] == true);
    loading = false;
    mute->setIcon(Visuals::icon("volume", Visuals::blue));
    mute->setText(status["muted"] == true ? "恢复声音" : "静音");
    mute->setToolTip(mute->text());
    mute->setAccessibleName(mute->text());
    const qint64 durationMs = qMax(qint64(0), qint64(status["duration_ms"].toDouble()));
    qint64 displayedPositionMs = qMax(qint64(0), qint64(status["position_ms"].toDouble()));
    // Cedar can report a position beyond the known end (including after its
    // completion callback). Match the bounded seek bar without rewriting the
    // backend snapshot or inferring completion from the displayed time.
    if (durationMs > 0)
        displayedPositionMs = qMin(displayedPositionMs, durationMs);
    currentTime->setText(active ? timeText(displayedPositionMs) : "--:--");
    duration->setText(active ? timeText(durationMs) : "--:--");
    decoder->setText(available ? "CedarX / TPlayer" : "播放组件未就绪");
    format->setText(status["codec_id"].isDouble()
                        ? "编码标识：" + QString::number(status["codec_id"].toInt())
                        : "编码：--");
    dimensions->setText(
        status["width"].toInt() > 0
            ? QString("画面：%1 × %2").arg(status["width"].toInt()).arg(status["height"].toInt())
            : "画面：--");
    fps->setText(QString("当前解码器：%1\n显示帧率：%2\n硬件解码：待板端验证")
                     .arg(available ? "CedarX / TPlayer" : "未运行")
                     .arg(Visuals::numberText(status["display_fps"], 1)));
    auto *surface = static_cast<VideoStage *>(stage);
    surface->playingLayer = !selectionNeedsOpen && available && status["backend"] == "cedar-tplayer" &&
                            name == "playing" && status["first_frame"] == true &&
                            status["visible"] == true;
    surface->message.clear();
    surface->description.clear();
    if (name == "not_installed")
    {
        surface->message = "播放器组件尚未安装";
        surface->description = "请安装匹配的桌面应用组件";
    }
    else if (name == "starting_service")
    {
        surface->message = "正在启动播放器";
        surface->description = "等待播放组件就绪";
    }
    else if (name == "error" && (available || status["backend"] != "unavailable"))
    {
        surface->message = available ? "这个视频暂时无法播放" : "播放服务暂不可用";
        surface->description = available
                                   ? "解码或显示未完成，请检查文件与播放状态"
                                   : "请重新选择视频后重试，不会自动重试";
    }
    else if (!available)
    {
        surface->message = "播放组件尚未就绪";
        surface->description = "当前构建或运行环境未提供解码能力";
    }
    else if (selectionNeedsOpen)
    {
        surface->message = "视频已选择";
        surface->description = "点击“播放”开始";
    }
    else if (name == "preparing" || name == "starting")
    {
        surface->message = "正在准备视频";
        surface->description = "等待解码器返回画面";
    }
    else if (name == "paused" || name == "prepared")
    {
        surface->message = "播放已暂停";
        surface->description = "点击播放继续，进度已保留";
    }
    else if (name == "completed")
    {
        surface->message = "播放结束";
        surface->description = "选择视频或重新播放";
    }
    else if (status["first_frame"] != true)
    {
        surface->message = "选择一个视频开始播放";
        surface->description = "从本地存储打开视频";
    }
    surface->update();
    note->setText(name == "error"
                      ? "播放未完成（" + status["error"].toString() + "）。没有自动重试。"
                      : !available
                            ? "播放组件尚未就绪，不能开始播放。"
                            : selectionNeedsOpen ? "视频已选择，等待点击“播放”。"
                            : name == "playing"
                                  ? "正在播放：" + QFileInfo(selectedFile).fileName()
                                  : name == "paused" ? "播放已暂停。"
                                                     : "选择视频后显示实际时长、画面和解码状态。");
    if (!lastFeedback.isEmpty())
        note->setText(note->text() + "\n最近操作：" + lastFeedback);
    if (status["width"].toInt() > 0)
        geometryTimer->start();
}
void MediaPanel::geometry()
{
    if (!isVisible() || pages->currentIndex() != 0)
        return;
    QRect area = stage->rect();
    if (aspect->currentIndex() == 0 && state["width"].toInt() > 0 && state["height"].toInt() > 0)
    {
        QSize size(state["width"].toInt(), state["height"].toInt());
        size.scale(area.size(), Qt::KeepAspectRatio);
        area = QRect(QPoint((area.width() - size.width()) / 2, (area.height() - size.height()) / 2),
                     size);
    }
    const QRect global(stage->mapToGlobal(area.topLeft()), area.size());
    const QRect clipped = global.intersected(QApplication::desktop()->screenGeometry(this))
                              .intersected(window()->frameGeometry());
    const qreal scale = stage->devicePixelRatioF();
    const int x = (qMax(0, int(std::ceil(clipped.x() * scale))) + 1) & ~1;
    const int y = (qMax(0, int(std::ceil(clipped.y() * scale))) + 1) & ~1;
    const int right = int(std::floor((clipped.x() + clipped.width()) * scale)) & ~1;
    const int bottom = int(std::floor((clipped.y() + clipped.height()) * scale)) & ~1;
    const int w = right - x, h = bottom - y;
    auto *surface = static_cast<VideoStage *>(stage);
    surface->videoRect = QRectF();
    if (clipped.isEmpty() || w < 8 || h < 8)
    {
        surface->update();
        if (state["available"] == true)
            session.command("suspend");
        return;
    }
    const QPoint origin = stage->mapToGlobal(QPoint(0, 0));
    surface->videoRect =
        QRectF(x / scale - origin.x(), y / scale - origin.y(), w / scale, h / scale);
    surface->update();
    if (session.snapshot()["state"] != "not_installed")
    {
        session.command("rect", {{"x", x}, {"y", y}, {"width", w}, {"height", h}});
        if ((state["first_frame"] == true || state["state"] == "prepared") &&
            state["visible"] != true)
            session.command("visible", {{"value", true}});
    }
}
void MediaPanel::hideEvent(QHideEvent *event)
{
    // Suspend also cancels autostart while an asynchronous prepare is pending.
    // Keep the decoder's position; closing the whole application retires it.
    if (state["available"] == true || state["state"] == "starting_service")
        session.command("suspend");
    if (full)
    {
        full = false;
        fullscreen->setText("全屏");
        fullscreen->setAccessibleName(fullscreen->text());
        heading->show();
        note->show();
        emit fullscreenRequested(false);
    }
    QWidget::hideEvent(event);
}
void MediaPanel::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    session.start();
    geometryTimer->start();
}
void MediaPanel::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    side->setVisible(!full && width() >= 900);
    if (geometryTimer)
        geometryTimer->start();
}
