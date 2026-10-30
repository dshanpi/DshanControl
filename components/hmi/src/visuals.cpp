#include "visuals.h"
#include <QApplication>
#include <QDateTime>
#include <QFontDatabase>
#include <QJsonArray>
#include <QMouseEvent>
#include <QPainterPath>
#include <QResizeEvent>
#include <QStyleOptionSlider>
#include <QStyleOptionGroupBox>
#include <QTabBar>
#include <QtMath>
#include <cmath>

void SectionCard::paintEvent(QPaintEvent *event)
{
    QGroupBox::paintEvent(event);
    if (title().isEmpty()) return;
    QStyleOptionGroupBox option;
    initStyleOption(&option);
    const QRect titleRect = style()->subControlRect(QStyle::CC_GroupBox, &option,
                                                    QStyle::SC_GroupBoxLabel, this);
    QPainter painter(this);
    painter.setPen(QColor("#e0e0e0"));
    const int y = titleRect.bottom() + 5;
    painter.drawLine(1, y, width() - 2, y);
}

TouchSlider::TouchSlider(QWidget *parent) : QSlider(Qt::Horizontal, parent)
{
    setMinimumHeight(48);
    setFocusPolicy(Qt::StrongFocus);
}

void TouchSlider::moveTo(int x)
{
    QStyleOptionSlider option;
    initStyleOption(&option);
    const QRect groove = style()->subControlRect(QStyle::CC_Slider, &option,
                                                QStyle::SC_SliderGroove, this);
    const QRect handle = style()->subControlRect(QStyle::CC_Slider, &option,
                                                QStyle::SC_SliderHandle, this);
    const int span = groove.width() - handle.width();
    if (span <= 0)
        return;
    setSliderPosition(QStyle::sliderValueFromPosition(minimum(), maximum(),
        x - dragOffset - groove.x() - handle.width() / 2, span, option.upsideDown));
}

void TouchSlider::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QSlider::mousePressEvent(event);
        return;
    }
    QStyleOptionSlider option;
    initStyleOption(&option);
    const QRect handle = style()->subControlRect(QStyle::CC_Slider, &option,
                                                QStyle::SC_SliderHandle, this);
    const int center = handle.x() + handle.width() / 2;
    const bool onHandle = qAbs(event->pos().x() - center) <= qMax(24, handle.width() / 2);
    dragOffset = onHandle ? event->pos().x() - center : 0;
    setFocus(Qt::MouseFocusReason);
    setSliderDown(true);
    if (!onHandle)
        moveTo(event->pos().x());
    event->accept();
}

void TouchSlider::mouseMoveEvent(QMouseEvent *event)
{
    if (!isSliderDown()) {
        QSlider::mouseMoveEvent(event);
        return;
    }
    moveTo(event->pos().x());
    event->accept();
}

void TouchSlider::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !isSliderDown()) {
        QSlider::mouseReleaseEvent(event);
        return;
    }
    moveTo(event->pos().x());
    setSliderDown(false);
    event->accept();
}

