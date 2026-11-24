#pragma once
#include "updater.h"
QJsonObject fieldbusRequest(const QJsonObject &configuration, const NativeUpdater &updater,
                           const QJsonObject &params);
