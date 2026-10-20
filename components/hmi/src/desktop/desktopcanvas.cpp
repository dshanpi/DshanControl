#include "desktopcanvas.h"

namespace DesktopV1
{
DesktopCanvas::DesktopCanvas(bool fixture, QWidget *parent)
    : QWidget(parent), model(fixture ? DisplayModel::example() : DisplayModel::unavailable())
{
    setObjectName("componentDesktop");
    animationsEnabled=!fixture && qEnvironmentVariable("DSHANCONTROL_DESKTOP_ANIMATIONS","1")!="0";
    adaptiveAnimations = !fixture && !qEnvironmentVariableIsSet("DSHANCONTROL_DESKTOP_ANIMATIONS");
    overlayAnimation.setEasingCurve(QEasingCurve::OutCubic);
    dragReturn.setDuration(160);dragReturn.setEasingCurve(QEasingCurve::OutCubic);
    connect(&dragReturn,&QVariantAnimation::valueChanged,this,[this](const QVariant &value){ghostOffset=value.toPointF();update();});
    connect(&dragReturn,&QVariantAnimation::finished,this,[this](){ghostSlot=-1;ghostOffset=QPointF();hits.clear();update();});
    connect(&overlayAnimation,&QVariantAnimation::valueChanged,this,[this](const QVariant &value){overlayProgress=value.toReal();update();});
    connect(&overlayAnimation, &QVariantAnimation::finished, this, &DesktopCanvas::completeOverlay);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_AcceptTouchEvents);
    resize(1024, 600);
    setMinimumSize(1024, 600);
    pointClock.start();
    committedLayout = DesktopLayoutStore::defaults();
    if (!fixture)
    {
        imageCache = new DesktopImageCache(this);
        connect(imageCache, &DesktopImageCache::changed, this,
                [this](const QString &) { update(); });
    }
    if (!fixture)
        configureLayoutStorage(QDir(qEnvironmentVariable("DSHANCONTROL_DESKTOP_STATE_DIR",
                                                         "/mnt/UDISK/dshancontrol/desktop"))
                                   .filePath("layout-v1.json"));
    if (fixture)
        media.load(":/desktop/assets/reference-poster.png");
    else
        brightness = -1;
    search = new QLineEdit(this);
    search->setObjectName("applicationSearch");
    search->setPlaceholderText("搜索应用");
    search->setStyleSheet("QLineEdit{border:1px solid #e2e9f3;border-radius:12px;"
                          "background:#f3f6fb;padding:8px 16px;color:#203958;font-size:14px;}");
    search->hide();
    if (!fixture)
    {
        keyboard = new DesktopKeyboard(this);
        search->installEventFilter(this);
    }
    connect(search, &QLineEdit::textChanged, this, [this](const QString &s) {
        query = s;
        update();
    });
    if (!fixture)
    {
        auto *clock = new QTimer(this);
        const auto refresh = [this]() {
            const auto now = QDateTime::currentDateTime();
            const QStringList days = {"星期一", "星期二", "星期三", "星期四",
                                      "星期五", "星期六", "星期日"};
            const auto clockText = now.toString("HH:mm");
            const auto dateText = now.toString("MM月dd日  ") + days.value(now.date().dayOfWeek() - 1);
            const auto controlDateText = now.toString("yyyy年MM月dd日  ") + days.value(now.date().dayOfWeek() - 1);
            if (model.clock != clockText || model.date != dateText || model.controlDate != controlDateText)
            {
                model.clock = clockText;
                model.date = dateText;
                model.controlDate = controlDateText;
                update();
            }
        };
        connect(clock, &QTimer::timeout, this, refresh);
        clock->start(1000);
        refresh();
    }
}

void DesktopCanvas::completeOverlay()
{
    animatedOverlay = -1;
    overlayProgress = 1;
    hits.clear();
    overlayBaseCache = QImage();
    overlayPanelCache = QImage();
    search->setVisible(state == 2);
    emit overlayChanged(state == 1 || state == 2 || state == 4);
    update();
}

