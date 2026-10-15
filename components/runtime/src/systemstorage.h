#pragma once
#include "storage.h"

QString bootPersistentDevice(const QByteArray &cmdline);
bool persistentMountPresent(const QByteArray &mountinfo, const QString &mount,
                            const QString &deviceIdentity);
QJsonObject ensureSystemStorage(const QJsonObject &config, bool mount);
QJsonObject initializeApplication(const QJsonObject &config);
