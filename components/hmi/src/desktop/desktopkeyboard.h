#pragma once
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QWidget>
namespace DesktopV1
{
class DesktopKeyboard : public QWidget
{
    Q_OBJECT
  public:
    explicit DesktopKeyboard(QWidget *parent = nullptr);
    void showFor(QLineEdit *field);
    void dismiss();
    void escape();
    void append(const QString &text);
    void choose(int index);
    QString composition() const { return spelling; }
    QStringList candidates() const { return choices; }

  private:
    QPointer<QLineEdit> target;
    QString spelling;
    QStringList choices;
    QLabel *preedit;
    QList<QPushButton *> candidateButtons, letterButtons;
    QPushButton *mode;
    bool chinese = true, numbers = false;
    int candidatePage = 0;
    QPushButton *button(const QString &text, const QString &name);
    void refresh();
};
} // namespace DesktopV1