namespace Visuals
{
const QColor ink("#092650"), muted("#526d93"), blue("#0062ff"), cyan("#45b5ff"), green("#24a244"),
    border("#dde8f5");
bool finiteNumber(const QJsonValue &value)
{
    return value.isDouble() && qIsFinite(value.toDouble());
}
QString numberText(const QJsonValue &value, int decimals)
{
    return finiteNumber(value) ? QString::number(value.toDouble(), 'f', decimals) : "--";
}
bool sensorAvailable(const QJsonObject &sensor)
{
    const QString state = sensor["state"].toString();
    return finiteNumber(sensor["value"]) && sensor["source"].toString() != "unavailable" &&
           !(sensor["online"].isBool() && !sensor["online"].toBool()) &&
           state != "离线" && state != "未连接" && state != "offline";
}
bool physicalReading(const QJsonObject &sensor, const QString &name, double *reading)
{
    if (sensor["name"].toString() != name || !sensorAvailable(sensor))
        return false;
    const QString unit = sensor["unit"].toString();
    if ((name == "温度" && unit != "°C") ||
        (name == "压力" && unit != "MPa" && unit != "kPa"))
        return false;
    *reading = sensor["value"].toDouble() / (name == "压力" && unit == "kPa" ? 1000 : 1);
    return true;
}
void initializeFonts()
{
    static bool loaded = false;
    if (loaded)
        return;
    QFontDatabase::addApplicationFont(":/hmi/fonts/DshanSans-Regular.otf");
    QFontDatabase::addApplicationFont(":/hmi/fonts/DshanSans-Bold.otf");
    loaded = true;
}
QFont font(int pixels, bool bold)
{
    QFont f("Dshan Sans");
    f.setPixelSize(pixels);
    f.setWeight(bold ? QFont::Bold : QFont::Normal);
    f.setStyleStrategy(QFont::PreferAntialias);
    return f;
}
void text(QPainter &p, const QRectF &r, const QString &s, int pixels, const QColor &color, bool bold,
          int align)
{
    p.setPen(color);
    p.setFont(font(pixels, bold));
    p.drawText(r, align, p.fontMetrics().elidedText(s, Qt::ElideRight, qMax(0, int(r.width()))));
}
void panel(QPainter &p, const QRectF &r, const QColor &fill)
{
    p.setPen(QPen(border, 1));
    p.setBrush(fill);
    p.drawRoundedRect(r.adjusted(.5, .5, -.5, -.5), 4, 4);
}
void logo(QPainter &p, const QRectF &r)
{
    QLinearGradient gradient(r.topLeft(), r.bottomRight());
    gradient.setColorAt(0, QColor("#0878ff"));
    gradient.setColorAt(1, blue);
    p.setPen(Qt::NoPen);
    p.setBrush(gradient);
    p.drawRoundedRect(r, 4, 4);
    text(p, r.adjusted(0, -1, 0, 0), "D", int(r.height() * .78), Qt::white, true, Qt::AlignCenter);
}
void icon(QPainter &p, const QRectF &r, const QString &name, const QColor &color)
{
    p.save();
    p.translate(r.topLeft());
    p.scale(r.width() / 24, r.height() / 24);
    p.setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    QPainterPath path;
    if (name == "bars") {
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawRect(QRectF(2, 15, 3, 7));
        p.drawRect(QRectF(8, 10, 3, 12));
        p.drawRect(QRectF(14, 4, 3, 18));
        p.drawRect(QRectF(20, 8, 2, 14));
    } else if (name == "topology") {
        p.drawRect(QRectF(9, 2, 6, 6));
        p.drawRect(QRectF(2, 17, 6, 5));
        p.drawRect(QRectF(16, 17, 6, 5));
        p.drawLine(12, 8, 12, 12);
        p.drawLine(5, 12, 19, 12);
        p.drawLine(5, 12, 5, 17);
        p.drawLine(19, 12, 19, 17);
    } else if (name == "microphone") {
        p.drawRoundedRect(QRectF(9, 2, 6, 13), 3, 3);
        p.drawArc(QRectF(6, 7, 12, 12), 180 * 16, 180 * 16);
        p.drawLine(12, 19, 12, 23); p.drawLine(8, 23, 16, 23);
        p.drawLine(3, 3, 21, 21);
    } else if (name == "home") {
        path.moveTo(2, 11); path.lineTo(12, 2); path.lineTo(22, 11);
        path.lineTo(19, 11); path.lineTo(19, 22); path.lineTo(14, 22);
        path.lineTo(14, 15); path.lineTo(10, 15); path.lineTo(10, 22);
        path.lineTo(5, 22); path.lineTo(5, 11); path.closeSubpath();
        p.setBrush(color); p.drawPath(path);
    } else if (name == "ethernet") {
        path.moveTo(3, 3); path.lineTo(21, 3); path.lineTo(21, 17);
        path.lineTo(16, 17); path.lineTo(16, 21); path.lineTo(8, 21);
        path.lineTo(8, 17); path.lineTo(3, 17); path.closeSubpath();
        p.drawPath(path);
        for (int x = 7; x <= 17; x += 2) p.drawLine(x, 7, x, 12);
    } else if (name == "wifi") {
        p.drawArc(QRectF(1, 3, 22, 22), 40 * 16, 100 * 16);
        p.drawArc(QRectF(5, 8, 14, 14), 40 * 16, 100 * 16);
        p.drawArc(QRectF(9, 13, 6, 6), 40 * 16, 100 * 16);
        p.setBrush(color); p.drawEllipse(QPointF(12, 21), 1.2, 1.2);
    } else if (name == "globe") {
        p.drawEllipse(QRectF(2, 2, 20, 20));
        p.drawEllipse(QRectF(8, 2, 8, 20));
        p.drawLine(2, 12, 22, 12);
    } else if (name == "video") {
        p.drawRoundedRect(QRectF(2, 3, 20, 18), 2, 2);
        QPolygonF triangle; triangle << QPointF(10, 7) << QPointF(17, 12) << QPointF(10, 17);
        p.drawPolygon(triangle);
    } else if (name == "play") {
        QPolygonF triangle; triangle << QPointF(7, 3) << QPointF(21, 12) << QPointF(7, 21);
        p.setBrush(color); p.drawPolygon(triangle);
    } else if (name == "pause") {
        p.fillRect(QRectF(6, 4, 4, 16), color); p.fillRect(QRectF(15, 4, 4, 16), color);
    } else if (name == "previous" || name == "next") {
        if (name == "previous") { p.translate(24, 0); p.scale(-1, 1); }
        p.drawLine(19, 4, 19, 20);
        QPolygonF triangle; triangle << QPointF(4, 4) << QPointF(16, 12) << QPointF(4, 20);
        p.setBrush(color); p.drawPolygon(triangle);
    } else if (name == "fullscreen") {
        for (int i = 0; i < 4; ++i) { p.drawLine(3, 8, 3, 3); p.drawLine(3, 3, 8, 3); p.translate(24, 0); p.rotate(90); }
    } else if (name == "volume") {
        QPolygonF speaker; speaker << QPointF(3, 9) << QPointF(7, 9) << QPointF(12, 5)
            << QPointF(12, 19) << QPointF(7, 15) << QPointF(3, 15);
        p.drawPolygon(speaker); p.drawArc(QRectF(10, 6, 9, 12), -65 * 16, 130 * 16);
        p.drawArc(QRectF(10, 2, 14, 20), -65 * 16, 130 * 16);
    } else if (name == "monitor") {
        p.drawRect(QRectF(2, 3, 20, 14));
        p.drawLine(12, 17, 12, 22);
        p.drawLine(7, 22, 17, 22);
        p.drawLine(6, 13, 10, 13);
        p.drawLine(10, 13, 16, 7);
    } else if (name == "sun") {
        p.drawEllipse(QRectF(7, 7, 10, 10));
        for (int i = 0; i < 8; ++i) {
            const double angle = i * M_PI / 4;
            p.drawLine(QPointF(12 + 8 * qCos(angle), 12 + 8 * qSin(angle)),
                       QPointF(12 + 11 * qCos(angle), 12 + 11 * qSin(angle)));
        }
    } else if (name == "type") {
        text(p, QRectF(0, 0, 24, 24), "Aa", 19, color, true, Qt::AlignCenter);
    } else if (name == "bell") {
        path.moveTo(4, 18);
        path.cubicTo(7, 15, 6, 13, 7, 8);
        path.cubicTo(8, 2, 16, 2, 17, 8);
        path.cubicTo(18, 13, 17, 15, 20, 18);
        path.closeSubpath();
        p.drawPath(path);
        p.drawArc(QRectF(9, 18, 6, 4), 180 * 16, 180 * 16);
        p.drawLine(12, 1, 12, 3);
    } else if (name == "settings") {
        QPolygonF gear;
        for (int i = 0; i < 32; i++) {
            double a = i * M_PI / 16;
            double radius = (i % 4 == 0 || i % 4 == 3) ? 10 : 8;
            gear << QPointF(12 + radius * qCos(a), 12 + radius * qSin(a));
        }
        p.drawPolygon(gear);
        p.drawEllipse(QRectF(8, 8, 8, 8));
    } else if (name == "clock") {
        p.drawEllipse(QRectF(2, 2, 20, 20));
        p.drawLine(12, 5, 12, 12);
        p.drawLine(12, 12, 17, 12);
    } else if (name == "server") {
        for (int y : {3, 10, 17}) {
            p.drawRoundedRect(QRectF(2, y, 20, 5), 1, 1);
            p.drawPoint(5, y + 2);
        }
    } else if (name == "cloud") {
        path.moveTo(5, 20);
        path.cubicTo(-2, 20, 0, 10, 6, 11);
        path.cubicTo(7, 0, 19, 2, 19, 10);
        path.cubicTo(26, 10, 25, 20, 19, 20);
        path.closeSubpath();
        p.drawPath(path);
    } else if (name == "thermometer") {
        path.moveTo(9, 15);
        path.lineTo(9, 4);
        path.cubicTo(9, 0, 15, 0, 15, 4);
        path.lineTo(15, 15);
        path.cubicTo(22, 22, 2, 26, 9, 15);
        p.drawPath(path);
        p.setPen(QPen(blue, 1.8));
        p.drawLine(12, 6, 12, 18);
        p.drawEllipse(QRectF(10, 18, 4, 4));
        p.setPen(QPen(color, 1.6));
        for (int y : {6, 10, 14})
            p.drawLine(16, y, 18, y);
    } else if (name == "chip") {
        p.drawRoundedRect(QRectF(5, 5, 14, 14), 2, 2);
        for (int x : {8, 12, 16}) {
            p.drawLine(x, 1, x, 5);
            p.drawLine(x, 19, x, 23);
            p.drawLine(1, x, 5, x);
            p.drawLine(19, x, 23, x);
        }
    } else if (name == "motor") {
        p.drawEllipse(QRectF(1, 5, 20, 16));
        p.setBrush(Qt::white);
        p.drawRect(QRectF(7, 3, 14, 17));
        p.drawLine(12, 1, 16, 1);
        p.drawLine(14, 1, 14, 3);
        p.drawLine(8, 23, 10, 20);
        p.drawLine(19, 23, 17, 20);
        for (int y : {7, 11, 15})
            p.drawLine(11, y, 17, y);
    } else if (name == "warning") {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#d68600"));
        QPolygonF triangle;
        triangle << QPointF(12, 2) << QPointF(23, 22) << QPointF(1, 22);
        p.drawPolygon(triangle);
        p.setPen(QPen(Qt::white, 2));
        p.drawLine(12, 8, 12, 14);
        p.drawPoint(12, 18);
    } else if (name == "document") {
        p.drawRect(QRectF(5, 2, 15, 20));
        p.drawLine(8, 7, 10, 7);
        p.drawLine(14, 7, 17, 7);
        p.drawLine(8, 12, 16, 12);
        p.drawLine(8, 17, 14, 17);
    } else if (name == "gauge") {
        p.drawArc(QRectF(2, 2, 20, 20), 0, 180 * 16);
        p.drawLine(3, 12, 3, 19);
        p.drawLine(21, 12, 21, 19);
        p.drawLine(12, 12, 17, 6);
        p.drawEllipse(QRectF(10, 10, 4, 4));
    } else if (name == "check") {
        p.drawLine(4, 12, 10, 18);
        p.drawLine(10, 18, 21, 5);
    } else if (name == "chevron") {
        p.drawLine(8, 4, 16, 12);
        p.drawLine(16, 12, 8, 20);
    } else if (name == "sun") {
        p.drawEllipse(QRectF(7, 7, 10, 10));
        for (int i = 0; i < 8; i++) {
            double a = i * M_PI / 4;
            p.drawLine(QPointF(12 + 8 * qCos(a), 12 + 8 * qSin(a)),
                       QPointF(12 + 11 * qCos(a), 12 + 11 * qSin(a)));
        }
    } else {
        p.drawEllipse(QRectF(2, 2, 20, 20));
        p.drawLine(12, 10, 12, 17);
        p.drawPoint(12, 6);
    }
    p.restore();
}
QIcon icon(const QString &name, const QColor &color)
{
    QPixmap pixmap(96, 96);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    icon(p, QRectF(0, 0, 96, 96), name, color);
    return QIcon(pixmap);
}
} // namespace Visuals

