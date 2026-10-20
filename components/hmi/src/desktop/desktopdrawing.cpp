#include "desktopcanvas.h"
#include <cmath>
namespace DesktopV1
{
static const QColor Ink("#142C4B"), Muted("#71859D"), Blue("#1468FA"), Line("#E3EBF3"),
    Green("#159D85"), Amber("#C88525");
void DesktopCanvas::txt(QPainter &p, qreal x, qreal y, qreal w, qreal h, QString s, int px,
                        QColor c, bool bold, int align)
{
    QFont f("Dshan Sans");
    f.setPixelSize(px);
    f.setWeight(bold ? QFont::DemiBold : QFont::Normal);
    p.setFont(f);
    p.setPen(c);
    p.drawText(QRectF(x, y, w, h), align, s);
}
void DesktopCanvas::rr(QPainter &p, QRectF r, QColor c, qreal radius, QColor border)
{
    p.setPen(border.alpha() ? QPen(border, 1) : Qt::NoPen);
    p.setBrush(c);
    p.drawRoundedRect(r, radius, radius);
}
void DesktopCanvas::card(QPainter &p, QRectF r, bool editing)
{
    rr(p, r.translated(0, 4), QColor(64, 95, 134, 12), 20);
    rr(p, r, QColor(255, 255, 255, 255), 20);
    if (editing)
    {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor("#7AA9F9"), 1.3, Qt::DashLine));
        p.drawRoundedRect(r.adjusted(1, 1, -1, -1), 19, 19);
    }
}
void DesktopCanvas::icon(QPainter &p, QString k, qreal x, qreal y, qreal size, QColor c)
{
    p.save();
    p.translate(x, y);
    p.scale(size / 24, size / 24);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(c, 1.65, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (k == "chart")
    {
        p.drawLine(3, 3, 3, 21);
        p.drawLine(3, 21, 22, 21);
        QPolygonF a;
        a << QPointF(6, 16) << QPointF(10, 10) << QPointF(14, 13) << QPointF(21, 5);
        p.drawPolyline(a);
    }
    else if (k == "wifi")
    {
        p.drawArc(QRectF(2, 5, 20, 16), 30 * 16, 120 * 16);
        p.drawArc(QRectF(6, 10, 12, 10), 30 * 16, 120 * 16);
        p.setBrush(c);
        p.drawEllipse(QPointF(12, 20), 1, 1);
    }
    else if (k == "bus")
    {
        p.drawRoundedRect(QRectF(8, 2, 8, 6), 1, 1);
        p.drawLine(12, 8, 12, 12);
        p.drawLine(4, 12, 20, 12);
        for (int x : {4, 12, 20})
        {
            p.drawLine(x, 12, x, 16);
            p.drawRect(QRectF(x - 3, 16, 6, 5));
        }
    }
    else if (k == "device")
    {
        p.drawRoundedRect(QRectF(2, 3, 20, 14), 2, 2);
        p.drawLine(12, 17, 12, 21);
        p.drawLine(7, 21, 17, 21);
        p.drawLine(6, 12, 10, 8);
        p.drawLine(10, 8, 14, 11);
        p.drawLine(14, 11, 18, 7);
    }
    else if (k == "bell")
    {
        QPainterPath a;
        a.moveTo(5, 17);
        a.lineTo(7, 14);
        a.lineTo(7, 9);
        a.cubicTo(7, 2, 17, 2, 17, 9);
        a.lineTo(17, 14);
        a.lineTo(19, 17);
        a.closeSubpath();
        p.drawPath(a);
        p.drawArc(QRectF(9, 17, 6, 5), 180 * 16, 180 * 16);
    }
    else if (k == "cloud" || k == "upload")
    {
        QPainterPath a;
        a.moveTo(6, 18);
        a.cubicTo(-2, 18, 1, 8, 7, 10);
        a.cubicTo(6, 0, 19, 0, 19, 10);
        a.cubicTo(25, 10, 25, 18, 18, 18);
        a.closeSubpath();
        p.drawPath(a);
        if (k == "upload")
        {
            p.drawLine(12, 19, 12, 10);
            p.drawLine(9, 13, 12, 10);
            p.drawLine(12, 10, 15, 13);
        }
    }
    else if (k == "video")
    {
        p.drawRoundedRect(QRectF(2, 4, 20, 16), 3, 3);
        p.setBrush(c);
        QPolygonF a;
        a << QPointF(10, 8) << QPointF(10, 16) << QPointF(16, 12);
        p.drawPolygon(a);
    }
    else if (k == "image")
    {
        p.drawRoundedRect(QRectF(2, 3, 20, 18), 3, 3);
        p.drawEllipse(QPointF(8, 8), 2, 2);
        p.drawLine(4, 18, 11, 11);
        p.drawLine(11, 11, 16, 16);
        p.drawLine(16, 16, 20, 12);
    }
    else if (k == "home")
    {
        QPolygonF a;
        a << QPointF(2, 11) << QPointF(12, 2) << QPointF(22, 11);
        p.drawPolyline(a);
        p.drawLine(5, 9, 5, 22);
        p.drawLine(5, 22, 19, 22);
        p.drawLine(19, 22, 19, 9);
        p.drawRect(QRectF(10, 14, 5, 8));
    }
    else if (k == "mic")
    {
        p.drawRoundedRect(QRectF(8, 2, 8, 13), 4, 4);
        p.drawArc(QRectF(4, 6, 16, 14), 180 * 16, 180 * 16);
        p.drawLine(12, 20, 12, 23);
    }
    else if (k == "settings")
    {
        p.drawEllipse(QRectF(5, 5, 14, 14));
        p.drawEllipse(QRectF(9, 9, 6, 6));
        for (int i = 0; i < 8; i++)
        {
            p.save();
            p.translate(12, 12);
            p.rotate(i * 45);
            p.drawLine(0, 7, 0, 10);
            p.restore();
        }
    }
    else if (k == "update")
    {
        p.drawArc(QRectF(4, 4, 16, 16), -30 * 16, 290 * 16);
        p.drawLine(20, 3, 20, 9);
        p.drawLine(20, 9, 15, 8);
    }
    else if (k == "sun")
    {
        p.drawEllipse(QRectF(8, 8, 8, 8));
        for (int i = 0; i < 8; i++)
        {
            p.save();
            p.translate(12, 12);
            p.rotate(i * 45);
            p.drawLine(0, 8, 0, 10);
            p.restore();
        }
    }
    else if (k == "grid")
    {
        for (int i = 0; i < 4; i++)
            p.drawRoundedRect(QRectF(3 + (i % 2) * 11, 3 + (i / 2) * 11, 7, 7), 1, 1);
    }
    else if (k == "plus")
    {
        p.drawLine(12, 4, 12, 20);
        p.drawLine(4, 12, 20, 12);
    }
    else if (k == "up" || k == "down")
    {
        QPolygonF a;
        a << QPointF(6, k == "up" ? 15 : 9) << QPointF(12, k == "up" ? 9 : 15)
          << QPointF(18, k == "up" ? 15 : 9);
        p.drawPolyline(a);
    }
    else if (k == "back")
    {
        p.drawLine(18, 12, 5, 12);
        p.drawLine(5, 12, 11, 6);
        p.drawLine(5, 12, 11, 18);
    }
    p.restore();
}
void DesktopCanvas::pill(QPainter &p, QRectF r, QString s, bool primary)
{
    rr(p, r, primary ? Blue : QColor("#EDF3FD"), r.height() / 2);
    txt(p, r.x(), r.y(), r.width(), r.height(), s, 14, primary ? Qt::white : Blue, true,
        Qt::AlignCenter);
}
void DesktopCanvas::title(QPainter &p, QRectF r, QString name, QString ico, QColor c)
{
    icon(p, ico, r.x() + 18, r.y() + 17, 20, c);
    txt(p, r.x() + 46, r.y() + 12, r.width() - 65, 30, name, 15, Ink, true);
}
void DesktopCanvas::trend(QPainter &p, QRectF r, bool big)
{
    for (int i = 0; i < 3; i++)
    {
        p.setPen(QPen(QColor("#E7EDF4"), 1, Qt::DashLine));
        p.drawLine(QPointF(r.x(), r.y() + i * r.height() / 2),
                   QPointF(r.right(), r.y() + i * r.height() / 2));
    }
    if (!model.fixture)
    {
        txt(p, r.x(), r.y(), r.width(), r.height(), "绑定点位后显示趋势", 12, Muted, false,
            Qt::AlignCenter);
        return;
    }
    QPainterPath line;
    for (int i = 0; i <= 80; i++)
    {
        qreal x = r.x() + r.width() * i / 80;
        qreal y = r.y() + r.height() * (.5 + .14 * sin(i * .17) + .08 * sin(i * .6) - i * .0015);
        if (i == 0)
            line.moveTo(x, y);
        else
            line.lineTo(x, y);
    }
    QPainterPath area = line;
    area.lineTo(r.right(), r.bottom());
    area.lineTo(r.left(), r.bottom());
    area.closeSubpath();
    QLinearGradient g(0, r.top(), 0, r.bottom());
    g.setColorAt(0, QColor(70, 137, 255, 46));
    g.setColorAt(1, QColor(70, 137, 255, 1));
    p.fillPath(area, g);
    p.setPen(QPen(Blue, big ? 3 : 2.5));
    p.drawPath(line);
    p.setPen(QPen(Qt::white, 3));
    p.setBrush(Blue);
    p.drawEllipse(line.currentPosition(), 4, 4);
    txt(p, r.x(), r.bottom() + 6, 80, 20, "14:00", 11, Muted);
    txt(p, r.center().x() - 30, r.bottom() + 6, 60, 20, "14:15", 11, Muted, false, Qt::AlignCenter);
    txt(p, r.right() - 60, r.bottom() + 6, 60, 20, "14:28", 11, Muted, false,
        Qt::AlignRight | Qt::AlignVCenter);
}
void DesktopCanvas::thumbnail(QPainter &p, QRectF r)
{
    p.save();
    QPainterPath clip;
    clip.addRoundedRect(r, 12, 12);
    p.setClipPath(clip);
    p.fillRect(r, QColor("#DCE7EC"));
    if (model.fixture && !media.isNull())
        p.drawPixmap(r, media, QRectF(225, 215, 695, 300));
    p.restore();
}
void DesktopCanvas::background(QPainter &p)
{
    const auto gradients = [](QPainter &target) {
        QLinearGradient g(0, 0, 1024, 600);
        g.setColorAt(0, QColor("#EAF2FD"));
        g.setColorAt(.55, QColor("#EEF3F9"));
        g.setColorAt(1, QColor("#DFE9F8"));
        target.fillRect(QRectF(0, 0, 1024, 600), g);
        QRadialGradient a(1020, 620, 680);
        a.setColorAt(0, QColor(111, 163, 249, 30));
        a.setColorAt(1, QColor(111, 163, 249, 0));
        target.fillRect(QRectF(0, 53, 1024, 547), a);
    };
    const qreal dpr = devicePixelRatioF();
    const QSize pixels(qRound(width() * dpr), qRound(height() * dpr));
    if (pixels.width() <= 0 || pixels.height() <= 0 ||
        qint64(pixels.width()) * pixels.height() > 4 * 1024 * 1024)
    {
        gradients(p);
        return;
    }
    if (backgroundCache.size() != pixels || backgroundCache.devicePixelRatioF() != dpr)
    {
        backgroundCache = QImage(pixels, QImage::Format_ARGB32_Premultiplied);
        if (!backgroundCache.isNull())
        {
            backgroundCache.setDevicePixelRatio(dpr);
            QPainter cached(&backgroundCache);
            cached.setRenderHints(p.renderHints());
            cached.scale(width() / 1024.0, height() / 600.0);
            gradients(cached);
        }
    }
    if (backgroundCache.isNull())
        gradients(p);
    else
        p.drawImage(QRectF(0, 0, 1024, 600), backgroundCache);
}
} // namespace DesktopV1
