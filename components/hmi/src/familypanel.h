#pragma once
#include "desktopclient.h"
#include <QWidget>
class QLabel;
class QPushButton;
class QComboBox;
class QTableWidget;
class QPlainTextEdit;
class QVBoxLayout;
class QScrollArea;
class QCheckBox;
class QHideEvent;
class FamilyPanel : public QWidget
{
  public:
    explicit FamilyPanel(DesktopClient *client, QWidget *parent = nullptr);
    void apply(const QJsonObject &state);
    void selectSection(int index);
    bool selectDeviceById(const QString &id);

  protected:
    void hideEvent(QHideEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

  private:
    DesktopClient *client;
    QJsonObject current;
    QLabel *note, *voiceState, *activation, *homeState, *detailTitle;
    QPushButton *talk, *stop, *sync;
    QComboBox *mode, *rooms;
    QTableWidget *devices;
    QPlainTextEdit *transcript;
    QScrollArea *controlScroll;
    QWidget *controlBody = nullptr;
    QList<QWidget *> actions;
    QList<QCheckBox *> boolInputs;
    QList<QLabel *> reported;
    QByteArray functionsIdentity, historyIdentity;
    QString selectedDevice;
    bool loading = false;
    bool voiceHoldActive = false, voiceFreeChatActive = false;
    void stopVoiceInteraction(bool explicitStop = false);
    void selectDevice();
    void buildControls();
    void control(const QString &id, const QString &code, const QJsonValue &value);
    bool canControl(const QString &id, const QString &code) const;
};
