#pragma once
#include <QIODevice>
#include <cstring>
#include <utility>

// A consumed control payload cannot be rewound by Qt's HTTP reconnect path.
// Pair with DoNotBufferUploadDataAttribute and an explicit ContentLengthHeader.
class OneShotUpload final : public QIODevice
{
  public:
    explicit OneShotUpload(QByteArray bytes) : payload(std::move(bytes))
    {
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
    }
    bool isSequential() const override { return true; }
    bool reset() override { return false; }
    bool seek(qint64) override { return false; }
    qint64 bytesAvailable() const override
    {
        return payload.size() - offset + QIODevice::bytesAvailable();
    }

  protected:
    qint64 readData(char *data, qint64 maximum) override
    {
        const qint64 count = qMin(maximum, qint64(payload.size()) - offset);
        if (count <= 0)
            return 0;
        std::memcpy(data, payload.constData() + offset, size_t(count));
        offset += count;
        return count;
    }
    qint64 writeData(const char *, qint64) override { return -1; }

  private:
    QByteArray payload;
    qint64 offset = 0;
};