NavigationButton::NavigationButton(const QString &label, const QString &symbol)
    : QPushButton(label), symbol(symbol)
{
    setCheckable(true);
    setMinimumHeight(44);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}
void NavigationButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QColor color = isChecked() ? Qt::white : Visuals::ink;
    if (isChecked()) {
        QLinearGradient g(0, 0, width(), height());
        g.setColorAt(0, QColor("#0669ff"));
        g.setColorAt(1, QColor("#0757e5"));
        p.setPen(QColor("#237cff"));
        p.setBrush(g);
        p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 4, 4);
    }
    if (width() > 90) {
        Visuals::icon(p, QRectF(15, 12, 22, 22), symbol, color);
        Visuals::text(p, QRectF(49, 0, width() - 52, height()), text(), 15, color, isChecked());
    } else {
        Visuals::icon(p, QRectF((width() - 22) / 2, 4, 22, 22), symbol, color);
        Visuals::text(p, QRectF(0, 27, width(), 17), text(), 11, color, isChecked(), Qt::AlignCenter);
    }
    if (hasFocus() && !isChecked()) {
        p.setPen(QPen(Visuals::blue, 1, Qt::DotLine));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(rect().adjusted(3, 3, -3, -3), 3, 3);
    }
}

OverviewDashboard::OverviewDashboard(bool trendOnly) : trendOnly(trendOnly)
{
    setMinimumSize(320, trendOnly ? 220 : 430);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAccessibleName("运行总览");
}
void OverviewDashboard::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (!trendOnly)
        setMinimumHeight(width() < 760 ? (topologyVisible ? 880 : 740) : 430);
}
void OverviewDashboard::setSnapshot(const QJsonObject &value)
{
    if (!Visuals::finiteNumber(value["timestamp"])) {
        setOffline();
        return;
    }
    data = value;
    connected = true;
    const auto sensors = data.value("sensors").toArray();
    // Use named physical quantities and retain a real time coordinate, not row order.
    const double now = data.value("timestamp").toDouble();
    for (int i = 0; i < 2; ++i) {
        auto &series = i == 0 ? temperatures : pressures;
        bool found = false;
        for (const auto &v : sensors) {
            const auto sensor = v.toObject();
            double reading;
            if (!Visuals::physicalReading(sensor, i == 0 ? "温度" : "压力", &reading))
                continue;
            found = true;
            if (!series.isEmpty() && now < series.last().x())
                series.clear();
            if (!series.isEmpty() && now == series.last().x())
                series.last().setY(reading);
            else
                series.append(QPointF(now, reading));
            while (!series.isEmpty() && (series.first().x() < now - 60 || series.size() > 240))
                series.removeFirst();
        }
        if (!found)
            series.clear();
    }
    update();
}
void OverviewDashboard::setOffline()
{
    connected = false;
    data = {};
    temperatures.clear();
    pressures.clear();
    update();
}
void OverviewDashboard::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor("#f2f7fd"));
    if (trendOnly) {
        drawTrend(p, rect().adjusted(0, 0, -1, -1));
        return;
    }
    const qreal x = 12, w = width() - 24;
    const bool narrow = width() < 760;
    qreal y = 10;
    const qreal summary = narrow ? 282 : 114;
    drawSummary(p, QRectF(x,y,w,summary));
    y += summary + 12;
    alarmAction = {};
    if(topologyVisible) {
        drawTopology(p,QRectF(x,y,w,narrow ? 556 : qMax(228.0,height()-y-12)),narrow);
    } else if(narrow) {
        drawTrend(p,QRectF(x,y,w,180)); y+=192;
        drawAlarms(p,QRectF(x,y,w,qMax(180.0,height()-y-12)));
    } else {
        const qreal left=(w-12)*.625;
        const qreal h=qMax(220.0,height()-y-12);
        drawTrend(p,QRectF(x,y,left,h));
        drawAlarms(p,QRectF(x+left+12,y,w-left-12,h));
    }
}
void OverviewDashboard::setTopologyVisible(bool visible)
{
    topologyVisible=visible;
    setMinimumHeight(width()<760 ? (visible?880:740) : 430);
    updateGeometry();update();
}

