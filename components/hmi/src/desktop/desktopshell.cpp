#include "desktopshell.h"
#include "../mainwindow.h"

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
    toolbar->setFixedHeight(50);
    toolbar->setStyleSheet("background:#eef3f9;color:#142c4b;");
    auto *bar = new QHBoxLayout(toolbar);
    bar->setContentsMargins(24, 3, 24, 3);
    applicationTitle = new QLabel;
    applicationTitle->setTextFormat(Qt::PlainText);
    applicationTitle->setStyleSheet("font-size:22px;font-weight:600;");
    bar->addWidget(applicationTitle, 1);
    auto *back = new QPushButton("← 返回工作台");
    back->setObjectName("backToDesktop");
    back->setMinimumSize(162, 44);
    back->setStyleSheet("QPushButton{border:0;border-radius:20px;background:#e5efff;"
                        "color:#1468fa;font-size:14px;font-weight:600;}");
    bar->addWidget(back);
    layout->addWidget(toolbar);
    applications = new MainWindow;
    applications->setDesktopPointSource(surface);
    applications->setDesktopEmbedded(true);
    layout->addWidget(applications, 1);
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
    applicationTitle->setText(catalog()[ApplicationRegistry::indexForId(id)].name);
    const bool changed = activeId != id;
    activeId = id;
    surface->change(0);
    surface->appActive = true;
    surface->update();
    applicationFrame->setVisible(!surface->overlayTransitionActive());
    if(!surface->overlayTransitionActive())applicationFrame->raise();
    if (changed) emit activeApplicationChanged(activeId);
    return true;
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
