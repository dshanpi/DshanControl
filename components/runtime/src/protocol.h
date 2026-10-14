#pragma once
#include "storage.h"
#include <QJsonArray>
extern const QStringList TargetFields;
void validateTarget(const QJsonObject &target);
void validateManifest(const QJsonObject &manifest, bool publicRelease);
QJsonObject matchTarget(const QJsonObject &manifest,
                        const QJsonObject &contract,
                        const QJsonObject &installed, qint64 highWater);
void verifySignature(const QByteArray &manifest, const QByteArray &signature,
                     const QString &publicKey);
