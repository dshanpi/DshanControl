#pragma once
#include <QImage>
#include <QWidget>

// A diagnostic pattern, with no sensor fixtures, profile changes or device I/O.
class DisplayProbe : public QWidget
{
  public:
    explicit DisplayProbe(QWidget *parent = nullptr);
    static QImage reference();
  protected:
    void paintEvent(QPaintEvent *) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
};
