#include "desktopshell.h"
#include "applicationguide.h"
#include "../mainwindow.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QTabBar>

namespace DesktopV1
{
DesktopShell::DesktopShell(QWidget *parent) : QWidget(parent)
{
    setObjectName("desktopShell");
    setWindowTitle("DshanControl · 组件桌面");
    resize(1024, 600);
    setMinimumSize(1024, 600);
    surface = new DesktopCanvas(false, this);
    applicationFrame = new QWidget(this);
    auto *layout = new QVBoxLayout(applicationFrame);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    toolbar = new QWidget;
    toolbar->setFixedHeight(54);
    toolbar->setStyleSheet("QWidget{background:#ffffff;color:#161616;font-family:'Dshan Sans';}"
                          "QPushButton{min-height:46px;padding:0 12px;border:1px solid #a8a8a8;border-radius:2px;color:#0f62fe;}"
                          "QPushButton:focus{border:2px solid #0f62fe;}");
    auto *bar = new QHBoxLayout(toolbar);
    bar->setContentsMargins(16, 2, 16, 2);
    applicationTitle = new QLabel;
    applicationTitle->setTextFormat(Qt::PlainText);
    applicationTitle->setObjectName("applicationLocation");
    applicationTitle->setStyleSheet("font-size:16px;font-weight:600;");
    applicationTitle->setWordWrap(false);
    applicationTitle->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
    applicationTitle->setMinimumWidth(0);
    auto *context = new QVBoxLayout;
    context->setSpacing(0); context->addWidget(applicationTitle);
    pageHint = new QLabel; pageHint->setObjectName("applicationPageHint");
    pageHint->setTextFormat(Qt::PlainText); pageHint->setStyleSheet("font-size:13px;color:#525252;");
    pageHint->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
    context->addWidget(pageHint); bar->addLayout(context, 1);
    auto *guide = new QPushButton("本页步骤");
    guide->setObjectName("showApplicationGuide");
    bar->addWidget(guide);
    connect(guide,&QPushButton::clicked,this,[this]() {
        ApplicationGuide dialog(activeId,applicationTitle->text(),this,activeHelpKey);
        if(dialog.exec()==QDialog::Accepted && !dialog.target.isEmpty())
            openApplication(dialog.target);
    });
    auto *switcher = new QPushButton("全部应用");
    switcher->setObjectName("switchApplication");
    bar->addWidget(switcher);
    connect(switcher, &QPushButton::clicked, this, &DesktopShell::chooseApplication);
    auto *back = new QPushButton("返回工作台");
    back->setObjectName("backToDesktop");
    back->setMinimumSize(132, 48);
    bar->addWidget(back);
    layout->addWidget(toolbar);
    applications = new MainWindow;
    applications->setDesktopPointSource(surface);
    applications->setDesktopEmbedded(true);
    connect(applications, &MainWindow::applicationNavigationRequested, this,
            [this](const QString &id) { openApplication(id); });
    layout->addWidget(applications, 1);
    const auto watchSections = [this](const QString &name, const QStringList &routes) {
        if (auto *tabs = applications->findChild<QTabWidget *>(name))
            connect(tabs, &QTabWidget::currentChanged, this, [this, tabs, routes](int index) {
                if (!surface->appActive || !tabs->isVisibleTo(applications)) return;
                const QString id = routes.value(index, activeId);
                recordApplication(id);
            });
    };
    watchSections("cloudTabs", {"cloud_bind", "reporting"});
    watchSections("familyTabs", {"voice", "home"});
    watchSections("settingsTabs", {"settings", "settings", "settings", "update"});
    watchSections("protocolTabs", {"fieldbus", "fieldbus", "fieldbus"});
    watchSections("canControls", {"fieldbus", "fieldbus"});
    watchSections("networkTabs", {"network", "network", "network"});
    for (const QString &name : {QString("mediaPages"), QString("devicePageStack")})
        if (auto *stack=applications->findChild<QStackedWidget *>(name))
            connect(stack,&QStackedWidget::currentChanged,this,[this,stack]() {
                if (surface->appActive && stack->isVisibleTo(applications)) recordApplication(activeId);
            });
    if (auto *tabs=applications->findChild<QTabBar *>("alarmTabs"))
        connect(tabs,&QTabBar::currentChanged,this,[this,tabs]() {
            if (surface->appActive && tabs->isVisibleTo(applications)) recordApplication("alarms");
        });
    applicationFrame->hide();
    connect(back, &QPushButton::clicked, this, &DesktopShell::backToDesktop);
    connect(surface, &DesktopCanvas::homeRequested, this, &DesktopShell::backToDesktop);
    connect(surface, &DesktopCanvas::appRequested, this, [this](const QString &id){openApplication(id);});
    connect(surface, &DesktopCanvas::appContextRequested, this, &DesktopShell::openApplication);
    brightnessReader = new DisplayBrightnessReader([this](int value, const QString &error) {
        surface->observeBrightness(value, error);
    }, this);
    connect(surface, &DesktopCanvas::brightnessRequested, this, [this](int percent) {
        brightnessReader->setSuspended(true);
        if (!applications->requestDesktopBrightness(percent))
        {
            if (surface->brightnessBusy())
                surface->finishBrightness(false, -1, "已有设置请求正在处理，请稍后重试");
            brightnessReader->setSuspended(false);
        }
    });
    connect(applications, &MainWindow::desktopBrightnessFinished, surface,
            &DesktopCanvas::finishBrightness);
    connect(applications, &MainWindow::desktopBrightnessFinished, this,
            [this](bool, int, const QString &) { brightnessReader->setSuspended(false); });
    connect(applications, &MainWindow::hmiSnapshotChanged, surface, &DesktopCanvas::applyHmi);
    connect(applications, &MainWindow::desktopSnapshotChanged, surface,
            &DesktopCanvas::applyServices);
    connect(surface, &DesktopCanvas::overlayChanged, this, [this](bool covered) {
        brightnessReader->setActive(surface->state == 1);
        applicationFrame->setVisible(surface->appActive && !covered);
        if (!covered && surface->appActive)
            applicationFrame->raise();
    });
    connect(applications, &MainWindow::mediaFullscreenChanged, this, [this](bool enabled) {
        fullscreen = enabled;
        toolbar->setVisible(!enabled);
        positionContent();
    });
    auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    escape->setContext(Qt::WidgetWithChildrenShortcut);
    connect(escape, &QShortcut::activated, this, [this]() {
        if (QApplication::activeModalWidget())
            return;
        if (surface->dismissKeyboardFirst() || surface->backLibraryLevel()) return;
        if (surface->state != 0)
            surface->change(0);
        else if (surface->appActive && !fullscreen)
            backToDesktop();
    });
    positionContent();
}

bool DesktopShell::openApplication(const QString &id, const QJsonObject &context)
{
    if (!applications->openApplication(id,context))
        return false;
    recordApplication(id);
    surface->change(0);
    surface->appActive = true;
    surface->update();
    applicationFrame->setVisible(!surface->overlayTransitionActive());
    if(!surface->overlayTransitionActive())applicationFrame->raise();
    return true;
}


void DesktopShell::recordApplication(const QString &id)
{
    const int index=ApplicationRegistry::indexForId(id);
    if (index<0) return;
    QString location="工作台 / " + catalog()[index].name;
    const QString tabName = id=="fieldbus" ? "protocolTabs" : id=="network" ? "networkTabs"
                           : id=="settings" ? "settingsTabs" : QString();
    auto *tabs=tabName.isEmpty() ? nullptr : applications->findChild<QTabWidget *>(tabName);
    if (tabs) {
        location += " / " + tabs->tabText(tabs->currentIndex());
        if (id=="fieldbus" && tabs->currentIndex()==0)
            if (auto *can=applications->findChild<QTabWidget *>("canControls"))
                location += " / " + can->tabText(can->currentIndex());
    }
    activeHelpKey=id;
    if (tabs) {
        if (id=="fieldbus") {
            activeHelpKey=QStringList{"fieldbus.nmt","fieldbus.rs485","fieldbus.ethercat"}.value(tabs->currentIndex(),id);
            if (tabs->currentIndex()==0)
                if (auto *can=applications->findChild<QTabWidget *>("canControls"))
                    activeHelpKey=can->currentIndex()==1 ? "fieldbus.sdo" : "fieldbus.nmt";
        } else if (id=="network") {
            activeHelpKey=QStringList{"network.wired","network.wifi","network.cellular"}.value(tabs->currentIndex(),id);
        } else if (id=="settings") {
            activeHelpKey=QStringList{"settings.display","settings.services","settings.audit","update"}.value(tabs->currentIndex(),id);
        }
    }
    if (id=="video")
        if (auto *stack=applications->findChild<QStackedWidget *>("mediaPages")) {
            activeHelpKey=QStringList{"video.player","video.library","video.settings"}.value(stack->currentIndex(),id);
            location+=" / "+QStringList{"播放器","本地视频","播放设置"}.value(stack->currentIndex());
        }
    if (id=="devices")
        if (auto *stack=applications->findChild<QStackedWidget *>("devicePageStack"))
            if (stack->currentIndex()==1) {activeHelpKey="devices.trend";location+=" / 绑定点位趋势";}
    if (id=="alarms")
        if (auto *filter=applications->findChild<QTabBar *>("alarmTabs")) {
            activeHelpKey=QStringList{"alarms.all","alarms.active","alarms.acknowledged"}.value(filter->currentIndex(),id);
            location+=" / "+filter->tabText(filter->currentIndex());
        }
    static const QMap<QString,QString> hints = {
        {"overview","先核对数据来源，再进入需要处理的应用"},
        {"fieldbus","核对通道与目标 → 确认执行 → 核对设备"},
        {"network","选择接口 → 刷新状态 → 验证目标服务"},
        {"devices","核对点位与采样质量，再查看趋势"},
        {"alarms","筛选并选择记录 → 查看详情 → 按需确认"},
        {"settings","核对当前子页与操作目标，再查看最近结果"},
        {"cloud_bind","核对连接 → 查询绑定码 → 核对账号归属"},
        {"reporting","核对配置和数据 → 上报 → 云端核对"},
        {"voice","确认语音服务 → 开始对话 → 结束对话"},
        {"home","选择设备和功能 → 控制 → 等待状态回报"},
        {"video","选择文件 → 核对名称 → 点击播放"},
        {"update","检查版本 → 下载校验 → 维护时安装"}
    };
    static const QMap<QString,QString> sectionHints = {
        {"fieldbus.nmt","核对节点；节点 0 为广播 → 发送 NMT"},
        {"fieldbus.sdo","核对节点、对象地址与 4 字节写入值"},
        {"fieldbus.rs485","填写首个从站配置 → 保存 → 验证采样"},
        {"fieldbus.ethercat","核对从站与对象；写入和周期测试分开执行"},
        {"network.wired","选择接口 → 刷新；IP 配置由系统服务管理"},
        {"network.wifi","选择无线接口 → 刷新；本页只查询连接"},
        {"network.cellular","核对模块 → 拨号或断开 → 核对地址"},
        {"settings.display","编辑或撤销 → 保存并应用 → 核对底部结果"},
        {"settings.services","核对服务 → 确认操作 → 刷新并查看日志"},
        {"settings.audit","筛选 → 选择记录 → 查看选中详情"},
        {"devices.trend","核对绑定点位与采样质量 → 查看趋势"},
        {"video.player","选择文件 → 核对名称 → 点击播放"},
        {"video.library","选择本地文件 → 返回播放器 → 点击播放"},
        {"video.settings","选择比例与循环 → 返回播放器验证"},
        {"alarms.all","筛选并选择记录 → 查看详情 → 按需确认"},
        {"alarms.active","确认后记录移至已确认页；现场故障仍需处理"},
        {"alarms.acknowledged","已确认表示已知悉；现场故障恢复需另行核对"}
    };
    pageHint->setText(sectionHints.value(activeHelpKey,hints.value(id)));
    applicationTitle->setText(location);
    const bool changed=activeId!=id;
    activeId=id;
    if (changed) emit activeApplicationChanged(activeId);
}

void DesktopShell::chooseApplication()
{
    QDialog dialog(this);
    dialog.setObjectName("applicationSwitcher");
    dialog.setWindowTitle("全部应用 · 选择要执行的任务");
    dialog.setStyleSheet(
        "QDialog{background:#f4f4f4;}"
        "QWidget{font-family:'Dshan Sans';font-size:16px;color:#161616;}"
        "QLabel{font-weight:600;}"
        "QPushButton{min-height:54px;background:white;border:1px solid #a8a8a8;border-radius:2px;padding:4px 12px;text-align:left;}"
        "QPushButton:checked{background:#d0e2ff;color:#0043ce;border-left:4px solid #0f62fe;}"
        "QPushButton:focus{border:2px solid #0f62fe;}");
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(20,16,20,16);
    layout->addWidget(new QLabel("选择应用 · 当前应用已标记"));
    auto *grid = new QGridLayout;
    layout->addLayout(grid,1);
    const QStringList categories{"industrial","network_cloud","system_media"};
    const QStringList headings{"现场与设备","连接与家庭","系统与媒体"};
    int rows[3] = {1,1,1};
    for (int i=0;i<3;++i) {
        grid->addWidget(new QLabel(headings[i]),0,i);
        grid->setColumnStretch(i,1);
    }
    QString selected;
    for (const auto &app : catalog()) {
        const int column=categories.indexOf(app.category);
        if (column<0) continue;
        auto *button=new QPushButton(app.name + "\n" + app.sub);
        button->setObjectName("appRoute_" + app.id);
        button->setCheckable(true);
        button->setChecked(app.id==activeId);
        button->setAccessibleName(app.name);
        grid->addWidget(button,rows[column]++,column);
        connect(button,&QPushButton::clicked,&dialog,[&dialog,&selected,app]() {
            selected=app.id;
            dialog.accept();
        });
    }
    auto *cancel=new QPushButton("取消 · 留在当前应用");
    cancel->setObjectName("cancelApplicationSwitch");
    layout->addWidget(cancel);
    connect(cancel,&QPushButton::clicked,&dialog,&QDialog::reject);
    dialog.resize(qMin(940,width()-32),qMin(560,height()-24));
    if (dialog.exec()==QDialog::Accepted && !selected.isEmpty())
        openApplication(selected);
}

void DesktopShell::backToDesktop()
{
    applicationFrame->hide();
    surface->appActive = false;
    surface->change(0);
    surface->setFocus();
    if (!activeId.isEmpty()) {
        activeId.clear();
        emit activeApplicationChanged(activeId);
    }
}

void DesktopShell::positionContent()
{
    surface->setGeometry(rect());
    applicationFrame->setGeometry(fullscreen ? rect()
                                             : QRect(0, qRound(53 * height() / 600.0), width(),
                                                     qRound(489 * height() / 600.0)));
}

void DesktopShell::closeEvent(QCloseEvent *event)
{
    if (!surface->requestLayoutExit()) event->ignore();
    else QWidget::closeEvent(event);
}

void DesktopShell::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    positionContent();
}
} // namespace DesktopV1
