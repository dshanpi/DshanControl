#pragma once
#include "applicationregistry.h"
#include "desktopimages.h"
#include "desktopkeyboard.h"
#include "desktoplayout.h"
#include "desktoppoints.h"
#include <QtWidgets>
#include <functional>
#include <memory>
#include <tuple>
namespace DesktopV1
{
QList<AppItem> catalog();
struct DisplayModel
{
    bool fixture = false, online = false;
    bool networkOnline = false, wiredOnline = false, wirelessOnline = false, cloudOnline = false;
    QString clock, date, footer, connection, badge, temperature, humidity, collection;
    QString mediaTitle, mediaTime, cloudState, cloudPeriod, pointCount, lastSend;
    QStringList busStates;
    QString alarmCount, alarmState, alarmText, alarmTime, homeName, homeStatus, homeCount;
    QString voiceTitle, voiceState, controlDate, wired, wireless, sound, notificationCount,
        controlCloud, pairingNotice;
    bool operator==(const DisplayModel &other) const
    {
        return std::tie(fixture, online, networkOnline, wiredOnline, wirelessOnline, cloudOnline,
                        clock, date, footer, connection, badge, temperature, humidity, collection,
                        mediaTitle, mediaTime, cloudState, cloudPeriod, pointCount, lastSend,
                        busStates, alarmCount, alarmState, alarmText, alarmTime, homeName,
                        homeStatus, homeCount, voiceTitle, voiceState, controlDate, wired, wireless,
                        sound, notificationCount, controlCloud, pairingNotice) ==
               std::tie(other.fixture, other.online, other.networkOnline, other.wiredOnline,
                        other.wirelessOnline, other.cloudOnline, other.clock, other.date,
                        other.footer, other.connection, other.badge, other.temperature,
                        other.humidity, other.collection, other.mediaTitle, other.mediaTime,
                        other.cloudState, other.cloudPeriod, other.pointCount, other.lastSend,
                        other.busStates, other.alarmCount, other.alarmState, other.alarmText,
                        other.alarmTime, other.homeName, other.homeStatus, other.homeCount,
                        other.voiceTitle, other.voiceState, other.controlDate, other.wired,
                        other.wireless, other.sound, other.notificationCount, other.controlCloud,
                        other.pairingNotice);
    }
    static DisplayModel example();
    static DisplayModel unavailable();
};
class DesktopCanvas : public QWidget
{
    Q_OBJECT
  public:
    explicit DesktopCanvas(bool fixture = false, QWidget *parent = nullptr);
    int state = 0, desk = 0, selected = 3, brightness = 72, group = 0;
    QSet<int> hidden;
    QLineEdit *search;
    bool rearranged = false, added = false;
    DisplayModel model;
    bool appActive = false;
    void change(int next);
    void openApp(int index);
    void applyHmi(const QJsonObject &snapshot);
    void applyServices(const QJsonObject &snapshot);
    void renderState(int next, int page = 0);
    void observeBrightness(int rawValue, const QString &error);
    void finishBrightness(bool success, int rawValue, const QString &message);
    bool brightnessBusy() const { return brightnessPending; }
    void configureLayoutStorage(const QString &filePath);
    QJsonObject layoutSnapshot() const { return editingLayout ? draftLayout : committedLayout; }
    QJsonObject pointById(const QString &id) const
    {
        return pointHistory.current(id, QDateTime::currentMSecsSinceEpoch(), pointClock.elapsed());
    }
    void paintPointHistory(QPainter &p, const QRectF &r, const QJsonObject &binding)
    {
        drawPointTrend(p, r, binding);
    }
    bool bindVideo(int slot, const QString &path);
    bool bindHome(int slot, const QString &deviceId);
    void openWidget(int slot);
    bool bindTrend(int slot, const QString &pointId, const QString &secondaryId);
    QJsonObject boundPoint(int slot, bool secondary = false) const;
    QVector<TrendSample> boundHistory(int slot, bool secondary = false) const;
    bool bindImage(int slot, const QString &path, const QString &fit = "contain");
    QImage boundImage(int slot) const;
    bool dismissKeyboardFirst();
    bool backLibraryLevel();
    bool dragReturning() const { return dragReturn.state() == QAbstractAnimation::Running; }
    bool overlayTransitionActive() const
    {
        return overlayAnimation.state() == QAbstractAnimation::Running;
    }
    bool requestLayoutExit() { return resolveLayoutExit(); }
    bool removeWidget(int slot);
    bool placeWidget(const QString &type, int slot = -1);
    bool swapWidgets(int from, int to);
    bool commitLayout();
    bool restoreDefaultLayout();
    void discardLayout();
    bool layoutEditing() const { return editingLayout; }
    static bool loadFonts(QString *error);
    void txt(QPainter &p, qreal x, qreal y, qreal w, qreal h, QString s, int px = 14,
             QColor c = QColor("#142C4B"), bool bold = false,
             int align = Qt::AlignLeft | Qt::AlignVCenter);
  signals:
    void appRequested(QString id);
    void appContextRequested(QString id, QJsonObject context);
    void overlayChanged(bool covered);
    void homeRequested();
    void brightnessRequested(int percent);

