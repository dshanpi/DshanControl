#pragma once
#include "mediasession.h"
#include <QWidget>
class QLabel;
class QPushButton;
class QTableWidget;
class QListWidget;
class QSlider;
class QComboBox;
class QCheckBox;
class QStackedWidget;
class QHideEvent;
class QShowEvent;
class QResizeEvent;
class QTimer;
class MediaPanel : public QWidget
{
    Q_OBJECT
  public:
    explicit MediaPanel(QWidget *parent = nullptr);
    bool selectFile(const QString &path);
  signals:
    void fullscreenRequested(bool enabled);

  protected:
    void hideEvent(QHideEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

  private:
    friend class HmiUiTest;
    MediaSession session;
    QWidget *heading, *side, *stage;
    QStackedWidget *pages;
    QLabel *title, *filename, *note, *currentTime, *duration, *decoder, *format, *dimensions, *fps;
    QPushButton *play, *previous, *next, *mute, *fullscreen;
    QSlider *seek, *volume;
    QComboBox *aspect;
    QCheckBox *loop;
    QTableWidget *files;
    QListWidget *playlist;
    QTimer *geometryTimer = nullptr;
    QString mediaDirectory, selectedFile, lastFeedback;
    QStringList mediaFiles;
    QJsonObject state;
    bool full = false, loading = false, selectionNeedsOpen = false;
    void scan();
    void choose();
    void open(const QString &path);
    void apply(const QJsonObject &status);
    void geometry();
    void showSection(int index);
};
