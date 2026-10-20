#pragma once
#include "desktopclient.h"
#include <QElapsedTimer>
#include <QWidget>
class QLabel;
class QPushButton;
class QTableWidget;
class QComboBox;
class QCheckBox;
class CloudPanel : public QWidget
{
  public:
    explicit CloudPanel(DesktopClient *client, QWidget *parent = nullptr);
    void apply(const QJsonObject &state);
    void selectSection(int index);

  private:
    DesktopClient *client;
    QLabel *identity, *connection, *ownership, *code, *expiry, *note, *reportState, *lastAck;
    QPushButton *refresh, *send;
    QTableWidget *points;
    QCheckBox *enabled;
    QComboBox *period;
    QElapsedTimer since;
    int remaining = 0;
    bool available = false, loading = false;
    QTimer countdown;
    void showRemaining();
};
