#ifdef DSHANCONTROL_HMI_UI_TEST
#include <QNetworkProxy>
#endif
#include "oneshotupload.h"
#include "mainwindow.h"
#include <QSignalBlocker>
#include "touchfiledialog.h"
#include "displayprobe.h"
#include "updatepanel.h"
#include "visuals.h"
#include "cloudpanel.h"
#include "familypanel.h"
#include "mediapanel.h"
#include "desktop/desktoptrendpanel.h"

#include <QAbstractItemView>
#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHeaderView>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QMessageBox>
#include <QNetworkReply>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QResizeEvent>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScreen>
#include <QWindow>
#include <QScrollArea>
#include <QScrollBar>
#include <QScroller>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QtMath>
#include <cmath>

namespace
{
const char *kBaseUrl = "http://127.0.0.1/api/hmi/v1";
const QColor kGrid("#d9e3ef");
const QColor kText("#18324f");
const QColor kMuted("#61758b");
const QColor kSuccess("#168366");
const QColor kWarning("#a3650c");
const QColor kDanger("#cf4555");

QStringList controlDevices(const QJsonObject &snapshot, const QString &kind, const QString &role)
{
    const auto platform = snapshot["platform"].toObject();
    if (!platform["capabilities"].toArray().contains(role))
        return {};
    QStringList devices;
    const auto channels = platform["channels"].toObject();
    for (const auto &value : channels) {
        const auto channel = value.toObject();
        const QString device = channel["device"].toString();
        if (channel["kind"].toString() == kind && channel["role"].toString() == role &&
            !device.trimmed().isEmpty())
            devices << device;
    }
    return devices;
}

QJsonObject cellularMapping(const QJsonObject &snapshot, const QString &profile)
{
    const auto platform = snapshot["platform"].toObject();
    const auto mapping = snapshot["cellular"].toObject()[profile].toObject();
    const QString device = mapping["device"].toString();
    const QString interface = mapping["interface"].toString();
    if (!platform["capabilities"].toArray().contains("dual-cellular") ||
        device.trimmed().isEmpty() || interface.trimmed().isEmpty())
        return {};
    const auto channels = platform["channels"].toObject();
    for (const auto &value : channels) {
        const auto channel = value.toObject();
        const auto role = channel["role"].toString();
        if (channel["kind"].toString() == "cellular" && channel["device"].toString() == device &&
            (role == "wan" || role == "wan-backup"))
            return {{"device", device}, {"interface", interface}};
    }
    return {};
}

QLabel *titleLabel(const QString &text)
{
    QLabel *label = new QLabel(text);
    label->setObjectName("sectionTitle");
    return label;
}

QLabel *captionLabel(const QString &text)
{
    QLabel *label = new QLabel(text);
    label->setObjectName("caption");
    label->setWordWrap(true);
    return label;
}

QWidget *iconLabel(const QString &symbol, const QString &title)
{
    auto *label = new QWidget;
    auto *row = new QHBoxLayout(label);
    row->setContentsMargins(0, 0, 6, 0);
    row->setSpacing(10);
    auto *icon = new QLabel;
    icon->setPixmap(Visuals::icon(symbol).pixmap(24, 24));
    row->addWidget(icon);
    row->addWidget(new QLabel(title));
    return label;
}

QTableWidgetItem *cell(const QString &text, const QColor &color = kText)
{
    QTableWidgetItem *item = new QTableWidgetItem(text);
    item->setForeground(color);
    item->setToolTip(text);
    item->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    return item;
}

QLineEdit *edit(const QString &value, const QString &placeholder = QString())
{
    QLineEdit *e = new QLineEdit(value);
    e->setPlaceholderText(placeholder);
    e->setMinimumHeight(44);
    e->setMinimumWidth(60);
    return e;
}

} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
#ifdef DSHANCONTROL_HMI_UI_TEST
    // Qt bypasses an application-wide proxy for loopback destinations. The
    // UI test executable explicitly binds its rejecting/fixture proxy before
    // the first request, so running tests beside a live board cannot control it.
    networkManager.setProxy(QNetworkProxy::applicationProxy());
#endif
    resize(1024, 600);
    setWindowTitle("DshanControl 工业控制桌面");
    Visuals::initializeFonts();
    new TouchFiles::Translation(this);
    applyTheme();

    QWidget *root = new QWidget;
    QVBoxLayout *rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    QFrame *header = new QFrame;
    header->setObjectName("header");
    header->setFixedHeight(40);
    QHBoxLayout *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(16, 0, 16, 0);
    headerLayout->setSpacing(10);
    QLabel *mark = new QLabel("D");
    mark->setObjectName("brandMark");
    mark->setFixedSize(27, 27);
    mark->setAlignment(Qt::AlignCenter);
    headerLayout->addWidget(mark);
    QLabel *brand = new QLabel("DshanControl");
    brand->setObjectName("brand");
    headerLayout->addWidget(brand);
    QLabel *brandSubtitle = new QLabel("工业控制桌面");
    brandSubtitle->setObjectName("brandSubtitle");
    headerLayout->addWidget(brandSubtitle);
    headerLayout->addStretch();
    demoBadge = new QLabel("演示数据");
    demoBadge->setObjectName("demoBadge");
    demoBadge->hide();
    headerLayout->addWidget(demoBadge);
    connectionLabel = new QLabel("● 正在连接");
    connectionLabel->setObjectName("connection");
    headerLayout->addWidget(connectionLabel);
    clockLabel = new QLabel;
    clockLabel->setObjectName("clock");
    headerLayout->addWidget(clockLabel);
    rootLayout->addWidget(header);

    QWidget *body = new QWidget;
    QHBoxLayout *bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);
    nav = new QFrame;
    nav->setObjectName("nav");
    nav->setFixedWidth(140);
    auto *navShell = new QVBoxLayout(nav);
    navShell->setContentsMargins(0, 0, 0, 0);
    auto *navScroll = new QScrollArea;
    navScroll->setObjectName("desktopNavigation");
    navScroll->setWidgetResizable(true);
    navScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *navSurface = new QWidget;
    navSurface->setObjectName("navSurface");
    auto *navLayout = new QVBoxLayout(navSurface);
    navLayout->setContentsMargins(5, 8, 5, 8);
    navLayout->setSpacing(3);
    const QStringList names = {"运行总览", "现场总线", "网络通信", "设备监控", "告警记录", "系统设置", "云服务", "家庭与对话", "视频播放"};
    const QStringList symbols = {"bars", "topology", "globe", "monitor", "bell", "settings", "cloud", "server", "video"};
    for (int i = 0; i < names.size(); ++i) {
        QPushButton *button = new NavigationButton(names[i], symbols[i]);
        button->setObjectName("navButton");
        button->setAccessibleName(names[i]);
        button->setToolTip(names[i]);
        button->setCheckable(true);
        button->setMinimumHeight(44);
        connect(button, &QPushButton::clicked, this, [this, i]() { showPage(i); });
        navButtons << button;
        navLayout->addWidget(button);
    }
    navLayout->addStretch();
    navModel = new QLabel("▣   工程预览");
    navModel->setObjectName("navModel");
    navModel->setWordWrap(true);
    navLayout->addWidget(navModel);
    navModel->hide();
    navScroll->setWidget(navSurface);
    navShell->addWidget(navScroll);
    bodyLayout->addWidget(nav);
    pages = new QStackedWidget;
    pages->setObjectName("pages");
    for (QWidget *page :
         {buildOverview(), buildFieldbus(), buildNetwork(), buildDevices(), buildAlarms(), buildSettings()}) {
        pages->addWidget(scrollPage(page));
    }
    desktopClient = new DesktopClient(this);
    connect(desktopClient, &DesktopClient::snapshotChanged, this, &MainWindow::desktopSnapshotChanged);
    cloudPanel = new CloudPanel(desktopClient);
    familyPanel = new FamilyPanel(desktopClient);
    mediaPanel = new MediaPanel;
    for (QWidget *page : {static_cast<QWidget *>(cloudPanel), static_cast<QWidget *>(familyPanel), static_cast<QWidget *>(mediaPanel)})
        pages->addWidget(scrollPage(page));
    connect(mediaPanel, &MediaPanel::fullscreenRequested, this, [this, header](bool enabled) {
        nav->setVisible(!enabled && !desktopEmbedded);
        header->setVisible(!enabled && !desktopEmbedded);
        if (auto *footer = findChild<QFrame *>("footer")) footer->setVisible(!enabled && !desktopEmbedded);
        emit mediaFullscreenChanged(enabled);
    });
    bodyLayout->addWidget(pages, 1);
    rootLayout->addWidget(body, 1);

    QFrame *footer = new QFrame;
    footer->setObjectName("footer");
    footer->setFixedHeight(32);
    QHBoxLayout *footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(14, 0, 14, 0);
    cpuValue = new QLabel("CPU --");
    memoryValue = new QLabel("内存 --");
    temperatureValue = new QLabel("温度 --");
    storageValue = new QLabel("存储 --");
    for (QLabel *label : {cpuValue, memoryValue, storageValue}) {
        label->setObjectName("headerMetric");
        footerLayout->addWidget(label);
        auto *separator = new QLabel("│");
        separator->setObjectName("caption");
        footerLayout->addWidget(separator);
    }
    temperatureValue->hide();
    refreshValue = new QLabel("数据刷新  --");
    refreshValue->setObjectName("headerMetric");
    footerLayout->addWidget(refreshValue);
    footerLayout->addStretch();
    alarmSummaryLabel = new QLabel("活动告警 --");
    footerLayout->addWidget(alarmSummaryLabel);
    rootLayout->addWidget(footer);
    setCentralWidget(root);
    for (const auto &control : capabilityControls)
        control.first->setEnabled(false);

    connect(&networkManager, &QNetworkAccessManager::finished, this, &MainWindow::handleReply);
    connect(&refreshTimer, &QTimer::timeout, this, &MainWindow::requestSnapshot);
    connect(&pageTimer, &QTimer::timeout, this,
            [this]() { showPage((pages->currentIndex() + 1) % pages->count()); });
    refreshTimer.start(1000);
    showPage(0);
    requestSnapshot();
}

