#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace DesktopV1
{
// A schema-checked, optimistic transaction over one user-owned layout file.
class DesktopLayoutStore
{
  public:
    explicit DesktopLayoutStore(QString path);
    QJsonObject load(QString *notice);
    bool initializeDefault(QString *error);
    bool commit(const QJsonObject &layout, QString *error);
    bool restoreDefaults(QString *error);
    static QJsonObject defaults();
    static bool validate(const QJsonObject &layout, QString *error);
    static bool compatible(const QString &slot, const QString &type);
    static QStringList slotIds();

  private:
    QString filePath;
    QByteArray baseline;
    bool existed = false, writableSchema = true;
};
} // namespace DesktopV1