bool DesktopCanvas::loadFonts(QString *error)
{
    for (const auto &name : {"DshanSans-Regular.otf", "DshanSans-Bold.otf"})
    {
        const int id = QFontDatabase::addApplicationFont(QString(":/hmi/fonts/") + name);
        if (id < 0 || !QFontDatabase::applicationFontFamilies(id).contains("Dshan Sans"))
        {
            if (error)
                *error = QString("无法加载要求的 Dshan Sans 字体：") + name;
            return false;
        }
    }
    return true;
}

void DesktopCanvas::change(int next)
{
    if(next!=3){dragReturn.stop();ghostSlot=-1;ghostOffset=QPointF();}
    if (next != 1)
        cancelBrightnessDrag();
    if (next != 2 && keyboard)
        keyboard->dismiss();
    if (!model.fixture)
    {
        if (editingLayout && next != 3 && next != 4 && !resolveLayoutExit())
            return;
        if ((next == 3 || next == 4) && !editingLayout)
        {
            draftLayout = committedLayout;
            editingLayout = true;
            layoutNotice.clear();
        }
        if (next == 4 && state != 4) {
            libraryReturnState = state == 3 ? 3 : 0;
            libraryGroup = -1;
        }
    }
    if(state==4 && next!=4) { libraryGroup=-1;librarySlot=-1; }
    hits.clear(); // A second event before the next paint must not use the old screen's targets.
    const int previous=state;
    state = next;
    animateOverlay(previous,next);
    search->setVisible(next == 2 && !overlayTransitionActive());
    if (next != 2)
    {
        search->clear();
        search->clearFocus();
        query.clear();
    }
    emit overlayChanged(next == 1 || next == 2 || next == 4 || overlayTransitionActive());
    update();
}

void DesktopCanvas::openApp(int index)
{
    if (!model.fixture && state == 3)
        return;
    const auto apps = catalog();
    if (index < 0 || index >= apps.size())
        return;
    selected = index;
    if (model.fixture)
    {
        if (index == 3)
            change(5);
        return;
    }
    hidden.clear();
    change(0);
    emit appRequested(apps[index].id);
}

void DesktopCanvas::renderState(int next, int page)
{
    desk = page;
    selected = 3;
    change(next);
}

void DesktopCanvas::hit(QRectF rect, std::function<void()> function)
{
    hits.append(qMakePair(rect, function));
}

