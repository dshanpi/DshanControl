#ifndef DSHAN_VISUALS_H
#define DSHAN_VISUALS_H
#include <QCheckBox>
#include <QGroupBox>
#include <QJsonObject>
#include <QPainter>
#include <QPointF>
#include <QPushButton>
#include <QSlider>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QVector>
namespace Visuals
{
extern const QColor ink, muted, blue, cyan, green, border;
void initializeFonts();
bool finiteNumber(const QJsonValue &value);
QString numberText(const QJsonValue &value, int decimals = 0);
bool sensorAvailable(const QJsonObject &sensor);
bool physicalReading(const QJsonObject &sensor, const QString &name, double *reading);
QFont font(int pixels, bool bold = false);
void text(QPainter &p, const QRectF &r, const QString &s, int pixels = 14,
          const QColor &color = QColor("#092650"), bool bold = false,
          int align = Qt::AlignLeft | Qt::AlignVCenter);
void icon(QPainter &p, const QRectF &r, const QString &name, const QColor &color = QColor("#092650"));
QIcon icon(const QString &name, const QColor &color = QColor("#092650"));
void panel(QPainter &p, const QRectF &r, const QColor &fill = Qt::white);
void logo(QPainter &p, const QRectF &r);
} // namespace Visuals
// A native group retains its accessibility/title semantics while separating
// the heading from content, as in the reviewed page cards.
class SectionCard : public QGroupBox
{
  public:
    explicit SectionCard(const QString &title, QWidget *parent = nullptr)
        : QGroupBox(title, parent) {}
  protected:
    void paintEvent(QPaintEvent *event) override;
};
class NavigationButton : public QPushButton
{
  public:
    NavigationButton(const QString &text, const QString &symbol);

  protected:
    void paintEvent(QPaintEvent *) override;

  private:
    QString symbol;
};
// Scroll areas should shrink pages to their layout minimum, rather than adopting
// a desktop-sized preferred height from a wrapped caption in a nested form.
class LayoutPage : public QWidget
{
  public:
    bool hasHeightForWidth() const override
    {
        return false;
    }
  protected:
    void resizeEvent(QResizeEvent *event) override;
};
// QScrollArea otherwise uses a nested wrapped label's preferred height for the
// whole page. Let the layout minimum drive scrolling and distribute free space.
class PageLayout : public QVBoxLayout
{
  public:
    explicit PageLayout(QWidget *parent) : QVBoxLayout(parent) {}
    bool hasHeightForWidth() const override
    {
        return false;
    }
    int heightForWidth(int) const override
    {
        return -1;
    }
};
class PageRowLayout : public QHBoxLayout
{
  public:
    explicit PageRowLayout(QWidget *parent) : QHBoxLayout(parent) {}
    bool hasHeightForWidth() const override
    {
        return false;
    }
    int heightForWidth(int) const override
    {
        return -1;
    }
};
class SectionTabs : public QTabWidget
{
  protected:
    void resizeEvent(QResizeEvent *event) override;
};
class ToggleSwitch : public QCheckBox
{
  public:
    explicit ToggleSwitch(const QString &label = QString());
    QSize sizeHint() const override
    {
        return QSize(108, 44);
    }

  protected:
    void paintEvent(QPaintEvent *) override;
    bool hitButton(const QPoint &point) const override
    {
        return rect().contains(point);
    }
};
// Horizontal slider for finger input: tap to position, or drag from a generous
// handle target without jumping when the finger lands off the handle center.
class TouchSlider : public QSlider
{
  public:
    explicit TouchSlider(QWidget *parent = nullptr);

  protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

  private:
    void moveTo(int x);
    int dragOffset = 0;
};
struct StatusMetric {
    StatusMetric(const QString &symbol, const QString &name, const QString &text,
                 const QColor &tint = Visuals::blue)
        : icon(symbol), label(name), value(text), color(tint)
    {
    }
    QString icon, label, value;
    QColor color = Visuals::blue;
};
class StatusStrip : public QWidget
{
  public:
    explicit StatusStrip(QWidget *parent = nullptr) : QWidget(parent)
    {
        setMinimumHeight(52);
    }
    void setMetrics(const QList<StatusMetric> &items)
    {
        metrics = items;
        updateGeometry();
        update();
    }
    QList<StatusMetric> values() const
    {
        return metrics;
    }

  protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;

  private:
    QList<StatusMetric> metrics;
};
class DataTable : public QTableWidget
{
  public:
    explicit DataTable(int columns) : QTableWidget(0, columns) {}
    void setEmptyText(const QString &text)
    {
        emptyText = text;
        viewport()->update();
    }

  protected:
    void paintEvent(QPaintEvent *event) override;

  private:
    QString emptyText = "暂无记录";
};
class OverviewDashboard : public QWidget
{
    Q_OBJECT
  public:
    explicit OverviewDashboard(bool trendOnly = false);
    void setSnapshot(const QJsonObject &value);
    void setOffline();
    void setTopologyVisible(bool visible);
    bool online() const
    {
        return connected;
    }
    QJsonObject snapshot() const
    {
        return data;
    }
    QSize sizeHint() const override
    {
        return QSize(880, 430);
    }
  signals:
    void alarmsRequested();

  protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;

  private:
    friend class HmiUiTest;
    QStringList summaryValues() const;
    void drawSummary(QPainter &, const QRectF &);
    void drawTopology(QPainter &, const QRectF &, bool narrow);
    void drawTrend(QPainter &, const QRectF &);
    void drawAlarms(QPainter &, const QRectF &);
    QJsonObject data;
    bool connected = false;
    bool trendOnly = false;
    bool topologyVisible = false;
    QVector<QPointF> temperatures, pressures;
    QRectF alarmAction;
};
#endif
