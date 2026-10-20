#pragma once
#include <QImage>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QThread>
namespace DesktopV1
{
class ImageDecodeWorker : public QObject
{
    Q_OBJECT
  public slots:
    void decode(QString path, quint64 generation);
  signals:
    void finished(QString path, quint64 generation, QImage image, QString error);
};
// At most four visible/layout images, one decode at a time, and no unbounded
// job queue.
class DesktopImageCache : public QObject
{
    Q_OBJECT
  public:
    explicit DesktopImageCache(QObject *parent = nullptr);
    ~DesktopImageCache() override;
    void setPaths(const QStringList &paths);
    void reload(const QString &path);
    QImage image(const QString &path) const { return images.value(path); }
    QString status(const QString &path) const;
  signals:
    void decodeRequested(QString path, quint64 generation);
    void changed(QString path);

  private:
    void dispatch();
    QThread thread;
    QSet<QString> active;
    QMap<QString, quint64> desired, pending;
    QMap<QString, QImage> images;
    QMap<QString, QString> errors;
    quint64 sequence = 0;
    bool busy = false;
};
} // namespace DesktopV1
