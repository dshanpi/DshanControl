#pragma once
#include "../displaybrightnessreader.h"
#include "desktopcanvas.h"
class MainWindow;
namespace DesktopV1
{
class DesktopShell : public QWidget
{
    Q_OBJECT
  public:
    explicit DesktopShell(QWidget *parent = nullptr);
    bool openApplication(const QString &id, const QJsonObject &context = {});
    void backToDesktop();
    DesktopCanvas *canvas() const { return surface; }

  protected:
    void resizeEvent(QResizeEvent *) override;
    void closeEvent(QCloseEvent *) override;

  private:
    DesktopCanvas *surface;
    MainWindow *applications;
    DisplayBrightnessReader *brightnessReader = nullptr;
    QWidget *applicationFrame;
    QWidget *toolbar;
    QLabel *applicationTitle;
    bool fullscreen = false;
    void positionContent();
};
} // namespace DesktopV1