void DesktopCanvas::paintEvent(QPaintEvent *)
{
    QElapsedTimer paintBudget;
    paintBudget.start();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.scale(width() / 1024.0, height() / 600.0);
    hits.clear();
    const auto drawBase = [this](QPainter &p) {
    background(p);
    if (!appActive)
    {
        if (state == 5 && model.fixture)
            detail(p);
        else
            desktop(p, state == 3);
    }
    if(!model.fixture && state==3 && ghostSlot>=0) {
        p.save();p.setClipRect(QRectF(0,53,1024,489));p.setOpacity(.88);
        const auto widget=widgetAt(ghostSlot);
        drawWidget(p,widget["type"].toString(),slotRects()[ghostSlot].translated(ghostOffset),true,widget["binding"].toObject());
        p.restore();
    }
    footer(p);
    status(p);
    };
    const auto drawPanel = [this](QPainter &p, int panel) {
        if (panel == 1) control(p);
        if (panel == 2) apps(p);
        if (panel == 4) library(p);
    };
    const bool animated = overlayTransitionActive();
    const int panel = animated ? animatedOverlay : state;
    bool cached = false;
    if (animated)
    {
        const qreal dpr = devicePixelRatioF();
        const QSize pixels(qRound(width() * dpr), qRound(height() * dpr));
        if (qint64(pixels.width()) * pixels.height() <= 4 * 1024 * 1024)
        {
            if (overlayBaseCache.size() != pixels || overlayBaseCache.devicePixelRatioF() != dpr)
            {
                overlayBaseCache = QImage(pixels, QImage::Format_ARGB32_Premultiplied);
                overlayPanelCache = QImage(pixels, QImage::Format_ARGB32_Premultiplied);
                if (!overlayBaseCache.isNull() && !overlayPanelCache.isNull())
                {
                    overlayBaseCache.setDevicePixelRatio(dpr);
                    overlayPanelCache.setDevicePixelRatio(dpr);
                    overlayBaseCache.fill(Qt::transparent);
                    overlayPanelCache.fill(Qt::transparent);
                    const auto prepare = [this](QPainter &target) {
                        target.setRenderHint(QPainter::Antialiasing);
                        target.setRenderHint(QPainter::SmoothPixmapTransform);
                        target.scale(width() / 1024.0, height() / 600.0);
                    };
                    QPainter base(&overlayBaseCache);
                    prepare(base);
                    drawBase(base);
                    QPainter layer(&overlayPanelCache);
                    prepare(layer);
                    skipDim = true;
                    drawPanel(layer, panel);
                    skipDim = false;
                }
            }
            cached = !overlayBaseCache.isNull() && !overlayPanelCache.isNull();
        }
    }
    // During the 220ms transition only composite two bounded raster layers.
    // Both layers use the normal production drawing functions; the final frame
    // renders live data again, so no stale snapshot survives the transition.
    if (cached)
        p.drawImage(QRectF(0, 0, 1024, 600), overlayBaseCache);
    else
        drawBase(p);
    if (animated)
    {
        p.save(); p.setOpacity(overlayProgress); dim(p); p.restore();
        p.save(); p.setClipRect(QRectF(0, 53, 1024, 547)); p.setOpacity(overlayProgress);
        if (panel == 1) p.translate(0, -450 * (1 - overlayProgress));
        else if (panel == 2) p.translate(0, 510 * (1 - overlayProgress));
        else if (panel == 4) p.translate(400 * (1 - overlayProgress), 0);
        skipDim = true;
    }
    if (cached)
        p.drawImage(QRectF(0, 0, 1024, 600), overlayPanelCache);
    else
        drawPanel(p, panel);
    if (animated) { skipDim = false; p.restore(); hits.clear(); }
    if (state == 3)
    {
        pill(p, QRectF(650, 75, 169, 32), "＋ 添加组件");
        hit(QRectF(645, 64, 174, 44), [this]() { change(4); });
        if (!model.fixture)
        {
            pill(p, QRectF(500, 72, 125, 36), "恢复默认");
            hit(QRectF(490, 64, 145, 44), [this]() {
                if (QMessageBox::question(this, "恢复工作台默认布局",
                                          "恢复两页默认组件和未绑定状态？当前布局文件会"
                                          "先保留一份备份，尚未保存的草稿将丢弃。",
                                          QMessageBox::Yes | QMessageBox::No,
                                          QMessageBox::No) == QMessageBox::Yes)
                    restoreDefaultLayout();
            });
        }
        if (!model.fixture)
            txt(p, 24, 525, 700, 17,
                layoutNotice.isEmpty()
                    ? QString("拖动交换兼容组件 · 点击组件配置绑定 · 完成编辑后保存 "
                              "· Esc 选择退出")
                    : layoutNotice,
                11, QColor("#71859D"));
    }
    if (!model.fixture && state == 0 && !layoutNotice.isEmpty())
        txt(p, 24, 525, 800, 17, layoutNotice, 11, QColor("#71859D"));
    if (keyboard)
        keyboard->setGeometry(qRound(24 * width() / 1024.0), qRound(332 * height() / 600.0),
                              qRound(976 * width() / 1024.0), qRound(260 * height() / 600.0));
    search->setGeometry(int(692 * width() / 1024.0), int(126 * height() / 600.0),
                        int(228 * width() / 1024.0), int(39 * height() / 600.0));
    p.end();
    // The approved low-performance policy preserves the exact final state
    // instead of scheduling a 30fps animation the raster device cannot draw.
    // Explicit 0/1 configuration remains deterministic for deployments/tests.
    if (adaptiveAnimations && animationsEnabled && paintBudget.nsecsElapsed() > 33333333)
    {
        animationsEnabled = false;
        qInfo("Desktop animations disabled: raster paint exceeded the 30fps budget");
        if (overlayTransitionActive())
        {
            overlayAnimation.stop();
            completeOverlay();
        }
    }

}

void DesktopCanvas::previewBrightness(qreal logicalX)
{
    brightness = qBound(4, qRound((logicalX - 200) * 100.0 / 274.0), 100);
    update();
}

