#ifndef DSHANCONTROL_UPDATEPANEL_H
#define DSHANCONTROL_UPDATEPANEL_H
#include <QProcess>
#include <QJsonObject>
#include <QTimer>
#include <QWidget>
class QLabel;
class QPushButton;
class QBoxLayout;
class QResizeEvent;
class UpdatePanel : public QWidget
{
  public:
    explicit UpdatePanel(QWidget *parent = nullptr);
    ~UpdatePanel() override;

  protected:
    void resizeEvent(QResizeEvent *event) override;

  private:
    friend class HmiUiTest;
    void reloadIdentity(bool restoreTask = true);
    void checkUpdates();
    void requestNative(const QString &method, const QJsonObject &params);
    void nativeResult(int code, QProcess::ExitStatus exitStatus);
    void resetPreflight(const QString &text);
    void downloadUpdate();
    void installUpdate();
    QString configPath = "/etc/dshancontrol/updater.json";
    QString updaterExecutable = "/usr/bin/dshancontrol-update";
    QList<QLabel *> preflightValues;
    QLabel *version;
    QLabel *board;
    QLabel *contract;
    QLabel *status;
    QLabel *availableVersion;
    QLabel *releaseSummary;
    QBoxLayout *columns;
    QPushButton *check;
    QPushButton *download;
    QPushButton *install;
    bool nativeMode = false;
    bool nativeIdentityReady = false;
    bool restoreTaskAfterIdentity = true;
    QString nativeMethod, taskId, checkTaskId, downloadTaskId;
    QJsonObject selectedRelease;
    QTimer taskPoll;
    QProcess process;
};
#endif
