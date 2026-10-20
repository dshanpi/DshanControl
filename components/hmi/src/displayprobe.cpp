#include "displayprobe.h"
#include <QKeyEvent>
#include <QPainter>
#include <QPushButton>

namespace {
void pattern(QPainter &p)
{
    p.fillRect(QRect(0, 0, 1024, 600), Qt::white);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(QPen(Qt::black, 1));
    p.drawRect(0, 0, 1023, 599);
    QFont font("Dshan Sans");
    font.setPixelSize(20);
    p.setFont(font);
    p.drawText(QRect(24, 14, 760, 36), "屏幕校验 · 1024 × 600 原生像素 · 不改变显示设置");
    const QList<QColor> primary{Qt::red, Qt::green, Qt::blue, Qt::cyan, Qt::magenta,
                               Qt::yellow, Qt::black, Qt::white};
    for (int i = 0; i < primary.size(); ++i) {
        const QRect box(24 + i * 122, 76, 122, 92);
        p.fillRect(box, primary[i]);
        p.setPen(i == 2 || i == 6 ? Qt::white : Qt::black);
        p.drawText(box, Qt::AlignCenter, primary[i].name().toUpper());
    }
    const QList<QColor> ui{QColor("#142C4B"), QColor("#1468FA"), QColor("#71859D"),
                          QColor("#EBF3FF"), QColor("#EDF3FA"), QColor("#FFFFFF")};
    for (int i = 0; i < ui.size(); ++i) {
        const QRect box(24 + i * 162, 190, 162, 64);
        p.fillRect(box, ui[i]);
        p.setPen(i < 3 ? Qt::white : QColor("#142C4B"));
        p.drawText(box, Qt::AlignCenter, ui[i].name().toUpper());
    }
    for (int i = 0; i < 32; ++i) {
        const int level = qRound(i * 255.0 / 31);
        p.fillRect(QRect(24 + i * 30, 277, 30, 45), QColor(level, level, level));
    }
    p.setPen(Qt::black);
    font.setPixelSize(16);
    p.setFont(font);
    p.drawText(QRect(24, 332, 970, 28), "灰阶应从黑到白逐级增加；红绿蓝应对应上方标注，浅灰与纯白应可区分。");
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor("#1468FA"), 1));
    p.drawRect(QRectF(50.5, 393.5, 99, 99));
    p.drawEllipse(QRectF(198.5, 393.5, 99, 99));
    p.setRenderHint(QPainter::Antialiasing, false);
    for (int x = 360; x <= 960; x += 50)
        p.drawLine(x, 380, x, 500);
    for (int y = 380; y <= 500; y += 20)
        p.drawLine(360, y, 960, y);
    p.setPen(Qt::black);
    p.drawText(QRect(24, 507, 320, 25), Qt::AlignCenter, "100 × 100 像素正方形 / 正圆");
    p.drawText(QRect(360, 507, 620, 25), Qt::AlignCenter, "横格 50px，竖格 20px；外边框应完整可见");
    p.drawText(QRect(24, 550, 950, 28), "对照同名校验 PNG：若文件正常而屏幕偏色或拉伸，再检查 LCD 输出配置。");
}
}

DisplayProbe::DisplayProbe(QWidget *parent) : QWidget(parent, Qt::Window)
{
    setObjectName("displayProbe");
    setWindowTitle("屏幕色彩与比例校验");
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowModality(Qt::ApplicationModal);
    resize(1024, 600);
    auto *close = new QPushButton("返回", this);
    close->setObjectName("closeDisplayProbe");
    close->setGeometry(880, 12, 120, 48);
    connect(close, &QPushButton::clicked, this, &QWidget::close);
}
QImage DisplayProbe::reference()
{
    QImage image(1024, 600, QImage::Format_RGB32);
    QPainter painter(&image);
    pattern(painter);
    return image;
}
void DisplayProbe::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), Qt::white);
    // Deliberately retain pixel geometry; an unexpected resolution should show
    // clipping or extra space instead of hiding it with a compensating stretch.
    pattern(painter);
}
void DisplayProbe::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) close();
    else QWidget::keyPressEvent(event);
}
void DisplayProbe::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (auto *button = findChild<QPushButton *>("closeDisplayProbe"))
        button->move(qMax(0, width() - 144), 12);
}