void DesktopCanvas::cancelBrightnessDrag()
{
    if (!brightnessDragging)
        return;
    brightnessDragging = false;
    brightness = brightnessBeforeDrag;
    update();
}

bool DesktopCanvas::event(QEvent *event)
{
    if (event->type() == QEvent::TouchCancel || event->type() == QEvent::WindowDeactivate ||
        event->type() == QEvent::Hide)
    {
        cancelBrightnessDrag();
        if (keyboard)
            keyboard->dismiss();
        dragSlot = dragTarget = -1;
        dragReturn.stop();ghostSlot=-1;ghostOffset=QPointF();
        pressAge.invalidate();
    }
    return QWidget::event(event);
}

void DesktopCanvas::finishBrightness(bool success, int rawValue, const QString &message)
{
    if (model.fixture)
        return;
    brightnessPending = false;
    brightnessDragging = false;
    const bool valid = success && rawValue >= 10 && rawValue <= 255;
    brightnessUnconfirmed = !valid;
    brightnessExpectedRaw = valid ? rawValue : -1;
    brightness = valid ? qRound(rawValue * 100.0 / 255.0) : -1;
    brightnessMessage = message;
    update();
}

void DesktopCanvas::mouseMoveEvent(QMouseEvent *event)
{
    if (brightnessDragging)
    {
        previewBrightness(event->pos().x() * 1024.0 / width());
        event->accept();
        return;
    }
    if (!model.fixture && state == 3 && dragSlot >= 0 &&
        QLineF(pressed,
               QPointF(event->pos().x() * 1024.0 / width(), event->pos().y() * 600.0 / height()))
                .length() >= 12)
    {
        dragTarget = slotAt(
            QPointF(event->pos().x() * 1024.0 / width(), event->pos().y() * 600.0 / height()));
        ghostSlot=dragSlot;
        ghostOffset=QPointF(event->pos().x()*1024.0/width(),event->pos().y()*600.0/height())-pressed;
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void DesktopCanvas::mousePressEvent(QMouseEvent *event)
{
    if (overlayTransitionActive() || dragReturning() || event->button() != Qt::LeftButton)
    {
        pressAge.invalidate();
        event->ignore();
        return;
    }
    pressAge.start();
    pressed = QPointF(event->pos().x() * 1024.0 / width(), event->pos().y() * 600.0 / height());
    dragSlot = !model.fixture && state == 3 ? slotAt(pressed) : -1;
    dragTarget = -1;
    if (dragSlot >= 0)
    {
        const auto rect = slotRects()[dragSlot];
        if (QRectF(rect.right() - 44, rect.top() - 14, 44, 44).contains(pressed))
            dragSlot = -1;
    }
    if (event->button() == Qt::LeftButton && !model.fixture && state == 1 &&
        QRectF(190, 360, 294, 65).contains(pressed) && !QApplication::activeModalWidget())
    {
        if (!model.online || brightnessPending)
        {
            pressAge.invalidate();
            return;
        }
        brightnessBeforeDrag = brightness;
        brightnessDragging = true;
        brightnessMessage.clear();
        previewBrightness(pressed.x());
    }
}

void DesktopCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    if (!pressAge.isValid() || event->button() != Qt::LeftButton)
        return;
    const qint64 held = pressAge.elapsed();
    pressAge.invalidate();
    const QPointF point(event->pos().x() * 1024.0 / width(), event->pos().y() * 600.0 / height());
    const qreal dx = point.x() - pressed.x(), dy = point.y() - pressed.y();
    if (brightnessDragging)
    {
        if (!model.online || state != 1 || QApplication::activeModalWidget())
        {
            cancelBrightnessDrag();
            pressAge.invalidate();
            return;
        }
        previewBrightness(point.x());
        brightnessDragging = false;
        brightnessPending = true;
        brightnessMessage = "正在保存并应用背光…";
        pressAge.invalidate();
        emit brightnessRequested(brightness);
        update();
        return;
    }
    if (QApplication::activeModalWidget())
        return;
    if (!model.fixture && state == 3 && dragSlot >= 0 && QLineF(pressed, point).length() >= 12)
    {
        const int from = dragSlot, to = slotAt(point);
        dragSlot = dragTarget = -1;
        const bool moved=to>=0 && to!=from && swapWidgets(from,to);
        if(!moved && animationsEnabled) {
            ghostSlot=from;ghostOffset=point-pressed;
            dragReturn.setStartValue(ghostOffset);dragReturn.setEndValue(QPointF());dragReturn.start();
        } else {ghostSlot=-1;ghostOffset=QPointF();}
        update();
        return;
    }
    dragSlot = dragTarget = -1;
    if (state == 0 && !appActive && slotAt(pressed) >= 0 && held >= 600 &&
        QLineF(pressed, point).length() < 12)
    {
        change(3);
        return;
    }
    if (pressed.y() < 64 && dy > 55)
    {
        change(1);
        return;
    }
    if (pressed.y() > 530 && dy < -55)
    {
        change(2);
        return;
    }
    if ((state == 1 && dy < -55) || (state == 2 && dy > 55))
    {
        change(0);
        return;
    }
    if (state == 0 && !appActive && qAbs(dx) > 100 && qAbs(dx) > qAbs(dy))
    {
        desk = 1 - desk;
        hits.clear();
        update();
        return;
    }
    if (QLineF(pressed, point).length() >= 12)
        return;
    for (int i = hits.size() - 1; i >= 0; --i)
        if (hits[i].first.contains(point))
        {
            const auto action = hits[i].second;
            action();
            return;
        }
}

void DesktopCanvas::mouseDoubleClickEvent(QMouseEvent *event)
{
    const QPointF point(event->pos().x() * 1024.0 / width(), event->pos().y() * 600.0 / height());
    if (event->button() == Qt::LeftButton && state == 0 && !appActive && slotAt(point) >= 0)
        change(3);
}

void DesktopCanvas::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
    {
        if (dismissKeyboardFirst() || backLibraryLevel())
            return;
        if (state != 0)
            change(0);
        else if (appActive)
            emit homeRequested();
    }
    else if (event->key() == Qt::Key_F2 && !appActive)
        change(3);
    else
        QWidget::keyPressEvent(event);
}
} // namespace DesktopV1

