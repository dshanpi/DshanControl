#include "server.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonDocument>
#include <QTextStream>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QString path = "/run/dshancontrol/desktop/desktop.sock",
            configPath = "/etc/dshancontrol/desktop.json";
    auto args = app.arguments();
    args.removeFirst();
    try
    {
        while (!args.isEmpty())
        {
            const auto key = args.takeFirst();
            require(!args.isEmpty() && (key == "--socket" || key == "--config"),
                    "Usage: dshancontrol-desktop-service [--socket PATH] [--config PRIVATE_JSON]",
                    "arguments");
            const auto value = args.takeFirst();
            if (key == "--socket")
                path = value;
            else
                configPath = value;
        }
        QJsonObject config;
        if (QFileInfo::exists(configPath) || QFileInfo(configPath).isSymLink())
            config = Desktop::privateConfig(configPath);
        DesktopServer server(path, config);
        return app.exec();
    }
    catch (const Failure &error)
    {
        QTextStream(stderr) << QJsonDocument(QJsonObject{{"ok", false},
                                                         {"code", error.code},
                                                         {"stage", error.stage},
                                                         {"message", error.detail}})
                                   .toJson(QJsonDocument::Compact)
                            << '\n';
        return 1;
    }
}
