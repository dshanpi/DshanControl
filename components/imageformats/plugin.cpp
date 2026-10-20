#include "codec-config.h"
#include "private/qpnghandler_p.h"
#include "qjpeghandler_p.h"
#include <QIODevice>
#include <QImageIOPlugin>
class DshanImagePlugin : public QImageIOPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QImageIOHandlerFactoryInterface" FILE "formats.json")
  public:
    Capabilities capabilities(QIODevice *device, const QByteArray &format) const override
    {
        if (format == "png" || format == "jpeg" || format == "jpg")
            return CanRead | CanWrite;
        if (!device || !device->isReadable())
            return {};
        if (QPngHandler::canRead(device) || QJpegHandler::canRead(device))
            return CanRead;
        return {};
    }
    QImageIOHandler *create(QIODevice *device,
                            const QByteArray &format = QByteArray()) const override
    {
        const bool png = format == "png" || (format.isEmpty() && QPngHandler::canRead(device));
        QImageIOHandler *handler = png ? static_cast<QImageIOHandler *>(new QPngHandler)
                                       : static_cast<QImageIOHandler *>(new QJpegHandler);
        handler->setDevice(device);
        handler->setFormat(png ? "png" : "jpeg");
        return handler;
    }
};
#include "plugin.moc"