QStringList OverviewDashboard::summaryValues() const
{
    const auto summary = data.value("summary").toObject();
    const auto system = data.value("system").toObject();
    auto countText = [](const QJsonValue &value) {
        const double n = value.toDouble(-1);
        return Visuals::finiteNumber(value) && n >= 0 && n < 9e15 && std::floor(n) == n
                   ? Visuals::numberText(value) : QString("--");
    };
    const auto uptimeValue = system.value("uptime_seconds");
    QString duration = "--";
    // Bound conversion before casting to qint64, including hostile JSON numbers.
    if (Visuals::finiteNumber(uptimeValue) && uptimeValue.toDouble() >= 0 &&
        uptimeValue.toDouble() < 9e15) {
        const qint64 uptime = qint64(uptimeValue.toDouble());
        duration = QString("%1:%2:%3")
                                 .arg(uptime / 3600, 2, 10, QChar('0'))
                                 .arg(uptime / 60 % 60, 2, 10, QChar('0'))
                                 .arg(uptime % 60, 2, 10, QChar('0'));
    }
    return {summary.value("status").toString("--"), countText(summary["online_devices"]),
            countText(data["alarms"].toObject()["active"]), duration};
}
void OverviewDashboard::drawSummary(QPainter &p,const QRectF &r)
{
    using namespace Visuals;
    const bool narrow=r.height()>200;
    const qreal w=narrow?r.width():(r.width()-24)/3;
    const auto values=summaryValues();
    const QStringList titles={"数据连接","设备状态","未确认告警"};
    for(int i=0;i<3;++i) {
        const QRectF card(r.x()+(narrow?0:i*(w+12)),r.y()+(narrow?i*94:0),w,narrow?86:r.height());
        panel(p,card);
        text(p,QRectF(card.x()+14,card.y()+8,w-28,22),titles[i],15,ink,true);
        const bool demo=data.value("settings").toObject().value("demo_mode").toBool();
        const QString value=i==0?(connected?(demo?QString("样例数据"):QString("已连接")):QString("服务离线")):values[i];
        const QColor color=i==0?(connected?green:muted):i==2?QColor("#c22b3d"):blue;
        text(p,QRectF(card.x()+14,card.y()+30,w-28,32),value+(i==1?" 台在线":i==2?" 条":""),24,color,true);
        const QString detail=i==0?("运行状态："+values[0]+" · 时长 "+values[3]):
            i==1?QString("设备数量来自最近服务记录"):QString("确认记录不消除现场故障");
        text(p,QRectF(card.x()+14,card.y()+65,w-28,22),detail,12,muted);
        if(!narrow && i==0)
            text(p,QRectF(card.x()+14,card.y()+87,w-28,18),
                 connected?"以最新服务响应为准":"当前设备状态未确认",11,muted);
    }
}
void OverviewDashboard::drawTopology(QPainter &p, const QRectF &r, bool narrow)
{
    using namespace Visuals;
    panel(p, r);
    text(p, r.adjusted(12, 6, -12, -r.height() + 32), "通信链路", 17, ink, true);
    const qreal left = r.x() + 14, right = r.right() - 14;
    const qreal nodeWidth = narrow ? r.width() - 28 : r.width() * .205;
    const qreal start = r.y() + 38;
    QRectF nodes[3];
    for (int i = 0; i < 3; i++)
        nodes[i] = QRectF(left, start + i * 63, nodeWidth, 53);
    QRectF center(narrow ? left : r.x() + r.width() * .432, narrow ? start + 218 : start + 20,
                  narrow ? nodeWidth : r.width() * .191, 137);
    QRectF outputs[2];
    for (int i = 0; i < 2; i++)
        outputs[i] = QRectF(narrow ? left : right - nodeWidth,
                            narrow ? start + 391 + i * 62 : start + i * 104, nodeWidth, narrow ? 53 : 69);
    auto wire = [&](const QPointF &a, const QPointF &b, const QColor &color) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QPainterPath path(a);
        if (narrow) {
            path.lineTo(QPointF(a.x(), (a.y() + b.y()) / 2));
            path.lineTo(QPointF(b.x(), (a.y() + b.y()) / 2));
        } else {
            const qreal elbow = a.x() + (b.x() - a.x()) * .8;
            path.lineTo(QPointF(elbow - 5, a.y()));
            path.quadTo(QPointF(elbow, a.y()), QPointF(elbow, a.y() + qBound(-5.0, b.y() - a.y(), 5.0)));
            path.lineTo(QPointF(elbow, b.y()));
        }
        path.lineTo(b);
        p.drawPath(path);
        p.setBrush(color);
        p.setPen(Qt::NoPen);
        QPolygonF arrow;
        if (narrow)
            arrow << b << b + QPointF(-4, -8) << b + QPointF(4, -8);
        else
            arrow << b << b + QPointF(-9, -5) << b + QPointF(-9, 5);
        p.drawPolygon(arrow);
    };
    const QStringList names = {"RS485 模块", "CAN 采集模块", "EtherCAT 单元"};
    const QStringList symbols = {"thermometer", "chip", "motor"};
    const QStringList protocols = {"Modbus RTU", "CANopen", "EtherCAT"};
    const auto buses = data.value("buses").toArray();
    for (int i = 0; i < 3; i++) {
        if (!narrow) {
            const QPointF end(center.left(), center.top() + 22 + i * 43);
            wire(QPointF(nodes[i].right(), nodes[i].center().y()), end, i == 1 ? green : blue);
            text(p, QRectF(nodes[i].right() + 4, nodes[i].y() + 1, center.left() - nodes[i].right() - 8, 23),
                 protocols[i], 12, ink, false, Qt::AlignCenter);
        }
        panel(p, nodes[i], QColor("#f6f9fe"));
        icon(p, QRectF(nodes[i].x() + 18, nodes[i].y() + 11, 31, 31), symbols[i]);
        text(p, nodes[i].adjusted(70, 5, -5, -25), names[i], 14, ink, true);
        bool present = false, up = false;
        QString state = "未配置";
        for (const auto &v : buses) {
            const auto bus = v.toObject();
            const auto name = bus.value("name").toString();
            if ((i == 0 && name == "RS485") || (i == 1 && name.startsWith("can", Qt::CaseInsensitive)) ||
                (i == 2 && name == "EtherCAT")) {
                present = true;
                up = up || bus.value("online").toBool();
                state = bus.value("state").toString("未知");
            }
        }
        if (!connected)
            state = "数据不可用";
        else if (up)
            state = "在线";
        p.setPen(Qt::NoPen);
        p.setBrush(up && connected ? green : QColor("#95a9c0"));
        p.drawEllipse(QRectF(nodes[i].x() + 70, nodes[i].y() + 33, 8, 8));
        text(p, nodes[i].adjusted(85, 25, -6, -4), state, 12, present ? ink : muted);
    }
    if (narrow)
        wire(QPointF(nodes[2].center().x(), nodes[2].bottom()), QPointF(center.center().x(), center.top()),
             blue);
    panel(p, center);
    p.setPen(QPen(blue, 2));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(center, 6, 6);
    logo(p, QRectF(center.x() + 24, center.y() + 18, 27, 27));
    text(p, QRectF(center.x() + 60, center.y() + 14, center.width() - 66, 24), "DshanControl", 14, ink, true);
    text(p, QRectF(center.x() + 60, center.y() + 35, center.width() - 66, 18), "工业控制网关", 12, muted);
    p.setPen(border);
    p.drawLine(QPointF(center.x() + 18, center.y() + 59), QPointF(center.right() - 18, center.y() + 59));
    const QStringList meta = {"显示刷新", "数据点", "状态"};
    const auto settings = data.value("settings").toObject();
    const QStringList values = {connected ? QString::number(settings.value("refresh_ms").toInt(1000)) + " ms"
                                          : "--",
                                connected ? QString::number(data.value("sensors").toArray().size()) : "--",
                                connected ? "● 运行中" : "离线"};
    for (int i = 0; i < 3; i++) {
        text(p, QRectF(center.x() + 21, center.y() + 68 + i * 20, 58, 20), meta[i], 11, muted);
        text(p, QRectF(center.x() + 85, center.y() + 68 + i * 20, center.width() - 93, 20), values[i], 11,
             i == 2 ? (connected ? green : muted) : ink);
    }
    const auto networks = data.value("networks").toArray();
    for (int i = 0; i < 2; i++) {
        if (!narrow)
            wire(QPointF(center.right(), center.center().y()),
                 QPointF(outputs[i].left(), outputs[i].center().y() - 4), blue);
        else if (i == 0)
            wire(QPointF(center.center().x(), center.bottom()),
                 QPointF(outputs[i].center().x(), outputs[i].top()), blue);
        panel(p, outputs[i], QColor("#f6f9fe"));
        icon(p, QRectF(outputs[i].x() + 17, outputs[i].y() + 20, 32, 32), i == 0 ? "server" : "cloud");
        const QJsonObject net = i < networks.size() ? networks[i].toObject() : QJsonObject();
        text(p, outputs[i].adjusted(71, 5, -5, -outputs[i].height() + 29), net.value("name").toString("网络接口未提供"),
             14, ink, true);
        text(p, outputs[i].adjusted(71, 27, -5, -outputs[i].height() + 45),
             !connected
                 ? "● 离线"
                 : (net.isEmpty() ? "● 未配置" : (net.value("online").toBool() ? "● 正常" : "● 未连接")),
             12, net.value("online").toBool() ? green : muted);
        if (!narrow)
            text(p, outputs[i].adjusted(71, 45, -5, -3),
                 net.value("address").toString(net.value("name").toString("--")), 10, muted);
    }
}
void OverviewDashboard::drawTrend(QPainter &p, const QRectF &r)
{
    using namespace Visuals;
    panel(p, r);
    text(p, QRectF(r.x() + 12, r.y() + 7, 90, 24), "设备趋势", 16, ink, true);
    const QString tv = temperatures.isEmpty() ? "--" : QString::number(temperatures.last().y(), 'f', 1);
    const QString pv = pressures.isEmpty() ? "--" : QString::number(pressures.last().y(), 'f', 2);
    const qreal legendX = qMax(r.x() + 110, r.right() - 284);
    text(p, QRectF(legendX, r.y() + 7, 138, 24), "● 温度 " + tv + "°C", 13, blue);
    text(p, QRectF(legendX + 140, r.y() + 7, 140, 24), "● 压力 " + pv + " MPa", 13, QColor("#d97706"));
    const QRectF plot = r.adjusted(58, 43, -58, -35);
    if (plot.width() < 40)
        return;
    double minT = 0, maxT = 60, minP = 0, maxP = 1.2;
    for (const auto &point : temperatures) {
        minT = qMin(minT, qFloor(point.y() / 20.0) * 20.0);
        maxT = qMax(maxT, qCeil(point.y() / 20.0) * 20.0);
    }
    for (const auto &point : pressures) {
        minP = qMin(minP, qFloor(point.y() / .4) * .4);
        maxP = qMax(maxP, qCeil(point.y() / .4) * .4);
    }
    for (int i = 0; i <= 3; i++) {
        qreal yy = plot.bottom() - i * plot.height() / 3;
        p.setPen(QPen(border, 1));
        p.drawLine(QPointF(plot.left(), yy), QPointF(plot.right(), yy));
        text(p, QRectF(plot.left() - 28, yy - 9, 23, 18),
             QString::number(minT + i * (maxT - minT) / 3, 'g', 3), 12, muted, false,
             Qt::AlignRight | Qt::AlignVCenter);
        text(p, QRectF(plot.right() + 9, yy - 9, 28, 18),
             QString::number(minP + i * (maxP - minP) / 3, 'f', 1), 12, muted);
    }
    for (int i = 0; i <= 6; i++) {
        qreal xx = plot.left() + i * plot.width() / 6;
        p.setPen(border);
        p.drawLine(QPointF(xx, plot.top()), QPointF(xx, plot.bottom()));
        text(p, QRectF(xx - 16, plot.bottom() + 7, 32, 18), QString::number(-60 + i * 10) + "s", 10, muted,
             false, Qt::AlignCenter);
    }
    p.save();
    p.translate(r.x() + 19, plot.center().y());
    p.rotate(-90);
    text(p, QRectF(-45, -10, 90, 20), "温度 (°C)", 12, muted, false, Qt::AlignCenter);
    p.restore();
    p.save();
    p.translate(r.right() - 17, plot.center().y());
    p.rotate(90);
    text(p, QRectF(-45, -10, 90, 20), "压力 (MPa)", 12, muted, false, Qt::AlignCenter);
    p.restore();
    const double now = data.value("timestamp").toDouble();
    p.save();
    p.setClipRect(plot.adjusted(-1, -1, 1, 1));
    for (int i = 0; i < 2; i++) {
        const auto &series = i == 0 ? temperatures : pressures;
        QPainterPath path;
        for (int n = 0; n < series.size(); n++) {
            QPointF point(plot.right() + (series[n].x() - now) / 60 * plot.width(),
                          plot.bottom() - (series[n].y() - (i == 0 ? minT : minP)) /
                                              (i == 0 ? maxT - minT : maxP - minP) * plot.height());
            if (n == 0)
                path.moveTo(point);
            else
                path.lineTo(point);
        }
        p.setPen(QPen(i == 0 ? blue : QColor("#d97706"), 1.6));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
    }
    p.restore();
    if (temperatures.isEmpty() && pressures.isEmpty())
        text(p, plot, "等待设备采样", 12, muted, false, Qt::AlignCenter);
}
void OverviewDashboard::drawAlarms(QPainter &p, const QRectF &r)
{
    using namespace Visuals;
    panel(p, r);
    text(p, QRectF(r.x() + 12, r.y() + 7, 100, 24), "活动告警", 16, ink, true);
    alarmAction = QRectF(r.right() - 90, r.y(), 82, 44);
    text(p, alarmAction, "查看全部  ›", 13, blue, false, Qt::AlignRight | Qt::AlignVCenter);
    auto rows = data.value("alarms").toObject().value("recent").toArray();
    if (!connected || rows.isEmpty()) {
        text(p, r.adjusted(15, 44, -15, -12), connected ? "暂无活动告警" : "等待告警数据", 13, muted, false,
             Qt::AlignCenter);
        return;
    }
    for (int i = 0; i < qMin(2, rows.size()); i++) {
        const auto row = rows[i].toObject();
        const qreal yy = r.y() + 45 + i * (r.height() - 45) / 2;
        if (i) {
            p.setPen(border);
            p.drawLine(QPointF(r.x() + 12, yy - 6), QPointF(r.right() - 12, yy - 6));
        }
        icon(p, QRectF(r.x() + 15, yy + 5, 18, 18), "warning");
        text(p, QRectF(r.x() + 43, yy, r.width() - 55, 25), row.value("message").toString("未知告警"), 14,
             ink, true);
        const QString time = QDateTime::fromSecsSinceEpoch(row.value("ts").toVariant().toLongLong())
                                 .toString("MM-dd hh:mm:ss");
        text(p, QRectF(r.x() + 43, yy + 26, r.width() - 55, 20),
             row.value("source").toString() + "   " + time, 12, muted);
    }
}
void OverviewDashboard::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && alarmAction.contains(event->pos()))
        emit alarmsRequested();
    QWidget::mouseReleaseEvent(event);
}

void LayoutPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    // Hidden tab pages can retain nested wrapped-label geometry from their
    // previous width. Recalculate for the actual viewport before exposing them.
    if (layout()) {
        layout()->invalidate();
        layout()->activate();
    }
}

void SectionTabs::resizeEvent(QResizeEvent *event)
{
    QTabWidget::resizeEvent(event);
    tabBar()->setMinimumWidth(width() - 2);
    tabBar()->setExpanding(true);
}
ToggleSwitch::ToggleSwitch(const QString &label) : QCheckBox(label)
{
    setMinimumHeight(44);
    setMinimumWidth(78);
}
void ToggleSwitch::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF track(0, (height() - 24) / 2.0, 43, 24);
    p.setPen(Qt::NoPen);
    p.setBrush(isEnabled() && isChecked() ? Visuals::blue : QColor("#bdcce0"));
    p.drawRoundedRect(track, 12, 12);
    p.setBrush(Qt::white);
    p.drawEllipse(QRectF(isChecked() ? 22 : 3, track.y() + 3, 18, 18));
    Visuals::text(p, QRectF(53, 0, width() - 53, height()),
                  text().isEmpty() ? (isChecked() ? "开启" : "关闭") : text(), 13,
                  isEnabled() ? Visuals::ink : Visuals::muted);
    if (hasFocus()) {
        p.setPen(QPen(Visuals::blue, 1, Qt::DotLine));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(rect().adjusted(0, 1, -1, -1), 3, 3);
    }
}

