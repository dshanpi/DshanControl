#include "providers.h"
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QUrlQuery>
#include <cstring>
#include <memory>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace Desktop;
namespace
{
class OneShotBody : public QIODevice
{
    QByteArray bytes;
    qint64 offset = 0;

  public:
    explicit OneShotBody(const QByteArray &body, QObject *parent) : QIODevice(parent), bytes(body)
    {
        open(QIODevice::ReadOnly);
    }
    bool isSequential() const override { return true; }
    bool reset() override { return false; }
    qint64 bytesAvailable() const override
    {
        return bytes.size() - offset + QIODevice::bytesAvailable();
    }

  protected:
    qint64 readData(char *out, qint64 max) override
    {
        const auto count = qMin(max, qint64(bytes.size()) - offset);
        if (count <= 0)
            return -1;
        memcpy(out, bytes.constData() + offset, size_t(count));
        offset += count;
        return count;
    }
    qint64 writeData(const char *, qint64) override { return -1; }
};
QUrl https(const QString &value, bool origin = false)
{
    QUrl url(value, QUrl::StrictMode);
    require(url.isValid() && url.scheme() == "https" && !url.host().isEmpty() &&
                url.userInfo().isEmpty() && !url.hasFragment() && !url.hasQuery() &&
                (!origin || url.path().isEmpty() || url.path() == "/"),
            "HTTPS endpoint required", "configuration");
    return url;
}
QUrl homeUrl(const QJsonObject &config, const QString &path)
{
    auto url = https(config["home"].toObject()["endpoint"].toString(), true);
    url.setPath(path);
    return url;
}
QByteArray credential(const QJsonObject &config, const QString &key)
{
    auto bytes = privateBytes(config[key].toString(), 2048).trimmed();
    require(!bytes.isEmpty() && bytes.size() <= 1024 && !bytes.contains('\0') &&
                !bytes.contains('\n') && !bytes.contains('\r'),
            "Invalid credential file", "configuration");
    return bytes;
}
QString homeKey(const QString &id, const QString &code) { return id + '/' + code; }
} // namespace
DesktopProviders::DesktopProviders(const QJsonObject &configuration, QObject *parent)
    : QObject(parent), config(configuration), cloudMqtt(this), sensorMqtt(this)
{
    network.setProxy(QNetworkProxy::NoProxy);
    cloud = {{"configured", false},
             {"mqtt", "not_configured"},
             {"error", ""},
             {"busy", false},
             {"pairing_error", ""}};
    home = {{"configured", false},
            {"authorization", "not_configured"},
            {"fresh", false},
            {"busy", false},
            {"error", ""}};
    voice = {{"configured", false}, {"ready", false}, {"state", "not_configured"}};
    reporting = {{"enabled", false},
                 {"period_seconds", 30},
                 {"persistent", false},
                 {"last_broker_ack", QJsonValue()},
                 {"error", ""}};
    stateDirectory = config["state_directory"].toString();
    if (!stateDirectory.isEmpty())
    {
        require(QDir::isAbsolutePath(stateDirectory) &&
                    QDir::cleanPath(stateDirectory) == stateDirectory &&
                    !QFileInfo(stateDirectory).isSymLink() && QDir().mkpath(stateDirectory),
                "Invalid private state directory", "configuration");
        struct stat st;
        require(::lstat(QFile::encodeName(stateDirectory).constData(), &st) == 0 &&
                    S_ISDIR(st.st_mode) && st.st_uid == ::geteuid() && !(st.st_mode & 0077),
                "Owner-only state directory required", "configuration");
        const auto path = stateDirectory + "/reporting.json";
        if (QFileInfo::exists(path) || QFileInfo(path).isSymLink())
        {
            auto stored = privateConfig(path);
            exactFields(stored, {"enabled", "period_seconds"});
            require(stored["enabled"].isBool() &&
                        (stored["period_seconds"] == 30 || stored["period_seconds"] == 60 ||
                         stored["period_seconds"] == 300),
                    "Invalid reporting settings", "configuration");
            reporting["enabled"] = stored["enabled"];
            reporting["period_seconds"] = stored["period_seconds"];
        }
        reporting["persistent"] = true;
    }
    const auto c = config["cloud"].toObject();
    if (!c.isEmpty())
    {
        require(validId(c["device_id"].toString()), "Registered cloud device identity required",
                "configuration");
        https(c["pairing_url"].toString());
        pairing.deviceId = c["device_id"].toString();
        cloudSecret = credential(c, "secret_file");
        cloud["configured"] = true;
        cloud["mqtt"] = c["mqtt"].isObject() ? "offline" : "not_configured";
    }
    const auto h = config["home"].toObject();
    if (!h.isEmpty())
    {
        https(h["endpoint"].toString(), true);
        homeToken = credential(h, "token_file");
        require(QRegularExpression("^[A-Za-z0-9._~-]{16,1024}$")
                    .match(QString::fromLatin1(homeToken))
                    .hasMatch(),
                "Invalid restricted home token", "configuration");
        home["configured"] = true;
        home["authorization"] = "unknown";
    }
    const auto ingress = config["sensor_mqtt"].toObject();
    if (!ingress.isEmpty())
    {
        require(validId(ingress["client_id"].toString()) &&
                    validId(ingress["username"].toString()) && ingress["sources"].isArray() &&
                    ingress["sources"].toArray().size() <= 32,
                "Invalid MQTT sensor configuration", "configuration");
        QSet<QString> topics, ids;
        for (const auto &entry : ingress["sources"].toArray())
        {
            const auto source = entry.toObject();
            const auto id = source["device_id"].toString();
            const auto topic = source["topic"].toString();
            require(validId(id) && !ids.contains(id) && topic == "devices/" + id + "/telemetry" &&
                        !topics.contains(topic) && source["points"].isObject() &&
                        source["points"].toObject().size() <= 64,
                    "Explicit normalized sensor topics required", "configuration");
            ids.insert(id);
            topics.insert(topic);
        }
        sensorSecret = credential(ingress, "password_file");
    }
    connect(&cloudMqtt, &MqttConnection::connected, this, [this]() {
        cloud["mqtt"] = "connected";
        cloud["error"] = "";
        emit changed();
    });
    connect(&cloudMqtt, &MqttConnection::failed, this, [this](const QString &code) {
        cloud["mqtt"] = "offline";
        cloud["error"] = code;
        nextCloudConnect = monotonicMs() + 5000;
        emit changed();
    });
    connect(&cloudMqtt, &MqttConnection::acknowledged, this, [this](int packet) {
        if (packet == reportPacket)
        {
            reportPacket = 0;
            reporting["last_broker_ack"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
            reporting["error"] = "";
            emit changed();
        }
    });
    connect(&cloudMqtt, &MqttConnection::unconfirmed, this, [this](int packet) {
        if (packet == reportPacket)
        {
            reportPacket = 0;
            reporting["error"] = "delivery_unknown";
            emit changed();
        }
    });
    connect(&sensorMqtt, &MqttConnection::failed, this,
            [this](const QString &) { nextSensorConnect = monotonicMs() + 5000; });
    connect(&sensorMqtt, &MqttConnection::message, this,
            [this, ingress](const QString &topic, const QByteArray &bytes, bool retained) {
                try
                {
                    require(!retained, "Retained sensor samples are not live readings",
                            "telemetry");
                    const auto value = strictJson(bytes);
                    require(value.isObject(), "Telemetry object required", "telemetry");
                    for (const auto &entry : ingress["sources"].toArray())
                    {
                        const auto source = entry.toObject();
                        if (source["topic"] != topic)
                            continue;
                        points.ingest(source["device_id"].toString(), value.toObject(),
                                      source["points"].toObject(), monotonicMs(),
                                      QDateTime::currentSecsSinceEpoch(), "MQTT");
                        emit changed();
                    }
                }
                catch (const Failure &)
                {
                    reporting["ingress_error"] = "invalid_sensor_message";
                }
            });
    const auto v = config["voice"].toObject();
    if (!v.isEmpty())
    {
        require(QDir::isAbsolutePath(v["socket"].toString()) &&
                    positiveInteger(v["runtime_uid"], true),
                "Explicit voice runtime identity required", "configuration");
        voice["configured"] = true;
        voice["state"] = "offline";
    }
    connect(&voiceSocket, &QLocalSocket::connected, this, [this, v]() {
        struct ucred peer;
        socklen_t size = sizeof(peer);
        if (::getsockopt(int(voiceSocket.socketDescriptor()), SOL_SOCKET, SO_PEERCRED, &peer,
                         &size) ||
            peer.uid != uid_t(v["runtime_uid"].toDouble()))
        {
            voiceSocket.abort();
            return;
        }
        voiceHeartbeat = monotonicMs();
        voiceBuffer.clear();
        voiceSocket.write("{\"version\":1,\"cmd\":\"status\"}\n");
    });
    connect(&voiceSocket, &QLocalSocket::readyRead, this, &DesktopProviders::handleVoice);
    connect(&voiceSocket, &QLocalSocket::disconnected, this, &DesktopProviders::voiceUnavailable);
    tick.setInterval(1000);
    connect(&tick, &QTimer::timeout, this, &DesktopProviders::update);
    tick.start();
}
void DesktopProviders::request(const QString &job, const QUrl &url, const QByteArray &method,
                               const QByteArray &body, const QByteArray &contentType,
                               const QByteArray &token,
                               std::function<void(const QJsonObject &, const QString &)> done)
{
    require(!jobs.contains(job), "Request already running", "request", "busy");
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::ManualRedirectPolicy);
    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    req.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
    req.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    if (url.scheme() == "https")
    {
        auto ssl = QSslConfiguration::defaultConfiguration();
        ssl.setPeerVerifyMode(QSslSocket::VerifyPeer);
        ssl.setProtocol(QSsl::TlsV1_2OrLater);
        if (config.contains("https_ca_file"))
        {
            const auto certs = QSslCertificate::fromData(
                readBytes(config["https_ca_file"].toString(), 1024 * 1024));
            require(!certs.isEmpty(), "HTTPS CA unavailable", "configuration");
            auto all = ssl.caCertificates();
            all.append(certs);
            ssl.setCaCertificates(all);
        }
        req.setSslConfiguration(ssl);
    }
    if (!token.isEmpty())
        req.setRawHeader("Authorization", "Bearer " + token);
    jobs.insert(job);
    QNetworkReply *reply;
    if (job == "home.command")
    {
        // A consumed sequential upload cannot be rewound by Qt's internal HTTP
        // recovery. The gateway should also honor the stable idempotency key.
        req.setAttribute(QNetworkRequest::DoNotBufferUploadDataAttribute, true);
        req.setHeader(QNetworkRequest::ContentLengthHeader, body.size());
        req.setRawHeader("Idempotency-Key", commandOrder.last().toLatin1());
        auto *upload = new OneShotBody(body, this);
        reply = network.sendCustomRequest(req, method, upload);
        upload->setParent(reply);
    }
    else
        reply = network.sendCustomRequest(req, method, body);
    reply->setReadBufferSize(131073);
    connect(reply, &QNetworkReply::sslErrors, reply, [reply](const QList<QSslError> &errors) {
        QStringList codes;
        for (const auto &error : errors)
            codes << QString::number(int(error.error()));
        reply->setProperty("tls_error_codes", codes.join(','));
    });
    auto received = std::make_shared<QByteArray>();
    connect(reply, &QNetworkReply::readyRead, this, [reply, received]() {
        *received += reply->readAll();
        if (received->size() > 131072)
            reply->abort();
    });
    QTimer::singleShot(15000, reply, [reply]() {
        if (reply->isRunning())
            reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, received, job, done]() {
        jobs.remove(job);
        *received += reply->readAll();
        QJsonObject root;
        QString error;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 401 || status == 403)
            error = "authorization_required";
        else if (reply->error() != QNetworkReply::NoError)
            error = "transport_failed_" + QString::number(int(reply->error())) +
                    (reply->property("tls_error_codes").toString().isEmpty()
                         ? QString()
                         : "_tls_" + reply->property("tls_error_codes").toString());
        else if (status != 200)
            error = "http_" + QString::number(status);
        else if (received->size() > 131072)
            error = "response_too_large";
        else
        {
            try
            {
                const auto value = strictJson(*received);
                require(value.isObject(), "Response object required");
                root = value.toObject();
            }
            catch (const Failure &)
            {
                error = "invalid_response";
            }
        }
        try
        {
            done(root, error);
        }
        catch (const Failure &)
        {
            if (job.startsWith("home"))
            {
                home["error"] = "invalid_response";
                home["fresh"] = false;
                home["busy"] = false;
            }
            else if (job == "pairing")
            {
                cloud["pairing_error"] = "invalid_response";
                cloud["busy"] = false;
                pairing.fresh = false;
            }
            else
                reporting["error"] = "invalid_local_sample";
        }
        received->fill(0);
        reply->deleteLater();
        emit changed();
    });
}
void DesktopProviders::refreshPairing()
{
    require(cloud["configured"] == true && !cloud["busy"].toBool(), "Device registration required",
            "pairing", "not-configured");
    const auto c = config["cloud"].toObject();
    const QByteArray body = "deviceId=" + QUrl::toPercentEncoding(pairing.deviceId) +
                            "&secret=" + QUrl::toPercentEncoding(QString::fromUtf8(cloudSecret));
    const auto started = monotonicMs();
    request("pairing", https(c["pairing_url"].toString()), "POST", body,
            "application/x-www-form-urlencoded", {},
            [this, started](const QJsonObject &root, const QString &error) {
                cloud["busy"] = false;
                cloud["pairing_error"] = error;
                pairing.fresh = false;
                if (error.isEmpty())
                    pairing.apply(root, monotonicMs() - started, monotonicMs());
            });
    cloud["busy"] = true;
}
void DesktopProviders::refreshHome()
{
    require(home["configured"] == true, "Home authorization not configured", "home",
            "not-configured");
    require(!home["busy"].toBool(), "Home request running", "home", "busy");
    request("home.devices", homeUrl(config, "/v1/devices"), "GET", {}, "application/json",
            homeToken, [this](const QJsonObject &root, const QString &error) {
                home["busy"] = false;
                home["error"] = error;
                if (!error.isEmpty())
                {
                    home["fresh"] = false;
                    if (error == "authorization_required")
                        home["authorization"] = "expired";
                    return;
                }
                auto next = validateDevices(root);
                devices = next;
                ++revision;
                lastHomeSync = monotonicMs();
                home["fresh"] = true;
                home["authorization"] = "authorized";
                for (const auto &entry : devices)
                {
                    const auto device = entry.toObject();
                    for (auto it = pending.begin(); it != pending.end();)
                    {
                        if (it->value("device_id") == device["id"] && device["online"] == true &&
                            reportedValue(device, it->value("code").toString()) ==
                                it->value("value"))
                            it = pending.erase(it);
                        else
                            ++it;
                    }
                }
            });
    home["busy"] = true;
}
void DesktopProviders::fetchFunctions(const QString &id)
{
    require(validId(id) && home["fresh"] == true && monotonicMs() - lastHomeSync <= 30000 &&
                !home["busy"].toBool(),
            "Fresh authorized device list required", "home");
    bool known = false;
    for (const auto &entry : devices)
        if (entry.toObject()["id"] == id)
            known = true;
    require(known, "Unknown household device", "home");
    request("home.functions", homeUrl(config, "/v1/devices/" + id + "/functions"), "GET", {},
            "application/json", homeToken,
            [this, id](const QJsonObject &root, const QString &error) {
                home["busy"] = false;
                home["error"] = error;
                if (!error.isEmpty())
                {
                    functions.remove(id);
                    functionTimes.remove(id);
                    return;
                }
                functions[id] = validateFunctions(root);
                functionTimes[id] = monotonicMs();
            });
    home["busy"] = true;
}
void DesktopProviders::command(const QJsonObject &params)
{
    exactFields(params, {"device_id", "code", "value", "command_id", "revision", "confirmed"});
    const auto id = params["device_id"].toString(), code = params["code"].toString(),
               commandId = params["command_id"].toString();
    require(validId(id) && validId(code) && params["confirmed"] == true &&
                QRegularExpression("^[0-9a-f]{32}$").match(commandId).hasMatch() &&
                !commandIds.contains(commandId) && params["revision"] == double(revision) &&
                home["fresh"] == true && home["authorization"] == "authorized" &&
                monotonicMs() - lastHomeSync <= 30000 && !home["busy"].toBool() &&
                functionTimes.contains(id) && monotonicMs() - functionTimes[id] <= 30000,
            "Command requires fresh authorization and explicit intent", "home", "stale-or-denied");
    bool online = false;
    for (const auto &entry : devices)
        if (entry.toObject()["id"] == id)
            online = entry.toObject()["online"] == true;
    require(online && !pending.contains(homeKey(id, code)), "Device offline or command unconfirmed",
            "home");
    QJsonObject function;
    for (const auto &entry : functions.value(id))
        if (entry.toObject()["code"] == code)
            function = entry.toObject();
    require(!function.isEmpty(), "Unknown capability", "home");
    validateCommand(function, params["value"]);
    commandIds.insert(commandId);
    commandOrder.append(commandId);
    if (commandOrder.size() > 256)
        commandIds.remove(commandOrder.takeFirst());
    const auto key = homeKey(id, code);
    pending[key] = {{"device_id", id},
                    {"code", code},
                    {"value", params["value"]},
                    {"command_id", commandId},
                    {"state", "sending"}};
    auto body = QJsonDocument(QJsonObject{{"code", code}, {"value", params["value"]}})
                    .toJson(QJsonDocument::Compact);
    request(
        "home.command", homeUrl(config, "/v1/devices/" + id + "/commands"), "POST", body,
        "application/json", homeToken, [this, key](const QJsonObject &root, const QString &error) {
            home["busy"] = false;
            home["error"] = error;
            if (error == "authorization_required" || (!root.isEmpty() && root["accepted"] == false))
            {
                pending.remove(key);
                if (error == "authorization_required")
                {
                    home["authorization"] = "expired";
                    home["fresh"] = false;
                }
                else
                    home["error"] = "command_rejected";
                return;
            }
            pending[key]["state"] = error.isEmpty() && root["accepted"] == true
                                        ? "awaiting_confirmation"
                                        : "result_unknown";
            // GET is read-only. Never retry POST on timeout, reconnect or a UI refresh.
            if (error.isEmpty() && root["accepted"] == true)
                refreshHome();
        });
    home["busy"] = true;
}
void DesktopProviders::report()
{
    require(cloudMqtt.online() && !reportPacket, "Cloud broker not ready", "reporting",
            "not-connected");
    const auto payload = points.cloudPayload(monotonicMs());
    require(!payload["data"].toArray().isEmpty(), "No fresh valid sensor values", "reporting",
            "no-data");
    reportPacket = cloudMqtt.publish("100ask/device/" + pairing.deviceId + "/sensor",
                                     QJsonDocument(payload).toJson(QJsonDocument::Compact));
    lastReportAttempt = monotonicMs();
    reporting["error"] = "";
}
void DesktopProviders::pollLocal()
{
    if (jobs.contains("local") || !config["local_points"].isObject())
        return;
    request("local", QUrl("http://127.0.0.1/api/hmi/v1/snapshot"), "GET", {}, "application/json",
            {}, [this](const QJsonObject &root, const QString &error) {
                if (!error.isEmpty() || root["ok"] != true ||
                    root["settings"].toObject()["demo_mode"] == true ||
                    !positiveInteger(root["timestamp"]))
                    return;
                const qint64 timestamp = qint64(root["timestamp"].toDouble());
                if (timestamp <= lastLocalTimestamp)
                    return;
                auto mapping = config["local_points"].toObject();
                QJsonObject values;
                for (const auto &entry : root["sensors"].toArray())
                {
                    auto item = entry.toObject();
                    const auto key = item["name"].toString();
                    const auto spec = mapping[key].toObject();
                    if (!mapping.contains(key) || item["source"] != "real" ||
                        item["unit"] != spec["input_unit"] || !item["value"].isDouble() ||
                        (item.contains("online") && item["online"] != true) ||
                        (item["state"] != "正常" && item["state"] != "online"))
                        continue;
                    values[key] = item["value"];
                }
                if (values.isEmpty())
                    return;
                QJsonObject message{
                    {"schema_version", 1},
                    {"device_id", "local"},
                    {"timestamp",
                     QDateTime::fromSecsSinceEpoch(timestamp, Qt::UTC).toString(Qt::ISODate)},
                    {"sequence", double(++localSequence)},
                    {"quality", "good"},
                    {"values", values}};
                points.ingest("local", message, mapping, monotonicMs(),
                              QDateTime::currentSecsSinceEpoch(), "本地采集");
                lastLocalTimestamp = timestamp;
            });
}
void DesktopProviders::voiceUnavailable()
{
    voice["ready"] = false;
    voice["state"] = "offline";
    voiceStopAt = 0;
    voiceBuffer.clear();
    emit changed();
}
void DesktopProviders::handleVoice()
{
    voiceBuffer += voiceSocket.readAll();
    try
    {
        require(voiceBuffer.size() <= 16384, "Voice event too large", "voice");
        int end;
        while ((end = voiceBuffer.indexOf('\n')) >= 0)
        {
            auto value = strictJson(voiceBuffer.left(end));
            voiceBuffer.remove(0, end + 1);
            require(value.isObject(), "Voice event required", "voice");
            const auto event = value.toObject();
            require(event["version"] == 1, "Unsupported voice bridge version", "voice");
            if (event["event"] == "status")
            {
                const auto state = event["state"].toString();
                require(QStringList{"idle", "listening", "thinking", "speaking", "error"}.contains(
                            state) &&
                            event["activated"].isBool() && event["microphone_ready"].isBool() &&
                            event["speaker_ready"].isBool(),
                        "Incomplete voice readiness", "voice");
                voice["ready"] = event["activated"] == true && event["microphone_ready"] == true &&
                                 event["speaker_ready"] == true;
                voice["state"] = state;
                voiceHeartbeat = monotonicMs();
            }
            else if ((event["event"] == "asr" || event["event"] == "tts") && voice["ready"] == true)
            {
                require(event["text"].isString() && event["text"].toString().size() <= 1024,
                        "Invalid voice text", "voice");
                history.append(
                    QJsonObject{{"speaker", event["event"] == "asr" ? "user" : "assistant"},
                                {"text", event["text"]}});
                while (history.size() > 32)
                    history.removeFirst();
            }
        }
        emit changed();
    }
    catch (const Failure &)
    {
        voiceSocket.abort();
        voiceUnavailable();
    }
}
void DesktopProviders::voiceCommand(const QString &command)
{
    const QSet<QString> allowed{"start_listen", "stop_listen", "enter_free_chat", "exit_free_chat"};
    require(allowed.contains(command) && voice["ready"] == true &&
                voiceSocket.state() == QLocalSocket::ConnectedState &&
                monotonicMs() - voiceHeartbeat <= 5000,
            "Voice runtime, activation and audio readiness required", "voice", "not-ready");
    voiceSocket.write(QJsonDocument(QJsonObject{{"version", 1}, {"cmd", command}})
                          .toJson(QJsonDocument::Compact) +
                      '\n');
    voiceStopAt = command == "start_listen"
                      ? monotonicMs() + 30000
                      : command == "enter_free_chat" ? monotonicMs() + 120000 : 0;
}
void DesktopProviders::update()
{
    const auto now = monotonicMs();
    try
    {
        const auto c = config["cloud"].toObject();
        if (cloud["configured"] == true && c["mqtt"].isObject() && !cloudMqtt.online() &&
            !cloudMqtt.connecting() && now >= nextCloudConnect)
        {
            nextCloudConnect = now + 30000;
            cloudMqtt.start(c["mqtt"].toObject(), "dshan-" + pairing.deviceId, pairing.deviceId,
                            cloudSecret, {});
            cloud["mqtt"] = "connecting";
        }
        const auto s = config["sensor_mqtt"].toObject();
        if (!s.isEmpty() && !sensorMqtt.online() && !sensorMqtt.connecting() &&
            now >= nextSensorConnect)
        {
            QStringList topics;
            for (const auto &source : s["sources"].toArray())
                topics << source.toObject()["topic"].toString();
            nextSensorConnect = now + 30000;
            sensorMqtt.start(s, s["client_id"].toString(), s["username"].toString(), sensorSecret,
                             topics);
        }
        if (now >= nextLocalPoll)
        {
            nextLocalPoll = now + 3000;
            pollLocal();
        }
        if (reporting["enabled"] == true &&
            now - lastReportAttempt >= reporting["period_seconds"].toInt() * 1000 &&
            cloudMqtt.online() && !reportPacket)
        {
            lastReportAttempt = now;
            if (!points.cloudPayload(now)["data"].toArray().isEmpty())
                report();
        }
    }
    catch (const Failure &failure)
    {
        reporting["error"] = failure.code;
    }
    if (voice["configured"] == true)
    {
        if (voiceSocket.state() == QLocalSocket::UnconnectedState)
            voiceSocket.connectToServer(config["voice"].toObject()["socket"].toString());
        if (voiceSocket.state() == QLocalSocket::ConnectedState && now - voiceHeartbeat > 5000)
        {
            voiceSocket.write("{\"version\":1,\"cmd\":\"stop_listen\"}\n{\"version\":1,\"cmd\":"
                              "\"exit_free_chat\"}\n");
            voiceSocket.flush();
            voiceSocket.disconnectFromServer();
            voiceUnavailable();
        }
        else if (voiceStopAt && now >= voiceStopAt)
        {
            voiceStopAt = 0;
            voiceSocket.write("{\"version\":1,\"cmd\":\"stop_listen\"}\n{\"version\":1,\"cmd\":"
                              "\"exit_free_chat\"}\n");
        }
    }
}
QJsonObject DesktopProviders::snapshot(bool privileged) const
{
    const auto now = monotonicMs();
    auto c = cloud;
    auto p = pairing.snapshot(now);
    if (!privileged)
        p.remove("code");
    c["pairing"] = p;
    auto h = home;
    h["devices"] = devices;
    h["revision"] = double(revision);
    if (now - lastHomeSync > 30000)
        h["fresh"] = false;
    QJsonObject capabilities, waiting;
    for (auto it = functions.begin(); it != functions.end(); ++it)
        capabilities[it.key()] = it.value();
    for (auto it = pending.begin(); it != pending.end(); ++it)
        waiting[it.key()] = it.value();
    h["functions"] = capabilities;
    h["pending"] = waiting;
    auto v = voice;
    if (now - voiceHeartbeat > 5000)
        v["ready"] = false;
    if (privileged)
        v["history"] = history;
    auto r = reporting;
    r["inflight"] = reportPacket != 0;
    r["sensor_mqtt"] = sensorMqtt.online() ? "connected" : "offline";
    r["points"] = points.snapshot(now);
    return {{"version", 1}, {"cloud", c}, {"reporting", r}, {"home", h}, {"voice", v}};
}
QJsonObject DesktopProviders::dispatch(const QString &method, const QJsonObject &params)
{
    if (method == "cloud.refresh")
    {
        exactFields(params, {});
        refreshPairing();
    }
    else if (method == "reporting.send")
    {
        exactFields(params, {});
        report();
    }
    else if (method == "reporting.configure")
    {
        exactFields(params, {"enabled", "period_seconds"});
        require(!stateDirectory.isEmpty() && params["enabled"].isBool() &&
                    (params["period_seconds"] == 30 || params["period_seconds"] == 60 ||
                     params["period_seconds"] == 300),
                "Persistent reporting configuration required", "configuration");
        durableJson(stateDirectory + "/reporting.json", params);
        reporting["enabled"] = params["enabled"];
        reporting["period_seconds"] = params["period_seconds"];
    }
    else if (method == "home.refresh")
    {
        exactFields(params, {});
        refreshHome();
    }
    else if (method == "home.functions")
    {
        exactFields(params, {"device_id"});
        fetchFunctions(params["device_id"].toString());
    }
    else if (method == "home.command")
        command(params);
    else if (method == "voice.command")
    {
        exactFields(params, {"command"});
        voiceCommand(params["command"].toString());
    }
    else if (method == "voice.clear")
    {
        exactFields(params, {});
        history = {};
    }
    else
        throw Failure("unknown-method", "request", "Unknown desktop method");
    emit changed();
    return {{"accepted", true}};
}
