#pragma once
#include "storage.h"
#include <sys/types.h>
void validateApplicationPeers(const QJsonObject &service,
                              const QJsonObject &updater);
void authorizeApplicationPeer(const QJsonObject &service, uid_t uid,
                              const QString &method);