  protected:
    void paintEvent(QPaintEvent *) override;
    bool event(QEvent *) override;
    bool eventFilter(QObject *, QEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void keyPressEvent(QKeyEvent *) override;

  private:
    QVariantAnimation overlayAnimation, dragReturn;
    QImage overlayBaseCache, overlayPanelCache;
    QImage backgroundCache;
    int ghostSlot = -1;
    QPointF ghostOffset;
    int animatedOverlay = -1;
    qreal overlayProgress = 1;
    bool skipDim = false, animationsEnabled = false;
    bool adaptiveAnimations = false;
    void completeOverlay();
    void animateOverlay(int previous, int next);
    QElapsedTimer pressAge;
    QPointF pressed;
    QList<QPair<QRectF, std::function<void()>>> hits;
    QPixmap media;
    QString query;
    std::unique_ptr<DesktopLayoutStore> layoutStore;
    DesktopImageCache *imageCache = nullptr;
    DesktopKeyboard *keyboard = nullptr;
    DesktopPointHistory pointHistory;
    QElapsedTimer pointClock;
    QJsonArray availablePoints;
    QJsonObject homeSnapshot;
    void editVideoBinding(int slot);
    void editHomeBinding(int slot);
    bool pointsConnected = false;
    void refreshPointBindings();
    void editTrendBinding(int slot);
    void drawPointTrend(QPainter &p, const QRectF &rect, const QJsonObject &binding);
    void refreshImages();
    void editImageBinding(int slot);
    void showImage(int slot);
    void drawBoundImage(QPainter &p, const QRectF &rect, const QJsonObject &binding);
    QJsonObject committedLayout, draftLayout;
    bool editingLayout = false;
    int libraryGroup = -1;
    bool canPlaceWidget(const QString &type) const;
    int librarySlot = -1, libraryReturnState = 0, dragSlot = -1, dragTarget = -1;
    QString layoutNotice;
    QJsonObject widgetAt(int slot) const;
    void updateWidget(int slot, const QJsonObject &widget);
    bool resolveLayoutExit();
    void closeLibrary();
    void chooseLibraryItem(int group);
    void drawWidget(QPainter &p, const QString &type, const QRectF &rect, bool editing,
                    const QJsonObject &binding);
    static QList<QRectF> slotRects();
    int slotAt(const QPointF &point) const;
    bool brightnessDragging = false, brightnessPending = false, brightnessUnconfirmed = false;
    int brightnessBeforeDrag = -1, brightnessExpectedRaw = -1;
    QString brightnessMessage;
    void previewBrightness(qreal logicalX);
    void cancelBrightnessDrag();
    void hit(QRectF r, std::function<void()> f);
    void rr(QPainter &p, QRectF r, QColor c, qreal radius = 18, QColor border = Qt::transparent);
    void card(QPainter &p, QRectF r, bool editing = false);
    void icon(QPainter &p, QString k, qreal x, qreal y, qreal size, QColor c = QColor("#1468FA"));
    void pill(QPainter &p, QRectF r, QString s, bool primary = false);
    void title(QPainter &p, QRectF r, QString name, QString ico, QColor c = QColor("#1468FA"));
    void trend(QPainter &p, QRectF r, bool big = false);
    void thumbnail(QPainter &p, QRectF r);
    void status(QPainter &p);
    void background(QPainter &p);
    void footer(QPainter &p);
    void desktop(QPainter &p, bool editing = false);
    void dim(QPainter &p);
    void control(QPainter &p);
    void apps(QPainter &p);
    void library(QPainter &p);
    void detail(QPainter &p);
};
int renderFixtures(QApplication &app, const QString &directory);
int runDesktopChecks(QApplication &app);
} // namespace DesktopV1