void StatusStrip::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    setFixedHeight(width() < 580 && metrics.size() > 2 ? 96 : 52);
}

void StatusStrip::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    Visuals::panel(p, rect().adjusted(0, 0, -1, -1));
    const int columns = width() < 580 && metrics.size() > 2 ? 2 : qMax(1, metrics.size());
    const qreal cellWidth = qreal(width()) / columns;
    const qreal cellHeight = qreal(height()) / qMax(1, (metrics.size() + columns - 1) / columns);
    for (int i = 0; i < metrics.size(); ++i) {
        const auto &m = metrics[i];
        QRectF cell(i % columns * cellWidth, i / columns * cellHeight, cellWidth, cellHeight);
        if (i % columns) {
            p.setPen(Visuals::border);
            p.drawLine(cell.topLeft() + QPointF(0, 12), cell.bottomLeft() - QPointF(0, 12));
        }
        Visuals::icon(p, QRectF(cell.x() + 12, cell.center().y() - 12, 24, 24), m.icon);
        Visuals::text(p, QRectF(cell.x() + 44, cell.y() + 5, cellWidth - 52, 18), m.label, 11,
                      Visuals::muted);
        Visuals::text(p, QRectF(cell.x() + 44, cell.y() + 22, cellWidth - 52, 23), m.value, 16, m.color,
                      true);
    }
}

void DataTable::paintEvent(QPaintEvent *event)
{
    QTableWidget::paintEvent(event);
    bool visibleRow = false;
    for (int row = 0; row < rowCount(); ++row)
        visibleRow |= !isRowHidden(row);
    if (visibleRow)
        return;
    QPainter p(viewport());
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF area = viewport()->rect();
    if (area.height() > 80)
        Visuals::icon(p, QRectF(area.center().x() - 15, area.center().y() - 37, 30, 30), "document",
                      QColor("#a6bbd6"));
    Visuals::text(p, QRectF(10, area.center().y(), area.width() - 20, 25), emptyText, 12, Visuals::muted,
                  false, Qt::AlignCenter);
}
