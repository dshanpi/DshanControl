#ifndef OMNIGATE_MAINWINDOW_H
#define OMNIGATE_MAINWINDOW_H

#include <QJsonObject>
#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QTimer>

QT_BEGIN_NAMESPACE
class QCheckBox;
class QFrame;
class QLineEdit;
class QBoxLayout;
class QResizeEvent;
class QComboBox;
class QLabel;
class QNetworkReply;
class QPushButton;
class QSlider;
class QStackedWidget;
class QTableWidget;
class QTabWidget;
class QTabBar;
QT_END_NAMESPACE

namespace DesktopV1 { class DesktopCanvas; class DesktopTrendPanel; }
class OverviewDashboard;
class StatusStrip;
class DesktopClient;
class CloudPanel;
class FamilyPanel;
class MediaPanel;

class MainWindow : public QMainWindow
{
    Q_OBJECT
  public:
    explicit MainWindow(QWidget *parent = nullptr);
    bool openApplication(const QString &id, const QJsonObject &context = {});
    void setDesktopEmbedded(bool embedded);
    void setDesktopPointSource(DesktopV1::DesktopCanvas *source);
    bool requestDesktopBrightness(int percent);

  signals:
    void applicationNavigationRequested(QString id);
    void hmiSnapshotChanged(const QJsonObject &snapshot);
    void desktopSnapshotChanged(const QJsonObject &snapshot);
    void mediaFullscreenChanged(bool enabled);
    void desktopBrightnessFinished(bool success, int rawValue, QString message);

  protected:
    void resizeEvent(QResizeEvent *event) override;

  private slots:
    void requestSnapshot();
    void handleReply(QNetworkReply *reply);
    void showPage(int index);

  private:
    friend class HmiUiTest;
    void navigateApplication(const QString &id);
    bool desktopEmbedded = false;
    QWidget *applicationUpdatePage = nullptr;
    DesktopClient *desktopClient = nullptr;
    CloudPanel *cloudPanel = nullptr;
    FamilyPanel *familyPanel = nullptr;
    MediaPanel *mediaPanel = nullptr;
    DesktopV1::DesktopTrendPanel *boundTrendPanel = nullptr;
    QStackedWidget *devicePageStack = nullptr;
    QCheckBox *canFdCheck = nullptr;
    QWidget *scrollPage(QWidget *page);
    void markOffline();
    QWidget *buildOverview();
    QWidget *buildFieldbus();
    QWidget *buildNetwork();
    QWidget *buildDevices();
    QWidget *buildAlarms();
    QWidget *buildSettings();
    QTableWidget *makeTable(const QStringList &headers);
    QPushButton *actionButton(const QString &text, const QString &kind = "primary");
    void applyTheme();
    void applyAlarmFilter();
    void updateAlarmSelection();
    QByteArray visibleAlarmCsv() const;
    void updateNetworkDetails();
    void updateNetworkTables(const QJsonObject &root);
    void updatePageMetrics(const QJsonObject &root);
    void updateAuditFilter();
    void updateSnapshot(const QJsonObject &root);
    void sendJson(const QByteArray &method, const QString &path, const QJsonObject &body,
                  const QString &kind);
    void postAction(const QString &action, const QJsonObject &params, const QString &description);
    QString sourceText(const QJsonObject &item, const QString &text) const;
    static QString uptimeText(qint64 seconds);

    OverviewDashboard *dashboard = nullptr;
    OverviewDashboard *deviceTrend = nullptr;
    QLabel *refreshValue = nullptr;
    QLabel *screenResolution = nullptr;
    QFrame *nav = nullptr;
    QLabel *navModel = nullptr;
    QList<QBoxLayout *> responsiveRows;
    QList<QComboBox *> canInterfaces;
    QList<QPair<QWidget *, QString>> capabilityControls;
    bool portInitialized = false;
    QLineEdit *serialPort = nullptr;
    bool alarmRequestActive = false;
    bool alarmsAvailable = false;
    bool alarmAckPending = false;
    bool settingsDirty = false;
    bool loadingSettings = false;
    bool settingsPending = false;
    bool desktopBrightnessRequest = false;
    quint64 settingsRevision = 0;
    quint64 settingsConnectionRevision = 0;
    quint64 savedSettingsRevision = 0;
    QJsonObject submittedSettings;
    void updateSettingsControls();
    void discardSettingsDraft();
    QString settingsDraftNotice;
    QString settingsFeedback = QStringLiteral("尚未执行保存；服务记录不代表背光已实际应用。");
    QList<QPushButton *> serviceButtons;
    QPushButton *rebootButton = nullptr;
    QNetworkAccessManager networkManager;
    QTimer refreshTimer;
    QTimer pageTimer;
    QStackedWidget *pages = nullptr;
    QList<QPushButton *> navButtons;
    QLabel *clockLabel = nullptr;
    QLabel *connectionLabel = nullptr;
    QLabel *alarmSummaryLabel = nullptr;
    QLabel *demoBadge = nullptr;
    QLabel *cpuValue = nullptr;
    QLabel *memoryValue = nullptr;
    QLabel *temperatureValue = nullptr;
    QLabel *storageValue = nullptr;
    QTableWidget *busTable = nullptr;
    QTableWidget *networkTable = nullptr;
    QList<QLabel *> networkFields;
    QLabel *networkDetail = nullptr;
    QList<QTableWidget *> networkTables;
    QList<QList<QLabel *>> networkFieldGroups;
    QList<QLabel *> networkDetails;
    QList<QLabel *> cellularStates, cellularInterfaces, cellularAddresses;
    QList<QLabel *> serviceStates;
    StatusStrip *rsStatus = nullptr;
    StatusStrip *ecStatus = nullptr;
    StatusStrip *monitorStatus = nullptr;
    QJsonObject latestSnapshot;
    bool monitorPaused = false;
    QPushButton *pauseMonitor = nullptr;
    QTableWidget *deviceTable = nullptr;
    QTableWidget *alarmTable = nullptr;
    QTabBar *alarmTabs = nullptr;
    QComboBox *alarmSeverity = nullptr;
    QComboBox *alarmSource = nullptr;
    QComboBox *alarmTime = nullptr;
    QLabel *alarmDetail = nullptr;
    QPushButton *alarmAck = nullptr;
    QTableWidget *auditTable = nullptr;
    QComboBox *auditResult = nullptr;
    QLabel *auditCount = nullptr;
    QSlider *brightnessSlider = nullptr;
    QCheckBox *demoCheck = nullptr;
    QComboBox *refreshCombo = nullptr;
    quint64 snapshotGeneration = 0;
    bool requestActive = false;
};

#endif
