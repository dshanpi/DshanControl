#pragma once
#include <QString>

// Keep HMI controls on loopback. Boards can choose an unprivileged listener port
// without allowing configuration to redirect local control requests off-device.
inline QString hmiApiBaseUrl()
{
    const QString configured = qEnvironmentVariable("DSHANCONTROL_HMI_API_PORT");
    if (configured.isEmpty())
        return QStringLiteral("http://127.0.0.1/api/hmi/v1");
    bool valid = false;
    const int port = configured.toInt(&valid);
    if (!valid || port < 1 || port > 65535)
        qFatal("DSHANCONTROL_HMI_API_PORT must be an integer from 1 to 65535");
    return QStringLiteral("http://127.0.0.1:%1/api/hmi/v1").arg(port);
}