namespace DesktopV1
{
bool DesktopCanvas::dismissKeyboardFirst()
{
    if (!keyboard || !keyboard->isVisible())
        return false;
    keyboard->escape();
    return true;
}
bool DesktopCanvas::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == search && keyboard && state == 2 &&
        (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::FocusIn))
    {
        if (!keyboard->isVisible())
            keyboard->showFor(search);
    }
    return QWidget::eventFilter(watched, event);
}
} // namespace DesktopV1

namespace DesktopV1 {
void DesktopCanvas::animateOverlay(int previous,int next)
{
    if(!animationsEnabled || previous==next)return;
    const auto overlay=[](int value){return value==1 || value==2 || value==4;};
    const int prior=overlayTransitionActive()?animatedOverlay:previous;
    const qreal start=overlayTransitionActive()?overlayProgress:1;
    overlayAnimation.stop();
    overlayBaseCache = QImage(); overlayPanelCache = QImage();
    if(!overlay(next) && !overlay(prior)) {animatedOverlay=-1;return;}
    animatedOverlay=overlay(next)?next:prior;
    const qreal from=overlay(next)?(prior==next?start:0):start;
    const qreal to=overlay(next)?1:0;
    overlayAnimation.setStartValue(from);overlayAnimation.setEndValue(to);
    overlayAnimation.setDuration(qMax(1,qRound(220*qAbs(to-from))));
    overlayAnimation.start();
}
}

namespace DesktopV1
{
void DesktopCanvas::observeBrightness(int rawValue, const QString &error)
{
    if (model.fixture || brightnessDragging || brightnessPending)
        return;
    const bool valid = rawValue >= 0 && rawValue <= 255 && error.isEmpty();
    brightness = valid ? qRound(rawValue * 100.0 / 255.0) : -1;
    brightnessUnconfirmed = !valid;
    brightnessExpectedRaw = -1;
    if (!valid)
        brightnessMessage = error;
    else if (brightnessMessage.contains("亮度不可用") || brightnessMessage.contains("亮度超时") ||
             brightnessMessage.contains("亮度服务") || brightnessMessage.contains("驱动确认"))
        brightnessMessage.clear();
    update();
}
}