void MainWindow::applyTheme()
{
    setStyleSheet(R"(
        * { font-family: "Dshan Sans"; font-size: 14px; color: #161616; }
        QMainWindow, QWidget#pages, QStackedWidget, QWidget#pageSurface { background: #f4f4f4; }
        QFrame#header, QFrame#footer { background: white; border-bottom: 1px solid #d0d0d0; }
        QFrame#footer { border-top: 1px solid #d0d0d0; }
        QFrame#nav { background: #f4f4f4; border-right: 1px solid #d0d0d0; }
        QWidget#navSurface { background: #f4f4f4; }
        QLabel#brandMark { color: white; background: #0f62fe; border-radius: 5px; font-size: 23px; font-weight: 700; }
        QLabel#brand { font-size: 20px; font-weight: 700; }
        QLabel#brandSubtitle { font-size: 13px; color: #193960; }
        QLabel#clock, QLabel#headerMetric, QLabel#navModel { color: #525252; font-size: 12px; }
        QLabel#navModel { padding: 8px 2px; border-top: 1px solid #d0d0d0; }
        QLabel#infoNote { color: #0f62fe; background: #e9f4ff; border-radius: 4px; padding: 8px 12px; font-size: 14px; }
        QLabel#warningNote { color: #b86c00; background: #fff7e7; border: 1px solid #f4d9a3; border-radius: 4px; padding: 8px 12px; font-size: 14px; }
        QLabel#demoBadge { color: #935a09; background: #fff1d7; padding: 4px 8px; border-radius: 4px; }
        QLabel#sectionTitle { font-size: 22px; font-weight: 700; }
        QLabel#caption, QLabel#nodeState, QLabel#gatewaySub, QLabel#statusName { color: #525252; font-size: 13px; }
        QLabel#panelTitle, QLabel#nodeName { font-size: 17px; font-weight: 700; }
        QLabel#statusValue, QLabel#gatewayTitle { font-size: 18px; font-weight: 700; }
        QPushButton { min-height: 46px; border-radius: 2px; padding: 0 12px;
                      color: white; background: #0f62fe; border: 1px solid #0f62fe; }
        QPushButton:pressed { background: #0d46a0; }
        QPushButton:focus, QLineEdit:focus, QComboBox:focus, QSpinBox:focus { border: 2px solid #0f62fe; }
        QPushButton:disabled { color: #718096; background: #e8edf4; border-color: #d0d0d0; }
        QPushButton[kind="secondary"] { color: #0f62fe; background: white; border-color: #a8a8a8; }
        QPushButton[kind="danger"] { color: #b52f40; background: #fff4f5; border-color: #e9b9c0; }
        QPushButton[kind="secondary"]:disabled, QPushButton[kind="danger"]:disabled { color: #8190a5; background: #e8edf4; border-color: #d0d0d0; }
        QPushButton#navButton { color: #385371; background: transparent; border: 1px solid transparent; padding: 0 4px; }
        QPushButton#navButton:checked { background: #0f62fe; color: white; font-weight: 700; }
        QPushButton#navButton:focus { border: 2px solid #719ce0; }
        QFrame#card, QFrame#panel, QGroupBox { background: white; border: 1px solid #d0d0d0; border-radius: 5px; }
        QFrame#gateway { background: #eaf2ff; border: 1px solid #8db5ef; border-radius: 5px; }
        QFrame#ioNode { background: #f7faff; border: 1px solid #d0d0d0; border-left: 3px solid #0f62fe; border-radius: 4px; }
        QFrame#rule { background: #d0d0d0; border: none; }
        QGroupBox { margin-top: 0; padding: 34px 12px 12px 12px; font-weight: 700; font-size: 16px; }
        QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; top: 10px; left: 12px; padding: 0; }
        QTableWidget { background: white; alternate-background-color: #f5f8fd; border: 1px solid #d0d0d0;
                       gridline-color: #e6edf5; selection-background-color: #dbeaff; selection-color: #123c70; }
        QHeaderView::section { background: #e8e8e8; color: #49627d; border: none;
                               border-bottom: 1px solid #d0d0d0; min-height: 24px; padding: 4px; }
        QTableWidget::item { padding: 5px; }
        QLabel#valueText { color: #0f62fe; font-weight: 700; }
        QLineEdit:disabled, QComboBox:disabled, QSpinBox:disabled { background: #e8e8e8; color: #8197b2; border-color: #d0d0d0; }
        QLineEdit, QComboBox, QSpinBox { background: white; border: 1px solid #a8a8a8; border-radius: 4px;
                                        min-height: 42px; padding: 0 8px; }
        QComboBox::drop-down { width: 28px; border-left: 1px solid #d0d0d0; }
        QSpinBox::up-button { subcontrol-origin: border; subcontrol-position: top right; width: 25px; height: 17px; border-left: 1px solid #d0d0d0; }
        QSpinBox::down-button { subcontrol-origin: border; subcontrol-position: bottom right; width: 25px; height: 17px; border-left: 1px solid #d0d0d0; }
        QSpinBox::up-arrow { image: url(:/hmi/icons/up.xpm); width: 9px; height: 5px; }
        QSpinBox::down-arrow { image: url(:/hmi/icons/down.xpm); width: 9px; height: 5px; }
        QComboBox::down-arrow { image: url(:/hmi/icons/down.xpm); width: 9px; height: 5px; }
        QComboBox QAbstractItemView { background: white; selection-background-color: #dbeaff; selection-color: #161616; }
        QTabWidget::pane { border: 1px solid #d0d0d0; background: white; border-radius: 4px; }
        QTabBar::tab { background: white; color: #525252; min-height: 42px; padding: 0 22px; border-bottom: 1px solid #d0d0d0; }
        QTabBar::tab:selected { color: #0f62fe; background: white; border-bottom: 3px solid #0f62fe; font-weight: 700; }
        QTabBar::tab:focus { border: 1px solid #0f62fe; }
        QCheckBox { spacing: 8px; min-height: 44px; }
        QCheckBox::indicator { width: 24px; height: 24px; }
        QSlider { min-height: 48px; }
        QSlider::groove:horizontal { height: 6px; background: #d0d0d0; border-radius: 3px; }
        QSlider::handle:horizontal { width: 28px; margin: -12px 0; background: white; border: 2px solid #0f62fe; border-radius: 15px; }
        QSlider::sub-page:horizontal { background: #0f62fe; }
        QScrollArea { border: none; background: #f4f4f4; }
        QScrollBar:vertical { background: #e8e8e8; width: 14px; margin: 0; }
        QScrollBar::handle:vertical { background: #a6bbd6; min-height: 44px; border-radius: 5px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QToolTip { background: white; color: #161616; border: 1px solid #a8a8a8; }
    )");
}

QWidget *MainWindow::scrollPage(QWidget *page)
{
    QScrollArea *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    if (page->objectName().isEmpty())
        page->setObjectName(qobject_cast<QGroupBox *>(page) ? "formPanel" : "pageSurface");
    scroll->setWidget(page);
    QScroller::grabGesture(scroll->viewport(), QScroller::TouchGesture);
    return scroll;
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    // Embedded applications occupy only the area between the desktop bars.
    // Report the physical display, not this page's smaller content rectangle.
    const auto *handle = window()->windowHandle();
    const auto *screen = handle ? handle->screen() : QGuiApplication::primaryScreen();
    if (screen)
    {
        const QSize pixels = screen->size() * screen->devicePixelRatio();
        screenResolution->setText(QString("%1 × %2（已检测）")
                                      .arg(pixels.width()).arg(pixels.height()));
    }
    else
        screenResolution->setText("尚未检测到屏幕");
    const bool compact = width() < 900;
    const bool narrow = width() < 700;
    nav->setFixedWidth(compact ? 68 : 140);
    const QStringList full = {"运行总览", "现场总线", "网络通信", "设备监控", "告警记录", "系统设置", "云服务", "家庭与对话", "视频播放"};
    const QStringList shortNames = {"总览", "总线", "网络", "监控", "告警", "设置", "云端", "家庭", "视频"};
    for (int i = 0; i < navButtons.size(); ++i)
        navButtons[i]->setText(compact ? shortNames[i] : full[i]);
    navModel->setVisible(!compact);
    if (auto *label = findChild<QLabel *>("brandSubtitle"))
        label->setVisible(!compact);
    clockLabel->setVisible(!narrow);
    temperatureValue->hide();
    storageValue->setVisible(!narrow);
    for (QBoxLayout *layout : responsiveRows)
        layout->setDirection(narrow ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
}

QTableWidget *MainWindow::makeTable(const QStringList &headers)
{
    QTableWidget *table = new DataTable(headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->hide();
    table->setAlternatingRowColors(true);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setMinimumSize(0, 130);
    table->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
    table->horizontalHeader()->setMinimumSectionSize(60);
    QScroller::grabGesture(table->viewport(), QScroller::TouchGesture);
    table->verticalHeader()->setDefaultSectionSize(44);
    return table;
}

QPushButton *MainWindow::actionButton(const QString &text, const QString &kind)
{
    QPushButton *button = new QPushButton(text);
    button->setProperty("kind", kind);
    return button;
}

QWidget *MainWindow::buildOverview()
{
    dashboard = new OverviewDashboard;
    connect(dashboard, &OverviewDashboard::alarmsRequested, this, [this]() { navigateApplication("alarms"); });
    return dashboard;
}

QWidget *MainWindow::buildFieldbus()
{
    QWidget *page = new LayoutPage;
    QVBoxLayout *layout = new PageLayout(page);
    layout->setContentsMargins(14, 6, 14, 6);
    QTabWidget *tabs = new SectionTabs;

    QWidget *canPage = new LayoutPage;
    auto *canRoot = new PageLayout(canPage);
    canRoot->setContentsMargins(0,0,0,0);
    auto *canContent = new LayoutPage;
    QVBoxLayout *canLayout = new PageLayout(canContent);
    canLayout->setContentsMargins(0, 8, 0, 0);
    busTable = makeTable({"接口", "链路", "波特率", "数据来源"});
    auto *interfacePanel = new QGroupBox("接口状态");
    interfacePanel->setStyleSheet("QGroupBox { padding-top: 26px; padding-bottom: 0; } QGroupBox::title { top: 4px; }");
    auto *interfaceLayout = new QVBoxLayout(interfacePanel);
    interfaceLayout->setContentsMargins(0, 0, 0, 0);
    interfaceLayout->addWidget(busTable);
    canLayout->addWidget(interfacePanel);
    busTable->setMinimumHeight(124);
    busTable->setMaximumHeight(124);
    busTable->setMinimumWidth(250);
    QGroupBox *canCfg = new QGroupBox("CAN 参数");
    QFormLayout *canCfgLayout = new QFormLayout(canCfg);
    canCfgLayout->setContentsMargins(0, 0, 0, 0);
    canCfgLayout->setVerticalSpacing(4);
    canCfg->setStyleSheet(
        "QGroupBox { padding-top: 30px; padding-bottom: 0; } QGroupBox::title { top: 6px; }");
    canCfgLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    QComboBox *canIf = new QComboBox;
    canInterfaces << canIf;
    QLineEdit *bitrate = edit("500000");
    QLineEdit *dbitrate = edit("2000000");
    QCheckBox *fd = new ToggleSwitch;
    canFdCheck = fd;
    capabilityControls << qMakePair<QWidget *, QString>(fd, "can-fd");
    QPushButton *applyCan = actionButton("应用配置");
    bitrate->setObjectName("canBitrate");
    dbitrate->setObjectName("canDataBitrate");
    fd->setObjectName("canFd");
    applyCan->setObjectName("canApply");
    auto *canInterfaceRow = new QHBoxLayout;
    canInterfaceRow->addWidget(canIf, 1);
    fd->setText("CAN-FD");
    fd->setMinimumWidth(108);
    canInterfaceRow->addWidget(fd);
    canCfgLayout->addRow("接口", canInterfaceRow);
    canCfgLayout->addRow("仲裁波特率", bitrate);
    canCfgLayout->addRow("数据波特率", dbitrate);
    dbitrate->setEnabled(false);
    connect(fd, &QCheckBox::toggled, dbitrate, &QWidget::setEnabled);
    connect(applyCan, &QPushButton::clicked, this, [=]() {
        bool rateOk = false, dataOk = false;
        const int rate = bitrate->text().toInt(&rateOk);
        const int dataRate = dbitrate->text().toInt(&dataOk);
        if (!rateOk || rate < 10000 || rate > 1000000 ||
            (fd->isChecked() && (!dataOk || dataRate < rate || dataRate > 8000000))) {
            QMessageBox::warning(this, "参数错误", "仲裁波特率须为 10000..1000000；CAN-FD 数据波特率须不低于仲裁波特率且不超过 8000000");
            return;
        }
        QJsonObject p{{"interface", canIf->currentText()},
                      {"bitrate", rate},
                      {"fd", fd->isChecked()},
                      {"restart_ms", 100}};
        if (fd->isChecked())
            p["data_bitrate"] = dataRate;
        postAction("can_apply", p, "确认重新配置 " + canIf->currentText() + "？接口会短暂中断。");
    });
    QTabWidget *canControls = new SectionTabs;
    canControls->setObjectName("canControls");
    QWidget *linkPage = new QWidget;
    QVBoxLayout *linkLayout = new QVBoxLayout(linkPage);
    linkLayout->setContentsMargins(0, 0, 0, 0);
    linkLayout->addWidget(canCfg, 1);

    auto *co = new QFrame;
    co->setObjectName("panel");
    QFormLayout *coGrid = new QFormLayout(co);
    coGrid->setContentsMargins(12, 4, 12, 4);
    coGrid->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    QComboBox *coIf = new QComboBox;
    canInterfaces << coIf;
    QSpinBox *node = new QSpinBox;
    node->setRange(0, 127);
    node->setValue(1);
    QComboBox *nmt = new QComboBox;
    nmt->addItems({"OPERATIONAL", "PRE-OPERATIONAL", "STOPPED", "RESET"});
    QPushButton *sendNmt = actionButton("发送 NMT", "danger");
    coGrid->addRow("接口", coIf);
    coGrid->addRow("节点", node);
    coGrid->addRow("目标状态", nmt);
    QLineEdit *coIndex = edit("0x2000");
    QSpinBox *coSub = new QSpinBox;
    coSub->setRange(0, 255);
    QLineEdit *coValue = edit("0");
    QPushButton *sdoWrite = actionButton("SDO 写入", "danger");
    sendNmt->setObjectName("canNmtSend");
    node->setObjectName("canNmtNode");
    nmt->setObjectName("canNmtState");
    coIndex->setObjectName("canSdoIndex");
    coSub->setObjectName("canSdoSubindex");
    coValue->setObjectName("canSdoValue");
    sdoWrite->setObjectName("canSdoWrite");
    auto *sdo = new QFrame;
    sdo->setObjectName("panel");
    auto *sdoForm = new QGridLayout(sdo);
    sdoForm->setContentsMargins(12, 4, 12, 4);
    QComboBox *sdoIf = new QComboBox;
    canInterfaces << sdoIf;
    QSpinBox *sdoNode = new QSpinBox;
    sdoNode->setRange(1, 127);
    sdoNode->setValue(1);
    sdoNode->setObjectName("canSdoNode");
    sdoForm->addWidget(new QLabel("接口"), 0, 0);
    sdoForm->addWidget(sdoIf, 0, 1);
    sdoForm->addWidget(new QLabel("节点"), 0, 2);
    sdoForm->addWidget(sdoNode, 0, 3);
    sdoForm->addWidget(new QLabel("索引"), 1, 0);
    sdoForm->addWidget(coIndex, 1, 1);
    sdoForm->addWidget(new QLabel("子索引"), 1, 2);
    sdoForm->addWidget(coSub, 1, 3);
    sdoForm->addWidget(new QLabel("写入值"), 2, 0);
    sdoForm->addWidget(coValue, 2, 1, 1, 3);
    connect(sendNmt, &QPushButton::clicked, this, [=]() {
        postAction(
            "canopen_nmt",
            {{"interface", coIf->currentText()}, {"node_id", node->value()}, {"state", nmt->currentText()}},
            QString("确认通过 %1 向%2发送 NMT %3？")
                .arg(coIf->currentText(), node->value() == 0 ? "所有节点（广播）" : QString("节点 %1 ").arg(node->value()), nmt->currentText()));
    });
    connect(sdoWrite, &QPushButton::clicked, this, [=]() {
        bool ok = false;
        int index = coIndex->text().toInt(&ok, 0);
        if (!ok || index < 0 || index > 0xffff) {
            QMessageBox::warning(this, "参数错误", "对象索引必须是 0..65535，可使用 0x 十六进制格式");
            return;
        }
        bool valueOk = false;
        const qlonglong value = coValue->text().toLongLong(&valueOk, 0);
        if (!valueOk || value < -2147483648LL || value > 4294967295LL) {
            QMessageBox::warning(this, "参数错误", "写入值必须是 32 位整数，可使用 0x 十六进制格式");
            return;
        }
        postAction("canopen_sdo_write",
                   {{"interface", sdoIf->currentText()},
                    {"node_id", sdoNode->value()},
                    {"index", index},
                    {"subindex", coSub->value()},
                    {"value", QString::number(value)},
                    {"size", 4}},
                   "确认写入 CANopen SDO？错误值可能改变设备行为。");
    });
    for (auto pane : {qMakePair<QWidget *, QPushButton *>(co, sendNmt),
                      qMakePair<QWidget *, QPushButton *>(sdo, sdoWrite)}) {
        QWidget *body = new QWidget;
        QVBoxLayout *bodyLayout = new QVBoxLayout(body);
        bodyLayout->setContentsMargins(0, 0, 0, 0);
        bodyLayout->setSpacing(4);
        bodyLayout->addWidget(pane.first, 1);
        canControls->addTab(body, pane.first == co ? "NMT 控制" : "SDO 写入");
    }
    QHBoxLayout *canColumns = new QHBoxLayout;
    responsiveRows << canColumns;
    canColumns->addWidget(linkPage, 1);
    canColumns->addWidget(canControls, 1);
    canLayout->addLayout(canColumns, 1);
    canRoot->addWidget(scrollPage(canContent),1);
    auto *canActionRow = new QHBoxLayout;
    canActionRow->addWidget(applyCan,1);
    auto *canActions = new QStackedWidget;
    canActions->addWidget(sendNmt); canActions->addWidget(sdoWrite);
    canActionRow->addWidget(canActions,1);
    canRoot->addLayout(canActionRow);
    connect(canControls,&QTabWidget::currentChanged,canActions,&QStackedWidget::setCurrentIndex);
    capabilityControls << qMakePair<QWidget *, QString>(applyCan, "can");
    capabilityControls << qMakePair<QWidget *, QString>(canActions, "canopen");
    capabilityControls << qMakePair<QWidget *, QString>(linkPage, "can");
    tabs->setObjectName("protocolTabs");
    tabs->addTab(canPage, "CAN / CANopen");
    capabilityControls << qMakePair<QWidget *, QString>(canControls, "canopen");

    QWidget *rsPage = new QWidget;
    QVBoxLayout *rsLayout = new PageLayout(rsPage);
    rsLayout->setContentsMargins(0, 10, 0, 0);
    rsStatus = new StatusStrip;
    rsStatus->setMetrics({{"topology", "链路", "未获取"},
                          {"server", "从站状态", "未获取"},
                          {"clock", "采集状态", "等待数据"}});
    rsLayout->addWidget(rsStatus);
    QGroupBox *rs = new QGroupBox("串口参数");
    QFormLayout *rsForm = new QFormLayout(rs);
    QLineEdit *rsPort = edit("");
    serialPort = rsPort;
    QLineEdit *rsBaud = edit("9600");
    QSpinBox *rsUnit = new QSpinBox;
    rsUnit->setRange(1, 247);
    rsUnit->setValue(1);
    QLineEdit *rsPoll = edit("1000");
    QPushButton *saveRs = actionButton("保存配置");
    rsPort->setObjectName("rsPort");
    rsBaud->setObjectName("rsBaud");
    rsUnit->setObjectName("rsUnit");
    rsPoll->setObjectName("rsPoll");
    saveRs->setObjectName("rsSave");
    rsForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    rsForm->setContentsMargins(0, 0, 0, 0);
    rsForm->setVerticalSpacing(6);
    rsForm->addRow("端口", rsPort);
    rsForm->addRow("波特率", rsBaud);
    rsForm->addRow("站号", rsUnit);
    rsForm->addRow("轮询周期(ms)", rsPoll);
    auto *serialNote = captionLabel("帧格式由系统管理。仅保存首个从站配置；运行生效状态未验证。");
    serialNote->setObjectName("infoNote");
    rsForm->addRow(serialNote);
    connect(saveRs, &QPushButton::clicked, this, [=]() {
        bool baudOk = false, pollOk = false;
        const int baud = rsBaud->text().toInt(&baudOk);
        const int poll = rsPoll->text().toInt(&pollOk);
        if (!baudOk || baud < 300 || baud > 4000000 || !pollOk || poll < 50 || poll > 3600000 ||
            !controlDevices(latestSnapshot, "serial", "modbus-rtu").contains(rsPort->text())) {
            QMessageBox::warning(this, "参数错误", "请选择板级串口，波特率须为300..4000000，轮询周期须为50..3600000毫秒");
            return;
        }
        postAction("rs485_config",
                   {{"port", rsPort->text()},
                    {"baudrate", baud},
                    {"unit_id", rsUnit->value()},
                    {"poll_ms", poll}},
                   "确认保存首个从站的 Modbus RTU 参数？运行生效状态未验证。");
    });
    capabilityControls << qMakePair<QWidget *, QString>(rs, "serial");
    auto *rsColumns = new QHBoxLayout;
    responsiveRows << rsColumns;
    auto *rsEditor = new QWidget;
    auto *rsEditorLayout = new QVBoxLayout(rsEditor);
    rsEditorLayout->setContentsMargins(0,0,0,0);
    rsEditorLayout->addWidget(scrollPage(rs),1);
    rsEditorLayout->addWidget(saveRs);
    capabilityControls << qMakePair<QWidget *, QString>(saveRs, "serial");
    rsColumns->addWidget(rsEditor, 1);
    auto *registers = new QGroupBox("从站与寄存器");
    auto *registerLayout = new QVBoxLayout(registers);
    auto *registerTable = makeTable({"地址", "类型", "数值", "单位"});
    static_cast<DataTable *>(registerTable)->setEmptyText("尚未获取寄存器明细");
    registerLayout->addWidget(registerTable, 1);
    registerLayout->addWidget(captionLabel("请先完成串口参数与采集点配置。"));
    rsColumns->addWidget(registers, 1);
    rsLayout->addLayout(rsColumns, 1);
    tabs->addTab(rsPage, "RS485 / Modbus");

    QWidget *ecPage = new QWidget;
    QVBoxLayout *ecLayout = new PageLayout(ecPage);
    ecLayout->setContentsMargins(0, 10, 0, 0);
    ecStatus = new StatusStrip;
    ecStatus->setMetrics(
        {{"topology", "主站", "未获取"}, {"server", "从站状态", "未获取"}, {"clock", "诊断周期", "未测量"}});
    ecLayout->addWidget(ecStatus);
    QGroupBox *ec = new QGroupBox("CoE 对象访问");
    QGridLayout *ecForm = new QGridLayout(ec);
    ecForm->setContentsMargins(0, 0, 0, 0);
    QSpinBox *slave = new QSpinBox;
    slave->setRange(1, 65535);
    slave->setValue(1);
    QLineEdit *ecIndex = edit("0x6040");
    QSpinBox *ecSub = new QSpinBox;
    ecSub->setRange(0, 255);
    QLineEdit *ecValue = edit("06 00");
    QPushButton *ecWrite = actionButton("CoE SDO 写入", "danger");
    QPushButton *ecCycle = actionButton("执行 1000 次周期测试");
    slave->setObjectName("ecSlave");
    ecIndex->setObjectName("ecIndex");
    ecSub->setObjectName("ecSub");
    ecValue->setObjectName("ecValue");
    ecWrite->setObjectName("ecWrite");
    ecCycle->setObjectName("ecCycle");
    ecForm->addWidget(new QLabel("从站"), 0, 0);
    ecForm->addWidget(slave, 0, 1);
    ecForm->addWidget(new QLabel("子索引"), 0, 2);
    ecForm->addWidget(ecSub, 0, 3);
    ecForm->addWidget(new QLabel("索引"), 1, 0);
    ecForm->addWidget(ecIndex, 1, 1);
    ecForm->addWidget(new QLabel("数据"), 1, 2);
    ecForm->addWidget(ecValue, 1, 3);
    ecForm->addWidget(ecWrite, 2, 0, 1, 4);
    connect(ecWrite, &QPushButton::clicked, this, [=]() {
        bool ok = false;
        int index = ecIndex->text().toInt(&ok, 0);
        const QRegularExpression bytes("\\A[0-9A-Fa-f]{2}(?:\\s+[0-9A-Fa-f]{2}){0,31}\\z");
        if (!ok || index < 0 || index > 65535 || !bytes.match(ecValue->text().trimmed()).hasMatch()) {
            QMessageBox::warning(this, "参数错误", "对象索引须为0..65535，数据须为1到32字节十六进制");
            return;
        }
        postAction("ethercat_sdo_write",
                   {{"slave", slave->value()},
                    {"index", index},
                    {"subindex", ecSub->value()},
                    {"value", ecValue->text()}},
                   "确认写入 EtherCAT 从站？错误控制字可能导致设备动作。");
    });
    connect(ecCycle, &QPushButton::clicked, this, [=]() {
        postAction("ethercat_cycle", {{"count", 1000}, {"period_us", 1000}},
                   "确认执行 1000 次 EtherCAT 周期测试？周期设定为1毫秒，实际耗时取决于现场响应。");
    });
    capabilityControls << qMakePair<QWidget *, QString>(ec, "ethercat");
    auto *ecColumns = new QHBoxLayout;
    responsiveRows << ecColumns;
    auto *ecLeft = new QVBoxLayout;
    auto *slaves = new QGroupBox("从站状态");
    auto *slaveLayout = new QVBoxLayout(slaves);
    slaveLayout->setContentsMargins(0, 0, 0, 0);
    auto *slaveTable = makeTable({"从站", "名称", "状态", "数据"});
    slaveTable->setMinimumHeight(104);
    static_cast<DataTable *>(slaveTable)->setEmptyText("尚未获取从站明细");
    slaveLayout->addWidget(slaveTable, 1);
    ecLeft->addWidget(slaves, 1);
    ecLeft->addWidget(ec);
    ecColumns->addLayout(ecLeft, 3);
    auto *diagnostics = new QGroupBox("周期诊断");
    auto *diagnosticLayout = new QVBoxLayout(diagnostics);
    diagnosticLayout->setContentsMargins(0, 0, 0, 0);
    auto *emptyPlot = new QLabel("等待周期诊断数据");
    emptyPlot->setObjectName("infoNote");
    emptyPlot->setMinimumHeight(100);
    emptyPlot->setAlignment(Qt::AlignCenter);
    diagnosticLayout->addWidget(emptyPlot, 1);
    auto *measurements = new QHBoxLayout;
    for (const QString &name : {QString("最新周期"), QString("最大抖动")}) {
        auto *card = new QFrame;
        card->setObjectName("card");
        auto *metric = new QVBoxLayout(card);
        metric->addWidget(captionLabel(name));
        auto *value = new QLabel("未测量");
        value->setObjectName("statusValue");
        metric->addWidget(value);
        measurements->addWidget(card, 1);
    }
    diagnosticLayout->addLayout(measurements);
    diagnosticLayout->addWidget(ecCycle);
    auto *warning = captionLabel("诊断操作会访问现场总线，请确认现场状态。");
    warning->setObjectName("warningNote");
    diagnosticLayout->addWidget(warning);
    capabilityControls << qMakePair<QWidget *, QString>(ecCycle, "ethercat");
    ecColumns->addWidget(diagnostics, 2);
    ecLayout->addLayout(ecColumns, 1);
    tabs->addTab(scrollPage(ecPage), "EtherCAT");
    layout->addWidget(tabs, 1);
    layout->addWidget(makeActionFeedback("fieldbusActionResult"));
    return page;
}

QWidget *MainWindow::buildNetwork()
{
    auto *page = new LayoutPage;
    auto *layout = new PageLayout(page);
    layout->setContentsMargins(12, 10, 12, 12);
    layout->setSpacing(10);
    auto *tabs = new SectionTabs;
    tabs->setObjectName("networkTabs");
    for (int kind = 0; kind < 2; ++kind) {
        auto *interfaces = new LayoutPage;
        auto *columns = new PageRowLayout(interfaces);
        columns->setContentsMargins(0, 10, 0, 0);
        responsiveRows << columns;
        auto *left = new QGroupBox(kind == 0 ? "有线网络接口" : "无线网络接口");
        auto *leftLayout = new QVBoxLayout(left);
        leftLayout->setContentsMargins(0, 0, 0, 0);
        auto *table = makeTable({"接口", "状态", "IPv4 地址", "来源"});
        table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        table->horizontalHeader()->setMinimumSectionSize(60);
        static_cast<DataTable *>(table)->setEmptyText(kind == 0 ? "尚未发现有线接口" : "尚未发现无线接口");
        leftLayout->addWidget(table, 1);
        auto *description = new QLabel("选中接口信息");
        description->setObjectName("panelTitle");
        leftLayout->addWidget(description);
        auto *form = new QFormLayout;
        form->setVerticalSpacing(12);
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        QList<QLabel *> fields;
        for (const QString &name :
             {QString("接口名称"), QString("IPv4 地址"), QString("链路状态"), QString("数据来源")}) {
            auto *value = new QLabel("--");
            value->setTextFormat(Qt::PlainText);
            value->setObjectName("valueText");
            value->setWordWrap(true);
            form->addRow(name, value);
            fields << value;
        }
        leftLayout->addLayout(form);
        auto *refresh = actionButton("刷新接口状态", "secondary");
        refresh->setObjectName(QString("networkRefresh%1").arg(kind));
        refresh->setIcon(Visuals::icon("globe", Visuals::blue));
        connect(refresh, &QPushButton::clicked, this, &MainWindow::requestSnapshot);
        columns->addWidget(left, 3);
        auto *details = new QGroupBox(kind == 0 ? "本地链路" : "无线链路");
        auto *detailsLayout = new QVBoxLayout(details);
        detailsLayout->setContentsMargins(0, 0, 0, 0);
        detailsLayout->setSpacing(18);
        auto *symbol = new QLabel;
        symbol->setPixmap(Visuals::icon(kind == 0 ? "chip" : "globe").pixmap(54, 54));
        symbol->setAlignment(Qt::AlignCenter);
        detailsLayout->addWidget(symbol);
        auto *detail = new QLabel("等待接口数据");
        detail->setObjectName("panelTitle");
        detail->setWordWrap(true);
        detail->setAlignment(Qt::AlignCenter);
        detailsLayout->addWidget(detail);
        auto *note = captionLabel(kind == 0 ? "地址由系统网络服务管理。此处显示设备实际接口信息。"
                                            : "无线连接由系统网络服务管理。接入后可在这里查看接口与地址。");
        note->setObjectName("infoNote");
        detailsLayout->addWidget(note);
        detailsLayout->addStretch();
        columns->addWidget(details, 1);
        networkTables << table;
        networkFieldGroups << fields;
        networkDetails << detail;
        connect(table, &QTableWidget::itemSelectionChanged, this, &MainWindow::updateNetworkDetails);
        auto *section = new LayoutPage;
        auto *sectionLayout = new PageLayout(section);
        sectionLayout->setContentsMargins(0,0,0,0);
        sectionLayout->addWidget(scrollPage(interfaces),1);
        sectionLayout->addWidget(refresh);
        tabs->addTab(section, kind == 0 ? "有线网络" : "无线网络");
    }
    networkTable = networkTables.first();
    networkFields = networkFieldGroups.first();
    networkDetail = networkDetails.first();
    auto *cellular = new LayoutPage;
    auto *controls = new PageLayout(cellular);
    controls->setContentsMargins(0, 10, 0, 0);
    auto *cards = new QHBoxLayout;
    responsiveRows << cards;
    for (const QString &profile : {QString("ec20a"), QString("ec20b")}) {
        auto *box = new QGroupBox(profile == "ec20a" ? "蜂窝链路 A" : "蜂窝链路 B");
        auto *body = new QVBoxLayout(box);
        auto *symbol = new QLabel;
        symbol->setPixmap(Visuals::icon("cloud").pixmap(48, 48));
        symbol->setAlignment(Qt::AlignCenter);
        body->addWidget(symbol);
        auto *state = new QLabel("等待板级能力信息");
        state->setWordWrap(true);
        state->setAlignment(Qt::AlignCenter);
        state->setObjectName("statusValue");
        state->setProperty("profile", profile);
        cellularStates << state;
        body->addWidget(state);
        auto *form = new QFormLayout;
        form->addRow("拨号配置", new QLabel(profile.toUpper()));
        auto *interface = new QLabel("--");
        auto *address = new QLabel("-- / 未获取");
        interface->setTextFormat(Qt::PlainText);
        address->setTextFormat(Qt::PlainText);
        address->setWordWrap(true);
        cellularInterfaces << interface;
        cellularAddresses << address;
        form->addRow("网络接口", interface);
        form->addRow("地址 / 信号", address);
        body->addLayout(form);
        body->addStretch();
        auto *row = new QHBoxLayout;
        auto *start = actionButton("启动拨号");
        auto *stop = actionButton("断开连接", "secondary");
        start->setObjectName(profile + "Connect");
        stop->setObjectName(profile + "Disconnect");
        start->setProperty("profile", profile);
        stop->setProperty("profile", profile);
        row->addWidget(start);
        row->addWidget(stop);
        body->addLayout(row);
        connect(start, &QPushButton::clicked, this, [=]() {
            postAction("ec20", {{"profile", profile}, {"operation", "connect"}},
                       "确认启动 " + profile + " 拨号？");
        });
        connect(stop, &QPushButton::clicked, this, [=]() {
            postAction("ec20", {{"profile", profile}, {"operation", "disconnect"}},
                       "确认断开 " + profile + "？");
        });
        capabilityControls << qMakePair<QWidget *, QString>(start, "dual-cellular");
        capabilityControls << qMakePair<QWidget *, QString>(stop, "dual-cellular");
        cards->addWidget(box, 1);
    }
    controls->addLayout(cards, 1);
    auto *notice = captionLabel(
        "拨号仅在支持双蜂窝模块的板卡上可用。断开连接前请确认当前远程会话与上行数据已妥善处理。");
    notice->setObjectName("warningNote");
    controls->addWidget(notice);
    tabs->addTab(scrollPage(cellular), "蜂窝网络");
    layout->addWidget(tabs, 1);
    layout->addWidget(makeActionFeedback("networkActionResult"));
    return page;
}

void MainWindow::updateNetworkDetails()
{
    const int indices[] = {0, 2, 1, 3};
    for (int kind = 0; kind < networkTables.size(); ++kind) {
        auto *table = networkTables[kind];
        const int row = table->currentRow();
        for (int i = 0; i < networkFieldGroups[kind].size(); ++i)
            networkFieldGroups[kind][i]->setText(
                row >= 0 && table->item(row, indices[i]) ? table->item(row, indices[i])->text() : "--");
        auto *state = row >= 0 ? table->item(row, 1) : nullptr;
        networkDetails[kind]->setText(state ? state->text() + "\n" + table->item(row, 0)->text()
                                            : "尚未获取链路信息");
        networkDetails[kind]->setStyleSheet(state && state->data(Qt::UserRole).toBool() ? "color:#168345;"
                                                                                        : "color:#61758b;");
    }
}

void MainWindow::updateNetworkTables(const QJsonObject &root)
{
    const auto channels = root["platform"].toObject()["channels"].toObject();
    const auto networks = root["networks"].toArray();
    for (int kind = 0; kind < networkTables.size(); ++kind) {
        auto *table = networkTables[kind];
        const QString selected = table->currentRow() >= 0 && table->item(table->currentRow(), 0)
                                     ? table->item(table->currentRow(), 0)->text()
                                     : QString();
        table->blockSignals(true);
        table->setRowCount(0);
        int selectRow = -1;
        for (const auto &value : networks) {
            const auto item = value.toObject();
            const QString name = item["name"].toString();
            QString type;
            for (const auto &channel : channels) {
                auto ch = channel.toObject();
                if (ch["device"].toString() == name)
                    type = ch["kind"].toString();
            }
            // Board mappings determine category; cellular netdev names can look like Ethernet.
            if (name.trimmed().isEmpty() || item["kind"].toString() == "cellular")
                continue;
            if (type != (kind == 0 ? "ethernet" : "wifi"))
                continue;
            const int row = table->rowCount();
            table->insertRow(row);
            const bool online = item["online"].toBool();
            QString state = item["state"].toString("未知");
            if (state == "up")
                state = "已连接";
            else if (state == "down")
                state = "未连接";
            else if (state == "unknown")
                state = "未知";
            auto *status = cell(state, online ? kSuccess : kMuted);
            status->setData(Qt::UserRole, online);
            if (online)
                status->setIcon(Visuals::icon("check", kSuccess));
            table->setItem(row, 0, cell(name));
            table->setItem(row, 1, status);
            table->setItem(row, 2,
                           cell(item["address"].toString().isEmpty() ? "--" : item["address"].toString()));
            const QString source = item["source"].toString();
            table->setItem(row, 3, cell(source == "demo" ? "样例" : (source == "real" ? "设备" : "未获取")));
            if (name == selected)
                selectRow = row;
        }
        // Once a selection disappears, leave details empty until the user chooses again.
        if (selected.isEmpty() && !table->property("selectionLost").toBool() && table->rowCount())
            selectRow = 0;
        table->setProperty("selectionLost", selectRow < 0 && (!selected.isEmpty() || table->property("selectionLost").toBool()));
        table->setCurrentCell(selectRow, selectRow >= 0 ? 0 : -1);
        table->blockSignals(false);
    }
    updateNetworkDetails();
}

QWidget *MainWindow::buildDevices()
{
    auto *page = new LayoutPage;
    auto *layout = new PageLayout(page);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(8);
    auto *heading = new QHBoxLayout;
    heading->addWidget(titleLabel("设备监控"));
    heading->addWidget(captionLabel("查看读数与趋势；暂停仅冻结本页显示"), 1);
    auto *period = new QLabel("最近 60 秒");
    period->setObjectName("valueText");
    heading->addWidget(period);
    layout->addLayout(heading);
    monitorStatus = new StatusStrip;
    monitorStatus->setMetrics(
        {{"thermometer", "温度", "--"}, {"gauge", "压力", "--"}, {"monitor", "数据状态", "等待数据"}});
    layout->addWidget(monitorStatus);
    deviceTrend = new OverviewDashboard(true);
    deviceTrend->setMinimumHeight(150);
    layout->addWidget(deviceTrend, 3);
    auto *points = new QGroupBox("数据点");
    auto *pointLayout = new QVBoxLayout(points);
    pointLayout->setContentsMargins(0, 0, 0, 0);
    deviceTable = makeTable({"名称", "当前值", "单位", "状态"});
    deviceTable->setMinimumHeight(124);
    deviceTable->verticalHeader()->setDefaultSectionSize(44);
    static_cast<DataTable *>(deviceTable)->setEmptyText("尚未获取采集读数");
    pointLayout->addWidget(deviceTable);
    layout->addWidget(points, 2);
    auto *actions = new QHBoxLayout;
    pauseMonitor = actionButton("暂停监控显示", "secondary");
    pauseMonitor->setCheckable(true);
    pauseMonitor->setObjectName("pauseMonitor");
    pauseMonitor->setToolTip("仅冻结此页的读数和趋势；现场采集、告警与其他页面继续刷新");
    connect(pauseMonitor, &QPushButton::toggled, this, [this](bool checked) {
        monitorPaused = checked;
        pauseMonitor->setText(checked ? "继续显示 · 当前已暂停" : "暂停监控显示");
        if (!checked && !latestSnapshot.isEmpty()) {
            // Paused display may have missed missing readings or clock resets.
            // Start a fresh segment instead of joining across that interval.
            deviceTrend->setOffline();
            updateSnapshot(latestSnapshot);
        }
    });
    actions->addWidget(pauseMonitor, 1);
    auto *settings = actionButton("刷新与显示设置", "secondary");
    settings->setIcon(Visuals::icon("settings", Visuals::blue));
    settings->setObjectName("monitorSettings");
    connect(settings, &QPushButton::clicked, this, [this]() { navigateApplication("settings"); });
    actions->addWidget(settings, 1);
    auto *monitorFooter = new QWidget;
    monitorFooter->setObjectName("monitorFooter");
    monitorFooter->setLayout(actions);
    auto *monitorPage = new LayoutPage;
    auto *monitorLayout = new PageLayout(monitorPage);
    monitorLayout->setContentsMargins(0,0,0,0);
    monitorLayout->addWidget(scrollPage(page),1);
    monitorLayout->addWidget(monitorFooter);
    devicePageStack=new QStackedWidget;
    devicePageStack->setObjectName("devicePageStack");
    devicePageStack->addWidget(monitorPage);
    boundTrendPanel=new DesktopV1::DesktopTrendPanel;
    devicePageStack->addWidget(boundTrendPanel);
    devicePageStack->layout()->setSizeConstraint(QLayout::SetNoConstraint);
    devicePageStack->setMinimumSize(0,0);
    return devicePageStack;
}

void MainWindow::updatePageMetrics(const QJsonObject &root)
{
    QJsonObject rs, ec;
    for (const auto &value : root["buses"].toArray()) {
        auto bus = value.toObject();
        if (bus["name"].toString().startsWith("RS485", Qt::CaseInsensitive))
            rs = bus;
        if (bus["name"].toString().startsWith("EtherCAT", Qt::CaseInsensitive))
            ec = bus;
    }
    auto busMetrics = [](const QJsonObject &bus, bool ethercat) {
        const bool available = !bus.isEmpty() && bus["source"].toString() != "unavailable";
        const bool online = available && bus["online"].toBool();
        return QList<StatusMetric>{
            {"topology", ethercat ? "主站链路" : "通信链路",
             available ? (online ? "在线" : "未连接") : "未获取", online ? kSuccess : kMuted},
            {"server", "从站状态", bus["state"].toString("未获取"), kMuted},
            {"clock", "数据来源",
             bus["source"].toString() == "demo" ? "样例数据" : (available ? "设备数据" : "等待采集"),
             Visuals::blue}};
    };
    rsStatus->setMetrics(busMetrics(rs, false));
    ecStatus->setMetrics(busMetrics(ec, true));
    const auto capabilities = root["platform"].toObject()["capabilities"].toArray();
    for (int i = 0; i < cellularStates.size(); ++i) {
        auto *state = cellularStates[i];
        const auto mapping = cellularMapping(root, state->property("profile").toString());
        QString text = root.isEmpty() ? "数据服务离线" : "板级能力或拨号映射未就绪";
        QString address = "--";
        if (!mapping.isEmpty()) {
            text = "拨号控制可用\n链路状态未获取";
            for (const auto &value : root["networks"].toArray()) {
                const auto item = value.toObject();
                if (item["name"] != mapping["interface"] || item["source"].toString() != "real")
                    continue;
                const auto status = item["state"].toString();
                text = status == "up" ? "链路已连接\n拨号状态未验证"
                       : status == "down" ? "链路未连接" : "链路状态未获取";
                if (!item["address"].toString().isEmpty()) address = item["address"].toString();
            }
        }
        state->setText(text);
        cellularInterfaces[i]->setText(mapping["interface"].toString("--"));
        cellularAddresses[i]->setText(address + " / 未获取");
    }
    const auto services = root["services"].toObject();
    const QStringList names = {"thingsboard", "bluetooth", "ntp"};
    for (int i = 0; i < serviceStates.size(); ++i) {
        const auto service = services[names[i]].toObject();
        const bool known = service["running"].isBool();
        const bool running = service["running"].toBool();
        serviceStates[i]->setText(known ? (running ? "● 运行中" : "● 未运行") : "● 状态未获取");
        serviceStates[i]->setStyleSheet(known && running ? "color:#168345;" : "color:#61758b;");
        serviceStates[i]->setToolTip(service["detail"].toString());
    }
    if (monitorPaused && !root.isEmpty())
        return;
    QString temperature = "--", pressure = "--";
    int readings = 0;
    for (const auto &value : root["sensors"].toArray()) {
        const auto sensor = value.toObject();
        if (!Visuals::sensorAvailable(sensor))
            continue;
        ++readings;
        double reading;
        if (Visuals::physicalReading(sensor, "温度", &reading))
            temperature = QString::number(reading, 'f', 1) + " °C";
        if (Visuals::physicalReading(sensor, "压力", &reading))
            pressure = QString::number(reading, 'f', 2) + " MPa";
    }
    monitorStatus->setMetrics(
        {{"thermometer", "温度", temperature},
         {"gauge", "压力", pressure},
         {"monitor", "数据状态", readings ? QString("%1 项读数").arg(readings) : "等待采集",
          readings ? kSuccess : kMuted}});
}

QWidget *MainWindow::buildAlarms()
{
    QWidget *page = new LayoutPage;
    QVBoxLayout *layout = new PageLayout(page);
    layout->setContentsMargins(12, 10, 12, 12);
    layout->setSpacing(9);
    QHBoxLayout *top = new QHBoxLayout;
    top->addWidget(titleLabel("告警记录"));
    top->addStretch();
    QPushButton *exportButton = actionButton("导出筛选结果", "secondary");
    exportButton->setObjectName("alarmExport");
    exportButton->setEnabled(false);
    exportButton->setIcon(Visuals::icon("document", Visuals::blue));
    top->addWidget(exportButton);
    layout->addLayout(top);
    alarmTabs = new QTabBar;
    alarmTabs->setObjectName("alarmTabs");
    alarmTabs->addTab("全部");
    alarmTabs->addTab("未确认");
    alarmTabs->addTab("已确认");
    alarmTabs->setExpanding(false);
    layout->addWidget(alarmTabs);
    connect(alarmTabs, &QTabBar::currentChanged, this, [this]() { applyAlarmFilter(); });
    QFrame *content = new QFrame;
    content->setObjectName("panel");
    QVBoxLayout *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 8, 0, 0);
    QHBoxLayout *filters = new QHBoxLayout;
    filters->setContentsMargins(12, 0, 12, 0);
    filters->addWidget(new QLabel("级别"));
    alarmSeverity = new QComboBox;
    alarmSeverity->addItem("全部", "");
    alarmSeverity->addItem("警告", "warning");
    alarmSeverity->addItem("错误", "error");
    alarmSeverity->addItem("提示", "info");
    filters->addWidget(alarmSeverity);
    filters->addSpacing(16);
    filters->addWidget(new QLabel("来源"));
    alarmSource = new QComboBox;
    alarmSource->addItem("全部", "");
    filters->addWidget(alarmSource, 1);
    filters->addSpacing(16);
    filters->addWidget(new QLabel("时间"));
    alarmTime = new QComboBox;
    alarmTime->addItem("全部记录", 0);
    alarmTime->addItem("最近 24 小时", 86400);
    alarmTime->addItem("最近 7 天", 7 * 86400);
    filters->addWidget(alarmTime);
    connect(alarmSource, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this]() { applyAlarmFilter(); });
    connect(alarmTime, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this]() { applyAlarmFilter(); });
    auto *reset = actionButton("清除筛选", "secondary");
    reset->setObjectName("clearAlarmFilters");
    reset->setEnabled(false);
    connect(reset,&QPushButton::clicked,this,[this]() {
        {
            const QSignalBlocker state(alarmTabs), severity(alarmSeverity), source(alarmSource), time(alarmTime);
            alarmTabs->setCurrentIndex(0);alarmSeverity->setCurrentIndex(0);
            alarmSource->setCurrentIndex(0);alarmTime->setCurrentIndex(0);
        }
        applyAlarmFilter();
    });
    contentLayout->addLayout(filters);
    auto *summary = captionLabel("尚未获取告警记录");
    summary->setObjectName("alarmFilterSummary");
    summary->setContentsMargins(12,0,12,0);
    top->insertWidget(top->count()-1,reset);
    contentLayout->addWidget(summary);
    connect(alarmSeverity, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this]() { applyAlarmFilter(); });
    alarmTable = makeTable({"ID", "时间", "级别", "来源", "内容", "状态"});
    alarmTable->hideColumn(0);
    alarmTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    alarmTable->setColumnWidth(1, 150);
    alarmTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    alarmTable->setColumnWidth(2, 80);
    contentLayout->addWidget(alarmTable, 1);
    layout->addWidget(content, 1);
    QFrame *details = new QFrame;
    details->setObjectName("panel");
    QHBoxLayout *row = new QHBoxLayout(details);
    QVBoxLayout *description = new QVBoxLayout;
    QLabel *title = new QLabel("选中告警详情");
    title->setObjectName("panelTitle");
    description->addWidget(title);
    alarmDetail = captionLabel("选择一条告警查看详情");
    description->addWidget(alarmDetail);
    row->addLayout(description, 1);
    alarmDetail->setTextFormat(Qt::PlainText);
    alarmAck = actionButton("确认选中告警");
    alarmAck->setObjectName("alarmAck");
    alarmAck->setEnabled(false);
    alarmAck->setIcon(Visuals::icon("check", Qt::white));
    row->addWidget(alarmAck);
    layout->addWidget(details);
    connect(alarmTable, &QTableWidget::itemSelectionChanged, this, &MainWindow::updateAlarmSelection);
    connect(alarmTable, &QTableWidget::currentCellChanged, this, &MainWindow::updateAlarmSelection);
    connect(alarmAck, &QPushButton::clicked, this, [this]() {
        applyAlarmFilter();
        if (!alarmAck->isEnabled())
            return;
        const QString id = alarmTable->item(alarmTable->currentRow(), 0)->text();
        alarmAckPending = true;
        updateAlarmSelection();
        sendJson("POST", QString("/alarms/%1/ack").arg(id), {}, "ack");
    });
    connect(exportButton, &QPushButton::clicked, this, [this]() {
        applyAlarmFilter();
        // Freeze records and count together before the file dialog runs its event loop.
        int count=0;
        for(int row=0;row<alarmTable->rowCount();++row)
            if(!alarmTable->isRowHidden(row)) ++count;
        if(count==0) return;
        const QByteArray output = visibleAlarmCsv();
        const QString name = TouchFiles::saveFile(
            this, "导出当前可见告警", QDir::homePath() + "/dshancontrol-alarms.csv", "CSV (*.csv)");
        if (name.isEmpty())
            return;
        QSaveFile file(name);
        if (!file.open(QIODevice::WriteOnly)) {
            QMessageBox::warning(this, "导出失败", file.errorString());
            return;
        }
        if (file.write(output) != output.size() || !file.commit())
            QMessageBox::warning(this, "导出失败", file.errorString());
        else {
            QMessageBox result(QMessageBox::Information,"导出完成",QString(),QMessageBox::Ok,this);
            result.setTextFormat(Qt::PlainText);
            result.setText(QString("已保存 %1 条告警（点击导出时的筛选结果）。\n%2").arg(count).arg(name));
            result.exec();
        }
    });
    return page;
}

QByteArray MainWindow::visibleAlarmCsv() const
{
    QByteArray output("\xef\xbb\xbf");
    auto quote = [](QString value) {
        if (!value.trimmed().isEmpty() && QString("=+-@").contains(value.trimmed().left(1)))
            value.prepend('\'');
        value.replace('"', "\"\"");
        return '"' + value + '"';
    };
    for (int row = -1; row < alarmTable->rowCount(); row++) {
        if (row >= 0 && alarmTable->isRowHidden(row))
            continue;
        QStringList fields;
        for (int col = 0; col < alarmTable->columnCount(); col++) {
            const auto *item =
                row < 0 ? alarmTable->horizontalHeaderItem(col) : alarmTable->item(row, col);
            fields << quote(item ? item->text() : QString());
        }
        output += (fields.join(',') + "\r\n").toUtf8();
    }
    return output;
}

void MainWindow::updateAlarmSelection()
{
    const int row = alarmTable->currentRow();
    const bool selected = row >= 0 && !alarmTable->isRowHidden(row) &&
                          !alarmTable->selectedItems().isEmpty() && alarmTable->item(row, 5);
    if (!selected) {
        alarmDetail->setText("选择一条告警查看详情");
        alarmAck->setEnabled(false);
        return;
    }
    const QString id = alarmTable->item(row, 0)->text();
    alarmDetail->setText(QString("ID %1 · %2\n%3").arg(id, alarmTable->item(row, 1)->text(),
                                                     alarmTable->item(row, 4)->text()));
    alarmAck->setEnabled(alarmsAvailable && !latestSnapshot.isEmpty() && !alarmAckPending && id != "--" &&
                        alarmTable->item(row, 5)->data(Qt::UserRole).toString() == "active");
}

void MainWindow::applyAlarmFilter()
{
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    int active = 0, acknowledged = 0;
    for (int row = 0; row < alarmTable->rowCount(); row++) {
        const auto *state = alarmTable->item(row, 5);
        const auto *severity = alarmTable->item(row, 2);
        if (!state || !severity)
            continue;
        const QString rawState = state->data(Qt::UserRole).toString(),
                      rawSeverity = severity->data(Qt::UserRole).toString();
        active += rawState == "active";
        acknowledged += rawState == "acknowledged";
        const bool matchState =
            alarmTabs->currentIndex() == 0 ||
            (alarmTabs->currentIndex() == 1 ? rawState == "active" : rawState == "acknowledged");
        const QString filter = alarmSeverity->currentData().toString();
        const QString source = alarmSource->currentData().toString();
        const auto *sourceItem = alarmTable->item(row, 3);
        const int seconds = alarmTime->currentData().toInt();
        const auto *timeItem = alarmTable->item(row, 1);
        const bool matchesTime =
            seconds == 0 || (timeItem && timeItem->data(Qt::UserRole).isValid() &&
                            timeItem->data(Qt::UserRole).toLongLong() >= now - seconds &&
                            timeItem->data(Qt::UserRole).toLongLong() <= now);
        const bool matchesSource =
            source.isEmpty() || (sourceItem && sourceItem->data(Qt::UserRole).toString() == source);
        alarmTable->setRowHidden(row, !matchState || (!filter.isEmpty() && rawSeverity != filter) ||
                                          !matchesSource || !matchesTime);
    }
    alarmTabs->setTabText(0, QString("全部（%1）").arg(alarmTable->rowCount()));
    alarmTabs->setTabText(1, QString("未确认（%1）").arg(active));
    alarmTabs->setTabText(2, QString("已确认（%1）").arg(acknowledged));
    if (alarmTable->currentRow() >= 0 && alarmTable->isRowHidden(alarmTable->currentRow())) {
        alarmTable->clearSelection();
        alarmTable->setCurrentCell(-1, -1);
    }
    int visible=0;
    for(int row=0;row<alarmTable->rowCount();++row)
        if(!alarmTable->isRowHidden(row)) ++visible;
    const bool filtered=alarmTabs->currentIndex()!=0 || alarmSeverity->currentIndex()!=0 ||
                        alarmSource->currentIndex()!=0 || alarmTime->currentIndex()!=0;
    const QString freshness=alarmsAvailable ? "记录已加载" : "最新状态未确认，保留记录仅供查看";
    const QString hint=visible ? "导出仅包含当前可见记录" : filtered ? "没有匹配记录，可清除筛选" : "当前没有记录";
    findChild<QLabel *>("alarmFilterSummary")->setText(
        QString("显示 %1 / %2 条 · %3 · %4").arg(visible).arg(alarmTable->rowCount()).arg(freshness,hint));
    findChild<QPushButton *>("clearAlarmFilters")->setEnabled(filtered);
    findChild<QPushButton *>("alarmExport")->setEnabled(visible>0);
    static_cast<DataTable *>(alarmTable)->setEmptyText(filtered ? "没有匹配记录 · 可清除筛选"
        : alarmsAvailable ? "暂无告警记录" : "尚未获取最新告警记录");
    updateAlarmSelection();
}

QWidget *MainWindow::buildSettings()
{
    QWidget *page = new LayoutPage;
    QVBoxLayout *layout = new PageLayout(page);
    layout->setContentsMargins(12, 10, 12, 12);
    layout->setSpacing(10);
    QHBoxLayout *heading = new QHBoxLayout;
    heading->addWidget(titleLabel("系统设置"));
    heading->addWidget(captionLabel("选择分类，修改后保存；执行结果在操作日志中核对"), 1);
    layout->addLayout(heading);
    QTabWidget *settingsTabs = new SectionTabs;
    settingsTabs->setObjectName("settingsTabs");
    QWidget *display = new LayoutPage;
    QVBoxLayout *displayLayout = new PageLayout(display);
    displayLayout->setContentsMargins(0, 10, 0, 0);
    displayLayout->setSpacing(10);
    QHBoxLayout *columns = new QHBoxLayout;
    responsiveRows << columns;
    auto *screen = new QGroupBox("显示");
    auto *screenForm = new QFormLayout(screen);
    screenForm->setVerticalSpacing(8);
    screenResolution = new QLabel;
    screenResolution->setObjectName("valueText");
    screenForm->addRow(iconLabel("monitor", "屏幕分辨率"), screenResolution);
    brightnessSlider = new TouchSlider;
    brightnessSlider->setObjectName("brightnessSlider");
    brightnessSlider->setAccessibleName("屏幕亮度");
    brightnessSlider->setRange(10, 255);
    brightnessSlider->setSingleStep(3);
    brightnessSlider->setValue(220);
    auto *brightnessRow = new QHBoxLayout;
    brightnessRow->setSpacing(6);
    auto *dimmer = new QPushButton("-");
    auto *brighter = new QPushButton("+");
    dimmer->setObjectName("brightnessDecrease");
    brighter->setObjectName("brightnessIncrease");
    dimmer->setAccessibleName("降低亮度");
    brighter->setAccessibleName("提高亮度");
    for (auto *button : {dimmer, brighter}) {
        button->setStyleSheet("padding: 0; min-width: 44px; max-width: 44px;"
                             "min-height: 48px; max-height: 48px;");
    }
    connect(dimmer, &QPushButton::clicked, brightnessSlider, [this]() {
        brightnessSlider->triggerAction(QAbstractSlider::SliderSingleStepSub);
    });
    connect(brighter, &QPushButton::clicked, brightnessSlider, [this]() {
        brightnessSlider->triggerAction(QAbstractSlider::SliderSingleStepAdd);
    });
    brightnessRow->addWidget(dimmer);
    brightnessRow->addWidget(brightnessSlider, 1);
    brightnessRow->addWidget(brighter);
    auto *brightnessValue =
        new QLabel(QString::number(qRound(brightnessSlider->value() * 100.0 / 255)) + "%");
    brightnessValue->setMinimumWidth(36);
    connect(brightnessSlider, &QSlider::valueChanged, this, [brightnessValue](int value) {
        brightnessValue->setText(QString::number(qRound(value * 100.0 / 255)) + "%");
    });
    brightnessRow->addWidget(brightnessValue);
    screenForm->addRow(iconLabel("sun", "屏幕亮度"), brightnessRow);
    screenForm->addRow(captionLabel("简体中文 · 思源黑体 · 根据屏幕自动调整布局"));
    auto *displayProbe = new QPushButton("屏幕色彩 / 比例校验");
    displayProbe->setObjectName("openDisplayProbe");
    displayProbe->setMinimumHeight(44);
    connect(displayProbe, &QPushButton::clicked, this, [this]() {
        auto *probe = new DisplayProbe(this);
        probe->showFullScreen();
    });
    auto *interaction = new QGroupBox("数据与交互");
    auto *form = new QFormLayout(interaction);
    form->setVerticalSpacing(12);
    refreshCombo = new QComboBox;
    refreshCombo->addItem("0.5 秒", 500);
    refreshCombo->addItem("1 秒", 1000);
    refreshCombo->addItem("2 秒", 2000);
    refreshCombo->addItem("5 秒", 5000);
    refreshCombo->setCurrentIndex(1);
    demoCheck = new ToggleSwitch;
    form->addRow(iconLabel("clock", "数据刷新"), refreshCombo);
    form->addRow(iconLabel("server", "演示数据"), demoCheck);
    auto *notice = new QLabel("演示模式使用样例数据；控制操作仍需确认。");
    notice->setObjectName("warningNote");
    notice->setWordWrap(true);
    form->addRow(notice);
    QPushButton *viewServices = actionButton("查看服务", "secondary");
    connect(viewServices, &QPushButton::clicked, this,
            [settingsTabs]() { settingsTabs->setCurrentIndex(1); });
    form->addRow(iconLabel("settings", "系统服务"), viewServices);
    auto *record = captionLabel("等待服务返回当前设置");
    record->setObjectName("settingsRecordedValues");
    record->setTextFormat(Qt::PlainText);
    screenForm->insertRow(1,record);
    auto *feedback = captionLabel(settingsFeedback);
    feedback->setObjectName("settingsLastResult");
    feedback->setTextFormat(Qt::PlainText);

    columns->addWidget(screen, 1);
    columns->addWidget(interaction, 1);
    displayLayout->addLayout(columns, 1);
    QFrame *footer = new QFrame;
    footer->setObjectName("panel");
    auto *footerBody = new QVBoxLayout(footer);
    footerBody->addWidget(feedback);
    auto *footerLayout = new QHBoxLayout;
    footerBody->addLayout(footerLayout);
    auto *editState = captionLabel("等待设备连接后加载设置");
    editState->setObjectName("settingsEditState");
    footerLayout->addWidget(editState, 1);
    screenForm->addRow(displayProbe);
    auto *discard = actionButton("撤销修改", "secondary");
    discard->setObjectName("discardSettingsDraft");
    discard->setEnabled(false);
    footerLayout->addWidget(discard);
    connect(discard, &QPushButton::clicked, this, &MainWindow::discardSettingsDraft);
    QPushButton *save = actionButton("保存设置");
    save->setObjectName("saveSettings");
    save->setMinimumWidth(126);
    footerLayout->addWidget(save);
    connect(brightnessSlider, &QSlider::valueChanged, this, [this]() {
        if (!loadingSettings) {
            settingsDirty = true;
            settingsDraftNotice.clear();
            ++settingsRevision;
            updateSettingsControls();
        }
    });
    connect(demoCheck, &QCheckBox::toggled, this, [this]() {
        if (!loadingSettings) {
            settingsDirty = true;
            settingsDraftNotice.clear();
            ++settingsRevision;
            updateSettingsControls();
        }
    });
    connect(refreshCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        if (!loadingSettings) {
            settingsDirty = true;
            settingsDraftNotice.clear();
            ++settingsRevision;
            updateSettingsControls();
        }
    });
    connect(save, &QPushButton::clicked, this, [this]() {
        if (settingsPending || latestSnapshot.isEmpty()) return;
        const QJsonObject values{{"brightness", brightnessSlider->value()},
                                 {"demo_mode", demoCheck->isChecked()},
                                 {"refresh_ms", refreshCombo->currentData().toInt()}};
        const auto revision = settingsRevision;
        const auto connectionRevision = settingsConnectionRevision;
        QMessageBox confirmation(QMessageBox::Question, "保存设置",
            QString("确认保存以下设置并应用背光？\n亮度：%1%\n数据刷新：%2 秒\n演示数据：%3")
                .arg(qRound(values["brightness"].toInt()*100.0/255))
                .arg(values["refresh_ms"].toInt()/1000.0)
                .arg(values["demo_mode"].toBool() ? "开启" : "关闭"),
            QMessageBox::Yes | QMessageBox::No, this);
        confirmation.button(QMessageBox::Yes)->setText("保存并应用");
        confirmation.button(QMessageBox::No)->setText("取消");
        confirmation.setDefaultButton(QMessageBox::No);
        if (confirmation.exec() != QMessageBox::Yes)
            return;
        if (settingsPending || latestSnapshot.isEmpty() || revision != settingsRevision ||
            connectionRevision != settingsConnectionRevision) {
            settingsDraftNotice = "连接或编辑已变化，本次未保存；请核对后重新确认。";
            updateSettingsControls();
            return;
        }
        settingsDraftNotice.clear();
        savedSettingsRevision = revision;
        submittedSettings = values;
        settingsPending = true;
        settingsFeedback = "正在保存设置，等待服务确认；尚未应用背光。";
        updateSettingsControls();
        sendJson("PUT", "/settings", values, "settings");
    });
    save->setEnabled(false);
    auto *displaySection = new LayoutPage;
    auto *displaySectionLayout = new PageLayout(displaySection);
    displaySectionLayout->setContentsMargins(0,0,0,0);
    displaySectionLayout->addWidget(scrollPage(display),1);
    displaySectionLayout->addWidget(footer);
    settingsTabs->addTab(displaySection, "显示与交互");
    auto *services = new LayoutPage;
    auto *serviceLayout = new PageLayout(services);
    serviceLayout->setContentsMargins(0, 10, 0, 0);
    serviceLayout->setSpacing(10);
    const QStringList serviceNames = {"thingsboard", "bluetooth", "ntp"};
    const QStringList titles = {"工业数据采集", "蓝牙连接服务", "网络时间同步"};
    const QStringList descriptions = {"ThingsBoard · 采集与上行", "Bluetooth · 近场设备连接",
                                      "NTP · 设备时钟同步"};
    for (int i = 0; i < serviceNames.size(); ++i) {
        const QString name = serviceNames[i];
        auto *card = new QFrame;
        card->setObjectName("panel");
        auto *row = new QHBoxLayout(card);
        row->setContentsMargins(14, 10, 14, 10);
        auto *symbol = new QLabel;
        symbol->setPixmap(Visuals::icon(i == 0 ? "server" : (i == 1 ? "globe" : "clock")).pixmap(30, 30));
        row->addWidget(symbol);
        auto *description = new QVBoxLayout;
        auto *title = new QLabel(titles[i]);
        title->setObjectName("panelTitle");
        description->addWidget(title);
        description->addWidget(captionLabel(descriptions[i]));
        row->addLayout(description, 1);
        auto *state = new QLabel("● 状态未获取");
        state->setMinimumWidth(100);
        state->setObjectName("caption");
        row->addWidget(state);
        serviceStates << state;
        auto *actions = new QHBoxLayout;
        for (const QString &operation : {QString("start"), QString("restart"), QString("stop")}) {
            auto *button =
                actionButton(operation == "start" ? "启动" : (operation == "restart" ? "重启" : "停止"),
                             operation == "stop" ? "danger" : "secondary");
            button->setProperty("serviceName", name);
            button->setObjectName("service-" + name + "-" + operation);
            button->setEnabled(false);
            button->setToolTip("服务可用性尚未获取");
            serviceButtons << button;
            actions->addWidget(button);
            connect(button, &QPushButton::clicked, this, [=]() {
                postAction("service", {{"service", name}, {"operation", operation}},
                           "确认" + button->text() + " " + name + "？");
            });
        }
        // Keep controls reachable on portrait displays without tiny touch targets.
        row->addLayout(actions);
        responsiveRows << row;
        serviceLayout->addWidget(card);
    }
    auto *serviceNotice = captionLabel("重启或停止采集服务会中断相应数据链路，请在允许维护时操作。");
    serviceNotice->setObjectName("warningNote");
    serviceLayout->addWidget(serviceNotice);
    serviceLayout->addStretch();
    auto *system = new QFrame;
    system->setObjectName("panel");
    auto *systemRow = new QHBoxLayout(system);
    auto *systemText = new QVBoxLayout;
    auto *systemTitle = new QLabel("设备电源管理");
    systemTitle->setObjectName("panelTitle");
    systemText->addWidget(systemTitle);
    systemText->addWidget(captionLabel("重启整机将暂时中断所有现场通信。"));
    systemRow->addLayout(systemText, 1);
    auto *reboot = actionButton("重启整机", "danger");
    rebootButton = reboot;
    reboot->setEnabled(false);
    connect(reboot, &QPushButton::clicked, this,
            [this]() { postAction("system_reboot", {}, "确认重启 DshanControl？所有现场通信将暂时中断。"); });
    systemRow->addWidget(reboot);
    serviceLayout->addWidget(system);
    auto *serviceSection=new QWidget;
    auto *serviceSectionLayout=new QVBoxLayout(serviceSection);
    serviceSectionLayout->setContentsMargins(0,0,0,0);
    serviceSectionLayout->addWidget(scrollPage(services),1);
    serviceSectionLayout->addWidget(makeActionFeedback("serviceActionResult"));
    settingsTabs->addTab(serviceSection, "服务管理");
    auto *audit = new LayoutPage;
    auto *auditLayout = new PageLayout(audit);
    auditLayout->setContentsMargins(0, 10, 0, 0);
    auto *auditPanel = new QGroupBox("操作记录");
    auto *auditBody = new QVBoxLayout(auditPanel);
    auto *auditHeader = new QHBoxLayout;
    auditHeader->addWidget(new QLabel("执行结果"));
    auditResult = new QComboBox;
    auditResult->addItem("全部", "");
    auditResult->addItem("成功", "success");
    auditResult->addItem("失败", "failed");
    auditHeader->addWidget(auditResult);
    auditHeader->addStretch();
    auto *reload = actionButton("刷新记录", "secondary");
    reload->setObjectName("reloadAudit");
    connect(reload, &QPushButton::clicked, this, [this]() { sendJson("GET", "/audit", {}, "audit"); });
    auditHeader->addWidget(reload);
    auditBody->addLayout(auditHeader);
    auditTable = makeTable({"时间", "动作", "目标", "结果"});
    static_cast<DataTable *>(auditTable)->setEmptyText("暂无操作记录");
    auto *details=actionButton("查看选中详情","secondary");
    details->setObjectName("auditDetails");details->setEnabled(false);
    auditHeader->insertWidget(auditHeader->count()-1,details);
    connect(auditTable,&QTableWidget::itemSelectionChanged,this,[this,details]() {
        const int row=auditTable->currentRow();
        details->setEnabled(row>=0 && !auditTable->isRowHidden(row) && auditTable->item(row,3));
    });
    connect(details,&QPushButton::clicked,this,[this]() {
        const int row=auditTable->currentRow();
        if(row<0 || auditTable->isRowHidden(row) || !auditTable->item(row,3)) return;
        QStringList lines;
        for(int col=0;col<4;++col)
            lines << auditTable->horizontalHeaderItem(col)->text()+"："+auditTable->item(row,col)->text();
        const auto detail=auditTable->item(row,3)->toolTip();
        lines << "详细结果："+(detail.isEmpty() ? QString("服务未提供详情") : detail);
        QMessageBox dialog(QMessageBox::Information,"操作记录详情",lines.join("\n"),QMessageBox::Ok,this);
        dialog.setTextFormat(Qt::PlainText);dialog.button(QMessageBox::Ok)->setText("返回日志");
        dialog.exec();
    });
    auditBody->addWidget(auditTable, 1);
    auditCount = captionLabel("显示最近 200 条操作记录");
    auditBody->addWidget(auditCount);
    auditLayout->addWidget(auditPanel, 1);
    auto *auditNote = captionLabel("记录来自当前设备，便于核对参数修改与服务操作结果。");
    auditNote->setObjectName("infoNote");
    auditLayout->addWidget(auditNote);
    connect(auditResult, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this]() { updateAuditFilter(); });
    settingsTabs->addTab(scrollPage(audit), "操作日志");
    auto *applicationUpdatePanel = new UpdatePanel;
    applicationUpdatePanel->setObjectName("applicationUpdatePanel");
    applicationUpdatePage = scrollPage(applicationUpdatePanel);
    settingsTabs->addTab(applicationUpdatePage, "应用更新");
    layout->addWidget(settingsTabs, 1);
    return page;
}

void MainWindow::setDesktopPointSource(DesktopV1::DesktopCanvas *source)
{
    boundTrendPanel->setSource(source);
}

void MainWindow::setDesktopEmbedded(bool embedded)
{
    desktopEmbedded = embedded;
    nav->setVisible(!embedded);
    if (auto *header = findChild<QFrame *>("header")) header->setVisible(!embedded);
    if (auto *footer = findChild<QFrame *>("footer")) footer->setVisible(!embedded);
}

void MainWindow::navigateApplication(const QString &id)
{
    if (desktopEmbedded) emit applicationNavigationRequested(id);
    else openApplication(id);
}

bool MainWindow::openApplication(const QString &id, const QJsonObject &context)
{
    DesktopV1::ApplicationRoute route;
    if (!DesktopV1::ApplicationRegistry::resolve(id, context, &route))
        return false;
    if (route.page == DesktopV1::ApplicationRoute::Devices) {
        boundTrendPanel->setBinding(route.context);
        devicePageStack->setCurrentIndex(route.context.isEmpty() ? 0 : 1);
        devicePageStack->setSizePolicy(QSizePolicy::Expanding,
            route.context.isEmpty() ? QSizePolicy::Preferred : QSizePolicy::Ignored);
        devicePageStack->updateGeometry();
    } else if (route.page == DesktopV1::ApplicationRoute::Cloud) {
        cloudPanel->selectSection(route.section);
    } else if (route.page == DesktopV1::ApplicationRoute::Family) {
        familyPanel->selectSection(route.section);
        if (route.context.contains("device_id"))
            familyPanel->selectDeviceById(route.context["device_id"].toString());
    }
    showPage(route.page);
    if (route.page == DesktopV1::ApplicationRoute::Video && route.context.contains("path"))
        mediaPanel->selectFile(route.context["path"].toString());
    if (id == "settings") {
        if (auto *tabs = findChild<QTabWidget *>("settingsTabs")) tabs->setCurrentIndex(0);
    }
    if (id == "update") {
        if (auto *tabs = findChild<QTabWidget *>("settingsTabs")) {
            const int updateIndex = tabs->indexOf(applicationUpdatePage);
            if (updateIndex >= 0) tabs->setCurrentIndex(updateIndex);
        }
    }
    return true;
}

void MainWindow::showPage(int index)
{
    if (index < 0 || index >= pages->count())
        return;
    pages->setCurrentIndex(index);
    for (int i = 0; i < navButtons.size(); ++i)
        navButtons[i]->setChecked(i == index);
    if (auto *scroll = findChild<QScrollArea *>("desktopNavigation"))
        scroll->ensureWidgetVisible(navButtons[index]);
    if (index == 4 && !alarmRequestActive) {
        alarmRequestActive = true;
        sendJson("GET", "/alarms?limit=500", {}, "alarms");
    }
    if (index == 5)
        sendJson("GET", "/audit", {}, "audit");
}

bool MainWindow::requestDesktopBrightness(int percent)
{
    // Reuse the settings/action transaction; preserve unsaved settings edits.
    if (settingsPending)
        return false;
    if (percent < 0 || percent > 100 || latestSnapshot.isEmpty() || settingsDirty) {
        emit desktopBrightnessFinished(false, -1, settingsDirty
            ? QString("请先保存系统设置中的修改，再调节亮度")
            : QString("数据服务未连接或亮度参数无效"));
        return false;
    }
    const int raw = qBound(10, qRound(percent * 255.0 / 100.0), 255);
    savedSettingsRevision = settingsRevision;
    submittedSettings = QJsonObject{{"brightness", raw}};
    settingsPending = desktopBrightnessRequest = true;
    settingsFeedback = "正在保存快捷亮度设置，等待服务确认。";
    updateSettingsControls();
    sendJson("PUT", "/settings", submittedSettings, "settings");
    return true;
}

void MainWindow::requestSnapshot()
{
    if (requestActive)
        return;
    requestActive = true;
    QNetworkRequest req(QUrl(QString(kBaseUrl) + "/snapshot"));
    QNetworkReply *reply = networkManager.get(req);
    reply->setProperty("kind", "snapshot");
    reply->setProperty("generation", QVariant::fromValue(snapshotGeneration));
    QTimer::singleShot(4000, reply, [reply]() {
        if (reply->isRunning())
            reply->abort();
    });
}

void MainWindow::sendJson(const QByteArray &method, const QString &path, const QJsonObject &body,
                          const QString &kind)
{
    // Reads begun before a settings transaction must not overwrite its outcome.
    if (kind == "settings")
        ++snapshotGeneration;
    QNetworkRequest req(QUrl(QString(kBaseUrl) + path));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    const auto bytes = QJsonDocument(body).toJson(QJsonDocument::Compact);
    QNetworkReply *reply = nullptr;
    if (method == "GET") {
        reply = networkManager.get(req);
    } else {
        req.setAttribute(QNetworkRequest::DoNotBufferUploadDataAttribute, true);
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
        req.setHeader(QNetworkRequest::ContentLengthHeader, bytes.size());
        auto *upload = new OneShotUpload(bytes);
        reply = networkManager.sendCustomRequest(req, method, upload);
        upload->setParent(reply);
    }
    reply->setProperty("kind", kind);
    QTimer::singleShot(30000, reply, [reply]() {
        if (reply->isRunning())
            reply->abort();
    });
}

QLabel *MainWindow::makeActionFeedback(const QString &name)
{
    auto *label=new QLabel("最近操作：尚未提交。执行后在此核对结果；设备状态以刷新结果为准。");
    label->setObjectName(name);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    label->setStyleSheet("font-size:14px;color:#525252;background:#edf5ff;padding:2px;");
    label->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
    return label;
}

void MainWindow::postAction(const QString &action, const QJsonObject &params, const QString &description)
{
    // Covers modal confirmation and the outstanding request, including navigation away.
    if (actionPending) return;
    const QString resultName=action=="ec20" ? "networkActionResult" :
        (action=="service" || action=="system_reboot") ? "serviceActionResult" : "fieldbusActionResult";
    auto *feedback=findChild<QLabel *>(resultName);
    const quint64 connectionRevision=settingsConnectionRevision;
    QJsonObject actionParams = params;
    const auto cellular = action == "ec20" ? cellularMapping(latestSnapshot, params["profile"].toString()) : QJsonObject();
    if (action == "ec20") {
        actionParams["device"] = cellular["device"];
        actionParams["interface"] = cellular["interface"];
    }
    const bool ethercat = action == "ethercat_sdo_write" || action == "ethercat_cycle";
    if (ethercat) {
        const auto devices = controlDevices(latestSnapshot, "ethernet", "ethercat");
        if (devices.size() != 1)
            return;
        actionParams["interface"] = devices.first();
    }
    const auto allowed = [this, &action, &params, &actionParams, ethercat, cellular]() {
        if (latestSnapshot.isEmpty()) return false;
        if (action == "service")
            return latestSnapshot.value("services").toObject()[params["service"].toString()]
                .toObject()["available"] == QJsonValue(true);
        if (action == "ec20")
            return !cellular.isEmpty() && cellularMapping(latestSnapshot, params["profile"].toString()) == cellular;
        if (action == "rs485_config")
            return controlDevices(latestSnapshot, "serial", "modbus-rtu").contains(params["port"].toString());
        if (ethercat) {
            const auto devices = controlDevices(latestSnapshot, "ethernet", "ethercat");
            return devices.size() == 1 && devices.first() == actionParams["interface"].toString();
        }
        if (action != "can_apply" && action != "canopen_nmt" && action != "canopen_sdo_write")
            return true;
        const QString device = params["interface"].toString();
        if (device.trimmed().isEmpty())
            return false;
        const auto platform = latestSnapshot.value("platform").toObject();
        const auto capabilities = platform["capabilities"].toArray();
        if ((action != "can_apply" && !capabilities.contains("canopen")) ||
            (params["fd"].toBool() && !capabilities.contains("can-fd")))
            return false;
        const auto channels = platform["channels"].toObject();
        for (const auto &value : channels) {
            const auto channel = value.toObject();
            if (channel["kind"].toString() == "can" && channel["device"].toString() == device)
                return true;
        }
        return false;
    };
    if (!allowed()) {
        if (feedback) feedback->setText("未提交：连接、能力或目标不可用，请刷新后核对。");
        return;
    }
    static const QMap<QString,QString> parameterNames = {
        {"interface","接口"},{"device","设备"},{"profile","模块配置"},{"port","串口"},
        {"baud","波特率"},{"baudrate","波特率"},{"unit","从站地址"},{"poll_ms","轮询间隔（毫秒）"},
        {"node_id","节点"},{"state","目标状态"},{"index","对象索引"},{"subindex","子索引"},
        {"value","写入值"},{"size","写入字节数"},{"bitrate","仲裁波特率"},
        {"data_bitrate","数据波特率"},{"fd","CAN-FD"},{"restart_ms","恢复间隔（毫秒）"},
        {"slave","从站"},{"data","十六进制数据"},{"cycles","周期数"},{"period_us","周期（微秒）"},
        {"service","服务"},{"operation","操作"}
    };
    QStringList submitted;
    for (auto it=actionParams.constBegin();it!=actionParams.constEnd();++it) {
        QString value=it.value().isBool() ? (it.value().toBool() ? "开启" : "关闭")
                                        : it.value().toVariant().toString();
        if(it.key()=="index") value="0x"+QString::number(it.value().toInt(),16);
        if(it.key()=="operation") value=QMap<QString,QString>{{"start","启动"},{"stop","停止"},
            {"restart","重启"},{"connect","拨号"},{"disconnect","断开"}}.value(value,value);
        submitted << parameterNames.value(it.key(),it.key())+"："+value;
    }
    const QString confirmationText=description+(submitted.isEmpty()?QString():"\n\n本次提交：\n"+submitted.join("\n"));
    QMessageBox confirmation(QMessageBox::Warning, "操作确认", confirmationText,
                             QMessageBox::Yes | QMessageBox::No, this);
    confirmation.setTextFormat(Qt::PlainText);
    confirmation.button(QMessageBox::Yes)->setText("确认执行");
    confirmation.button(QMessageBox::No)->setText("取消");
    confirmation.setDefaultButton(QMessageBox::No);
    actionPending=true;
    if (confirmation.exec() != QMessageBox::Yes) {
        actionPending=false;
        return;
    }
    // The modal confirmation runs an event loop; a snapshot may revoke access.
    if (connectionRevision!=settingsConnectionRevision || !allowed()) {
        actionPending=false;
        if (feedback) feedback->setText("未提交：确认期间连接或目标发生变化，请重新核对。");
        return;
    }
    actionResultLabel=feedback;
    actionDescription=description;
    if (feedback) feedback->setText("等待设备回复："+description+" 请勿重复提交。");
    sendJson("POST", "/actions", {{"action", action}, {"params", actionParams}, {"confirmed", true}}, "action");
}

void MainWindow::handleReply(QNetworkReply *reply)
{
    const QString kind = reply->property("kind").toString();
    if (kind == "snapshot") {
        requestActive = false;
        const auto generation = reply->property("generation");
        if (generation.isValid() && generation.toULongLong() != snapshotGeneration) {
            reply->deleteLater();
            requestSnapshot();
            return;
        }
    }
    if (kind == "alarms")
        alarmRequestActive = false;
    if (kind == "ack")
        alarmAckPending = false;
    if (kind == "alarms" || kind == "ack") {
        alarmsAvailable = false;
        applyAlarmFilter();
    }
    const QByteArray raw = reply->readAll();
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (kind == "settings" || kind == "brightness") {
        const bool ok = reply->error() == QNetworkReply::NoError && doc.isObject() &&
                        doc.object()["ok"] == QJsonValue(true);
        bool valid = ok;
        if (ok && kind == "settings") {
            const auto received = doc.object()["settings"].toObject();
            for (auto it = submittedSettings.begin(); it != submittedSettings.end(); ++it)
                valid = valid && received[it.key()] == it.value();
        }
        if (ok && kind == "brightness")
            valid = doc.object()["result"].toObject()["brightness"] == submittedSettings["brightness"];
        if (valid && kind == "settings") {
            if (settingsRevision == savedSettingsRevision) settingsDirty = false;
            settingsFeedback = "设置已保存；正在应用背光，等待设备确认。";
            updateSettingsControls();
            sendJson("POST", "/actions", {{"action", "brightness"},
                {"params", QJsonObject{{"value", submittedSettings["brightness"]}}},
                {"confirmed", true}}, "brightness");
        } else {
            // Also discard reads started while the save/apply transaction was pending.
            ++snapshotGeneration;
            settingsPending = false;
            const QString message = valid ? "显示设置已保存，背光已应用" :
                (kind == "brightness" ? "显示设置已保存，背光应用未确认。请重试并检查背光设备。" :
                                        "设置保存未确认；保留当前编辑，请重试。");
            settingsFeedback = message;
            updateSettingsControls();
            if (desktopBrightnessRequest) {
                desktopBrightnessRequest = false;
                emit desktopBrightnessFinished(valid,
                    valid ? submittedSettings["brightness"].toInt() : -1, message);
            } else {
                if (valid) QMessageBox::information(this, "操作完成", message);
                else QMessageBox::warning(this, "操作失败", message);
            }
            requestSnapshot();
        }
        reply->deleteLater();
        return;
    }
    if (kind == "action") {
        actionPending=false;
        QString outcome;
        if (reply->error()!=QNetworkReply::NoError || !doc.isObject())
            outcome="执行结果未确认，请先核对设备与操作日志；不要直接重试。";
        else if (doc.object().value("ok")==QJsonValue(true))
            outcome="命令已完成；实际设备状态仍需刷新核对。";
        else
            outcome="服务返回失败："+doc.object().value("error").toString("未提供原因")+"；先核对设备与日志。";
        if (actionResultLabel) actionResultLabel->setText("最近操作："+actionDescription+" "+outcome);
        actionResultLabel.clear();
    }
    if (reply->error() != QNetworkReply::NoError || !doc.isObject()) {
        if (kind == "snapshot")
            markOffline();
        if (kind != "snapshot") {
            const QJsonValue backendError = doc.isObject() ? doc.object().value("error") : QJsonValue();
            const QString message = backendError.isString() && !backendError.toString().trimmed().isEmpty()
                                        ? backendError.toString()
                                        : QString::fromUtf8(raw.isEmpty() ? reply->errorString().toUtf8() : raw);
            QMessageBox::critical(this, "操作失败", message);
        }
        reply->deleteLater();
        return;
    }
    const QJsonObject root = doc.object();
    if (!root.value("ok").toBool(false)) {
        if (kind == "snapshot") {
            markOffline();
            reply->deleteLater();
            return;
        }
        QMessageBox::warning(this, "操作失败", root.value("error").toString("未知错误"));
    } else if (kind == "snapshot") {
        updateSnapshot(root);
    } else if (kind == "alarms") {
        alarmsAvailable = true;
        QString selectedId;
        if (alarmTable->currentRow() >= 0 && alarmTable->item(alarmTable->currentRow(), 0))
            selectedId = alarmTable->item(alarmTable->currentRow(), 0)->text();
        const QJsonArray rows = root.value("alarms").toArray();
        alarmTable->clearSelection();
        alarmTable->setCurrentCell(-1, -1);
        alarmTable->setRowCount(rows.size());
        for (int i = 0; i < rows.size(); ++i) {
            const QJsonObject x = rows[i].toObject();
            const double id = x["id"].toDouble();
            const bool validId = Visuals::finiteNumber(x["id"]) && id >= 1 &&
                                 id <= 9007199254740991.0 && std::floor(id) == id;
            alarmTable->setItem(i, 0, cell(validId ? QString::number(qint64(id)) : "--"));
            const double timestamp = x["ts"].toDouble();
            const bool validTime = Visuals::finiteNumber(x["ts"]) && timestamp >= 0 &&
                                   timestamp <= 253402300799.0 && std::floor(timestamp) == timestamp;
            auto *timeCell = cell(validTime ? QDateTime::fromSecsSinceEpoch(qint64(timestamp))
                                                .toString("yyyy-MM-dd hh:mm:ss") : "--");
            if (validTime)
                timeCell->setData(Qt::UserRole, qint64(timestamp));
            alarmTable->setItem(i, 1, timeCell);
            const QString severity = x.value("severity").toString();
            auto *level =
                cell(severity == "warning"
                         ? "警告"
                         : (severity == "error" ? "错误" : (severity == "info" ? "提示" : severity)),
                     severity == "warning" ? QColor("#d18a00") : kText);
            level->setData(Qt::UserRole, severity);
            level->setIcon(Visuals::icon(severity == "warning" ? "warning" : "info", Visuals::blue));
            alarmTable->setItem(i, 2, level);
            const QString source = x.value("source").toString();
            auto *sourceCell = cell(source == "demo" ? "样例设备" : source);
            sourceCell->setData(Qt::UserRole, source);
            alarmTable->setItem(i, 3, sourceCell);
            alarmTable->setItem(i, 4, cell(sourceText(x, x.value("message").toString())));
            const QString state = x.value("state").toString();
            auto *status = cell(state == "active" ? "未确认" : (state == "acknowledged" ? "已确认" : state),
                                state == "active" ? QColor("#d18a00") : Visuals::muted);
            status->setData(Qt::UserRole, state);
            alarmTable->setItem(i, 5, status);
            if (alarmTable->item(i, 0)->text() == selectedId)
                alarmTable->setCurrentCell(i, 0);
        }
        const QString selectedSource = alarmSource->currentData().toString();
        const QString selectedSourceText = alarmSource->currentText();
        alarmSource->blockSignals(true);
        alarmSource->clear();
        alarmSource->addItem("全部", "");
        for (int row = 0; row < alarmTable->rowCount(); ++row) {
            auto *source = alarmTable->item(row, 3);
            const QString raw = source->data(Qt::UserRole).toString();
            if (!raw.isEmpty() && alarmSource->findData(raw) < 0)
                alarmSource->addItem(source->text(), raw);
        }
        if (!selectedSource.isEmpty() && alarmSource->findData(selectedSource) < 0)
            alarmSource->addItem(selectedSourceText, selectedSource);
        alarmSource->setCurrentIndex(qMax(0, alarmSource->findData(selectedSource)));
        alarmSource->blockSignals(false);
        applyAlarmFilter();
    } else if (kind == "audit") {
        const QJsonArray rows = root.value("audit").toArray();
        auditTable->setRowCount(rows.size());
        for (int i = 0; i < rows.size(); ++i) {
            QJsonObject x = rows[i].toObject();
            auditTable->setItem(i, 0,
                                cell(QDateTime::fromSecsSinceEpoch(x.value("ts").toVariant().toLongLong())
                                         .toString("MM-dd hh:mm:ss")));
            static const QMap<QString, QString> actionNames = {{"brightness", "背光亮度"},
                                                               {"service", "服务管理"},
                                                               {"can_apply", "CAN 参数配置"},
                                                               {"rs485_config", "串口参数配置"},
                                                               {"canopen_nmt", "CANopen NMT 控制"},
                                                               {"canopen_sdo_write", "CANopen SDO 写入"},
                                                               {"canopen_rpdo", "CANopen PDO 发送"},
                                                               {"ethercat_sdo_write", "EtherCAT 对象写入"},
                                                               {"ethercat_cycle", "EtherCAT 周期诊断"},
                                                               {"ec20", "蜂窝拨号"},
                                                               {"system_reboot", "重启设备"},
                                                               {"alarm_ack", "告警确认"}};
            const QString action = x.value("action").toString();
            auto *actionCell = cell(actionNames.value(action, action));
            actionCell->setToolTip(action);
            auditTable->setItem(i, 1, actionCell);
            auditTable->setItem(i, 2, cell(x.value("target").toString()));
            const QString result = x.value("result").toString();
            auto *state = cell(result == "success" ? "成功" : (result == "failed" ? "失败" : result),
                               result == "success" ? kSuccess : (result == "failed" ? kDanger : kMuted));
            state->setData(Qt::UserRole, result);
            state->setIcon(Visuals::icon(result == "success" ? "check" : (result == "failed" ? "warning" : "info"),
                                         result == "success" ? kSuccess : (result == "failed" ? kDanger : kMuted)));
            state->setToolTip(x["detail"].toString());
            auditTable->setItem(i, 3, state);
        }
        updateAuditFilter();
    } else {
        if (kind == "ack" && !alarmRequestActive) {
            alarmRequestActive = true;
            sendJson("GET", "/alarms?limit=500", {}, "alarms");
        }
        QMessageBox::information(this, "操作完成",
                                 kind == "action" ? "命令已执行；实际状态以设备刷新结果为准。" : "操作已完成");
        requestSnapshot();
        if (pages->currentIndex() == 5)
            sendJson("GET", "/audit", {}, "audit");
    }
    reply->deleteLater();
}

void MainWindow::updateSettingsControls()
{
    const bool online = !latestSnapshot.isEmpty();
    const auto recorded = latestSnapshot.value("settings").toObject();
    const bool canRestore = recorded["brightness"].isDouble() &&
        recorded["brightness"].toDouble() == recorded["brightness"].toInt() &&
        recorded["brightness"].toInt() >= 10 && recorded["brightness"].toInt() <= 255 &&
        recorded["demo_mode"].isBool() && recorded["refresh_ms"].isDouble() &&
        recorded["refresh_ms"].toDouble() == recorded["refresh_ms"].toInt() &&
        recorded["refresh_ms"].toInt() >= 500 && recorded["refresh_ms"].toInt() <= 10000;
    if (auto *discard = findChild<QPushButton *>("discardSettingsDraft"))
        discard->setEnabled(online && canRestore && settingsDirty && !settingsPending);
    if (auto *record = findChild<QLabel *>("settingsRecordedValues"))
        record->setText(!online ? "服务已离线：当前编辑仍保留，暂时不能撤销或保存。"
            : !canRestore ? "服务设置不完整：保留当前编辑，暂时不能撤销。"
            : QString("最近服务记录：亮度 %1% · 刷新 %2 秒 · 演示%3\n此记录不代表背光已实际应用。")
                .arg(qRound(recorded["brightness"].toInt()*100.0/255))
                .arg(recorded["refresh_ms"].toInt()/1000.0)
                .arg(recorded["demo_mode"].toBool() ? "开启" : "关闭"));
    if (auto *feedback = findChild<QLabel *>("settingsLastResult"))
        feedback->setText("最近操作：" + settingsFeedback);
    if (auto *state = findChild<QLabel *>("settingsEditState"))
        state->setText(settingsPending ? "正在保存并应用，请等待设备确认…"
            : !online ? "设备未连接，暂时不能保存设置"
            : settingsDirty ? "有未保存的修改 · 点击“保存设置”后生效"
                            : "设置已加载 · 修改后点击“保存设置”");
    if (!settingsDraftNotice.isEmpty()) {
        if (auto *state = findChild<QLabel *>("settingsEditState"))
            state->setText(settingsDraftNotice + "\n" + state->text());
    }
    findChild<QPushButton *>("saveSettings")->setEnabled(online && !settingsPending);
    rebootButton->setEnabled(online);
    for (auto *button : serviceButtons) {
        const bool available = online && latestSnapshot["services"].toObject()
            [button->property("serviceName").toString()].toObject()["available"] == QJsonValue(true);
        button->setEnabled(available);
        button->setToolTip(available ? "操作将影响相应数据链路，需确认" : "离线或设备未提供此服务");
    }
}

void MainWindow::discardSettingsDraft()
{
    auto *button = findChild<QPushButton *>("discardSettingsDraft");
    if (!button || !button->isEnabled() || settingsPending) return;
    const auto recorded = latestSnapshot.value("settings").toObject();
    const auto revision = settingsRevision;
    const auto connectionRevision = settingsConnectionRevision;
    QMessageBox confirmation(QMessageBox::Question, "撤销未保存的修改",
        "将当前编辑恢复为页面显示的服务记录。此操作只改表单，不保存设置，也不调节背光。",
        QMessageBox::Yes | QMessageBox::No, this);
    confirmation.setTextFormat(Qt::PlainText);
    confirmation.button(QMessageBox::Yes)->setText("撤销修改");
    confirmation.button(QMessageBox::No)->setText("继续编辑");
    confirmation.setDefaultButton(QMessageBox::No);
    if (confirmation.exec() != QMessageBox::Yes) return;
    if (settingsPending || latestSnapshot.isEmpty() || revision != settingsRevision ||
        connectionRevision != settingsConnectionRevision ||
        latestSnapshot.value("settings").toObject() != recorded) {
        updateSettingsControls();
        QMessageBox::information(this, "未撤销修改", "服务状态或编辑已变化，未撤销；请核对页面后重试。");
        return;
    }
    loadingSettings = true;
    brightnessSlider->setValue(recorded["brightness"].toInt());
    demoCheck->setChecked(recorded["demo_mode"].toBool());
    const int refresh = recorded["refresh_ms"].toInt();
    int index = refreshCombo->findData(refresh);
    if (index < 0) {
        refreshCombo->addItem(QString("%1 秒").arg(refresh / 1000.0), refresh);
        index = refreshCombo->count() - 1;
    }
    refreshCombo->setCurrentIndex(index);
    loadingSettings = false;
    settingsDirty = false;
    settingsDraftNotice.clear();
    ++settingsRevision;
    updateSettingsControls();
}

void MainWindow::markOffline()
{
    ++settingsConnectionRevision;
    emit hmiSnapshotChanged({});
    alarmsAvailable = false;
    applyAlarmFilter();
    connectionLabel->setText("● 数据服务离线");
    connectionLabel->setStyleSheet("color:#b52f40;");
    cpuValue->setText("CPU --");
    memoryValue->setText("内存 --");
    temperatureValue->setText("温度 --");
    storageValue->setText("存储 --");
    alarmSummaryLabel->setText("● 数据服务离线");
    alarmSummaryLabel->setStyleSheet("color:#b52f40;");
    for (const auto &control : capabilityControls)
        control.first->setEnabled(false);
    for (QTableWidget *table : {busTable, networkTable, deviceTable})
        table->setRowCount(0);
    latestSnapshot = {};
    updateSettingsControls();
    updatePageMetrics({});
    for (auto *table : networkTables) {
        table->setRowCount(0);
        table->setProperty("selectionLost", false);
    }
    dashboard->setOffline();
    deviceTrend->setOffline();
    updateNetworkDetails();
}

QString MainWindow::sourceText(const QJsonObject &item, const QString &text) const
{
    const QString source = item.value("source").toString("real");
    if (source == "demo")
        return text + "  [演示]";
    if (source == "unavailable")
        return text + "  [不可用]";
    return text;
}

QString MainWindow::uptimeText(qint64 seconds)
{
    qint64 days = seconds / 86400;
    seconds %= 86400;
    return QString("%1天 %2:%3")
        .arg(days)
        .arg(seconds / 3600, 2, 10, QChar('0'))
        .arg((seconds / 60) % 60, 2, 10, QChar('0'));
}

void MainWindow::updateSnapshot(const QJsonObject &root)
{
    if (!root.value("system").isObject() || !root.value("summary").isObject() ||
        !Visuals::finiteNumber(root["timestamp"]) || root["timestamp"].toDouble() < 0 ||
        root["timestamp"].toDouble() > 253402300799.0) {
        markOffline();
        return;
    }
    connectionLabel->setText("● 数据在线");
    connectionLabel->setStyleSheet("color:#168366;");
    if (pages->currentIndex() == 4 && !alarmRequestActive) {
        alarmRequestActive = true;
        sendJson("GET", "/alarms?limit=500", {}, "alarms");
    }
    const qint64 timestamp = root.value("timestamp").toVariant().toLongLong();
    clockLabel->setText(QDateTime::fromSecsSinceEpoch(timestamp).toString("MM-dd  hh:mm:ss"));
    dashboard->setSnapshot(root);
    latestSnapshot = root;
    emit hmiSnapshotChanged(root);
    updateAlarmSelection();
    updatePageMetrics(root);
    updateSettingsControls();
    if (!monitorPaused)
        deviceTrend->setSnapshot(root);
    const QJsonObject system = root.value("system").toObject();
    const QJsonObject summary = root.value("summary").toObject();
    const QJsonObject platform = root.value("platform").toObject();
    const QJsonObject channels = platform.value("channels").toObject();
    const QJsonArray capabilities = platform.value("capabilities").toArray();
    QStringList canDevices;
    const auto serialDevices = controlDevices(root, "serial", "modbus-rtu");
    const auto ethercatDevices = controlDevices(root, "ethernet", "ethercat");
    for (const QJsonValue &value : channels) {
        const QJsonObject channel = value.toObject();
        if (channel.value("kind").toString() == "can" &&
            !channel.value("device").toString().trimmed().isEmpty())
            canDevices << channel.value("device").toString();
    }
    for (QComboBox *combo : canInterfaces) {
        QStringList current;
        for (int i = 0; i < combo->count(); ++i)
            current << combo->itemText(i);
        if (current != canDevices) {
            const QString selected = combo->currentText();
            combo->clear();
            combo->addItems(canDevices);
            if (combo->findText(selected) >= 0)
                combo->setCurrentText(selected);
        }
    }
    if (!portInitialized && !serialDevices.isEmpty()) {
        serialPort->setText(serialDevices.first());
        portInitialized = true;
    }
    if (!capabilities.contains("can-fd") || canDevices.isEmpty())
        canFdCheck->setChecked(false);
    for (const auto &control : capabilityControls) {
        control.first->setEnabled(control.second == "can"
                                      ? !canDevices.isEmpty()
                                      : (control.second == "canopen" || control.second == "can-fd")
                                            ? !canDevices.isEmpty() && capabilities.contains(control.second)
                                      : control.second == "serial" ? !serialDevices.isEmpty()
                                      : control.second == "ethercat" ? ethercatDevices.size() == 1
                                      : control.second == "dual-cellular" ? !cellularMapping(root, control.first->property("profile").toString()).isEmpty()
                                                                    : capabilities.contains(control.second));
    }

    alarmSummaryLabel->setText("● 系统" + summary.value("status").toString("未知"));
    alarmSummaryLabel->setStyleSheet(summary.value("status").toString() == "正常" ? "color:#168345;"
                                                                                  : "color:#bb7900;");
    cpuValue->setText(QString("CPU  %1%").arg(Visuals::numberText(system["cpu"])));
    memoryValue->setText(QString("内存  %1%").arg(Visuals::numberText(system["memory"])));
    temperatureValue->setText(
        system.value("temperature_source").toString() == "unavailable"
            ? "温度 --"
            : QString("温度  %1°C").arg(Visuals::numberText(system["temperature"], 1)));
    storageValue->setText(QString("存储  %1%").arg(Visuals::numberText(system["storage"])));

    const QJsonArray buses = root.value("buses").toArray();
    busTable->setRowCount(0);
    for (int i = 0; i < buses.size(); ++i) {
        QJsonObject x = buses[i].toObject();
        QString name = x.value("name").toString();
        QString state = x.value("state").toString();
        const QColor color = x["online"].toBool() ? kSuccess : kMuted;
        if (name.startsWith("can", Qt::CaseInsensitive)) {
            const int row = busTable->rowCount();
            busTable->insertRow(row);
            busTable->setItem(row, 0, cell(name));
            busTable->setItem(row, 1, cell(state, color));
            busTable->setItem(row, 2,
                              cell(x.value("bitrate").toInt()
                                       ? QString::number(x.value("bitrate").toDouble() / 1000.0) + " kbit/s"
                                       : "--"));
            busTable->setItem(row, 3,
                              cell(x.value("source").toString() == "demo"
                                       ? "样例"
                                       : (x.value("source").toString() == "real" ? "设备" : "未获取")));
        }
    }

    updateNetworkTables(root);
    if (!monitorPaused) {
        const QJsonArray sensors = root.value("sensors").toArray();
        deviceTable->setRowCount(sensors.size());
        for (int i = 0; i < sensors.size(); ++i) {
            const auto sensor = sensors[i].toObject();
            const bool available = Visuals::sensorAvailable(sensor);
            deviceTable->setItem(i, 0, cell(sensor["name"].toString()));
            auto *reading = cell(available ? QString::number(sensor["value"].toDouble(), 'f',
                                                             sensor["unit"].toString() == "MPa" ? 2 : 1)
                                           : "--",
                                 Visuals::blue);
            QFont bold = reading->font();
            bold.setBold(true);
            reading->setFont(bold);
            deviceTable->setItem(i, 1, reading);
            deviceTable->setItem(i, 2, cell(sensor["unit"].toString()));
            const QString stateText = available ? sensor["state"].toString("未知") : "未获取";
            auto *state = cell(stateText, available && stateText == "正常" ? kSuccess : kMuted);
            state->setToolTip(sourceText(sensor, state->text()));
            deviceTable->setItem(i, 3, state);
        }
    }

    const QJsonObject settings = root.value("settings").toObject();
    loadingSettings = true;
    if (!settingsDirty && !settingsPending) {
        brightnessSlider->setValue(settings.value("brightness").toInt(220));
        demoCheck->setChecked(settings.value("demo_mode").toBool(false));
    }
    demoBadge->hide();
    connectionLabel->setText(settings.value("demo_mode").toBool(false) ? "● 样例数据" : "● 数据在线");
    int refresh = settings.value("refresh_ms").toInt(1000);
    int combo = refreshCombo->findData(refresh);
    if (combo < 0 && refresh >= 500 && refresh <= 10000 && !settingsDirty && !settingsPending) {
        refreshCombo->addItem(QString("%1 秒").arg(refresh / 1000.0), refresh);
        combo = refreshCombo->count() - 1;
    }
    if (combo >= 0 && !settingsDirty && !settingsPending)
        refreshCombo->setCurrentIndex(combo);
    loadingSettings = false;
    refreshTimer.setInterval(qBound(500, refresh, 10000));
    refreshValue->setText(QString("数据刷新  %1 秒").arg(refresh / 1000.0, 0, 'g', 2));
    int cycle = settings.value("page_cycle_seconds").toInt();
    if (cycle > 0)
        pageTimer.start(cycle * 1000);
    else
        pageTimer.stop();
}

void MainWindow::updateAuditFilter()
{
    int visible = 0;
    for (int row = 0; row < auditTable->rowCount(); ++row) {
        const auto *item = auditTable->item(row, 3);
        const bool show =
            auditResult->currentData().toString().isEmpty() ||
            (item && item->data(Qt::UserRole).toString() == auditResult->currentData().toString());
        auditTable->setRowHidden(row, !show);
        visible += show;
    }
    if (auto *details=findChild<QPushButton *>("auditDetails")) {
        const int row=auditTable->currentRow();
        details->setEnabled(row>=0 && !auditTable->isRowHidden(row) && auditTable->item(row,3));
    }
    static_cast<DataTable *>(auditTable)->setEmptyText(auditTable->rowCount()==0
        ? "尚未加载操作记录；可以刷新记录。"
        : "没有匹配记录；请选择全部执行结果。");
    auditCount->setText(
        QString("显示 %1 / %2 条记录 · 最近 200 条").arg(visible).arg(auditTable->rowCount()));
}
