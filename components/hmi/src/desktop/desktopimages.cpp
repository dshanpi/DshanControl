#include "desktopimages.h"
#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
namespace DesktopV1
{
void ImageDecodeWorker::decode(QString path, quint64 generation)
{
    QImage image;
    QString error;
    QFile file(path);
    const QFileInfo info(path);
    if (!info.isFile() || info.size() > 8 * 1024 * 1024 || !file.open(QIODevice::ReadOnly))
        error = "图片无法读取或超过 8 MiB";
    else
    {
        QByteArray bytes = file.read(8 * 1024 * 1024 + 1);
        file.close();
        if (bytes.size() > 8 * 1024 * 1024)
            error = "图片超过 8 MiB，请缩小后重试";
        else
        {
            QBuffer buffer(&bytes);
            buffer.open(QIODevice::ReadOnly);
            QImageReader reader(&buffer);
            const auto format = reader.format().toLower();
            const auto size = reader.size();
            if (format != "png" && format != "jpeg" && format != "jpg")
                error = "仅支持 PNG / JPEG 图片";
            else if (!size.isValid() || size.width() > 4096 || size.height() > 4096 ||
                     qint64(size.width()) * size.height() > 4194304)
                error = "图片超过 400 万像素，请缩小后重试";
            else
            {
                reader.setAutoTransform(true);
                if (size.width() > 1024 || size.height() > 600)
                    reader.setScaledSize(size.scaled(1024, 600, Qt::KeepAspectRatio));
                image = reader.read();
                if (image.isNull())
                    error = "图片损坏或解码失败";
                else if (image.width() > 1024 || image.height() > 600)
                    image = image.scaled(1024, 600, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
        }
    }
    emit finished(path, generation, image, error);
}
DesktopImageCache::DesktopImageCache(QObject *parent) : QObject(parent)
{
    auto *worker = new ImageDecodeWorker;
    worker->moveToThread(&thread);
    connect(&thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(this, &DesktopImageCache::decodeRequested, worker, &ImageDecodeWorker::decode,
            Qt::QueuedConnection);
    connect(
        worker, &ImageDecodeWorker::finished, this,
        [this](QString path, quint64 generation, QImage image, QString error) {
            busy = false;
            if (active.contains(path) && desired.value(path) == generation)
            {
                images[path] = image;
                errors[path] = error;
                emit changed(path);
            }
            dispatch();
        },
        Qt::QueuedConnection);
    thread.start();
}
DesktopImageCache::~DesktopImageCache()
{
    thread.quit();
    thread.wait();
}
void DesktopImageCache::setPaths(const QStringList &paths)
{
    QSet<QString> next;
    for (const auto &path : paths)
        if (!path.isEmpty() && next.size() < 4)
            next.insert(path);
    const auto removed = active - next;
    for (const auto &path : removed)
    {
        images.remove(path);
        errors.remove(path);
        pending.remove(path);
        desired.remove(path);
    }
    const auto added = next - active;
    active = next;
    for (const auto &path : added)
    {
        desired[path] = ++sequence;
        pending[path] = sequence;
    }
    dispatch();
}
void DesktopImageCache::reload(const QString &path)
{
    if (!active.contains(path))
        return;
    images.remove(path);
    errors.remove(path);
    desired[path] = ++sequence;
    pending[path] = sequence;
    dispatch();
}
QString DesktopImageCache::status(const QString &path) const
{
    if (path.isEmpty())
        return "尚未绑定图片 · 点击选择";
    if (!images.contains(path))
        return "正在读取图片…";
    return errors.value(path);
}
void DesktopImageCache::dispatch()
{
    if (busy || pending.isEmpty())
        return;
    const auto it = pending.begin();
    const auto path = it.key();
    const auto generation = it.value();
    pending.erase(it);
    busy = true;
    emit decodeRequested(path, generation);
}
} // namespace DesktopV1
