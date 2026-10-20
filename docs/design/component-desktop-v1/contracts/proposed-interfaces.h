// IMPLEMENTATION CONTRACT ONLY. These classes do not yet exist in the product.
// Qt 5.11 / C++11. Wire protocols remain the existing contracts in document 05.
#pragma once
#include <QObject>
#include <QWidget>
#include <QJsonObject>
#include <QJsonArray>
#include <QVector>
#include <QDateTime>
#include <QRect>
namespace DesktopV1 {
struct PointSample { QDateTime utc; double value; QString quality, unit; };
enum class Overlay { None, ControlCenter, AppDrawer, WidgetLibrary, Confirmation };
class HmiClient : public QObject {
    Q_OBJECT
public:
    explicit HmiClient(QObject *parent=nullptr);
    void requestSnapshot(); // at most one pending, 4s timeout
    quint64 request(const QByteArray &verb,const QString &path,const QJsonObject &body={});
signals:
    void snapshotChanged(QJsonObject snapshot);
    void requestFinished(quint64 localToken,QJsonObject body);
    void requestFailed(quint64 localToken,QString code,QString message);
};
class AppRouter : public QObject {
    Q_OBJECT
public:
    explicit AppRouter(QObject *parent=nullptr);
    bool open(const QString &stableAppId,const QJsonObject &context={});
    void backToDesktop();
signals:
    void pageChanged(QString appId,QJsonObject context);
    void unavailable(QString appId,QString reason);
};
class DesktopStateStore : public QObject {
    Q_OBJECT
public:
    explicit DesktopStateStore(QString absolutePath,QObject *parent=nullptr);
    QJsonObject load();
    bool validate(const QJsonObject &layout,QString *error) const;
    bool commit(const QJsonObject &draft,QString *error); // QSaveFile, no partial writes
signals:
    void layoutCommitted(QJsonObject layout);
};
class PointHistoryStore : public QObject {
    Q_OBJECT
public:
    explicit PointHistoryStore(QObject *parent=nullptr);
    void ingest(QString pointId,PointSample sample); // good real samples only
    QVector<PointSample> series(QString pointId,int seconds=1800) const;
signals:
    void seriesChanged(QString pointId);
};
class OverlayManager : public QObject {
    Q_OBJECT
public:
    explicit OverlayManager(QObject *parent=nullptr);
    void show(Overlay kind); // exclusive; suppress video layer first
    void dismiss();
signals:
    void overlayChanged(Overlay kind);
    void mediaVisibilityRequired(bool visible);
};
class BrightnessController : public QObject {
    Q_OBJECT
public:
    explicit BrightnessController(HmiClient *client,QObject *parent=nullptr);
    void previewPercent(int percent); // local only
    void commitPercent(int percent); // PUT/settings then POST/actions; verify readback
signals:
    void pendingChanged(bool pending);
    void actualChanged(int rawValue);
    void failed(QString stage,QString message);
};
class WidgetFactory {
public:
    QWidget *create(const QJsonObject &instance,QWidget *parent);
    bool supports(const QString &type,const QString &slotId) const;
};
// ViewModels passed by value are immutable snapshots. Widgets never issue device I/O.
// Existing DesktopClient/MediaSession public functions stay as documented in doc05.
// Extract MainWindow private builders internally; do not call private methods here.
}
