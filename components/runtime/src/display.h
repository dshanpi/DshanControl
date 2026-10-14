#pragma once
#include "updater.h"

// Paths and board bindings come exclusively from system-owned configuration.
QJsonObject displayBrightness(const QJsonObject &configuration,
                              const NativeUpdater &updater,
                              const QJsonObject &params, bool write);
