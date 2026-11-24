#include "fieldbus.h"
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QProcess>
#include <QProcessEnvironment>
#include <csignal>
#include <sys/prctl.h>
#include <unistd.h>

namespace {
class HardwareProcess : public QProcess {
    const pid_t owner = ::getpid();
    void setupChildProcess() override {
        if (::setsid() < 0 || ::prctl(PR_SET_PDEATHSIG, SIGKILL) < 0 || ::getppid() != owner)
            ::_exit(127);
    }
};
}
QJsonObject fieldbusRequest(const QJsonObject &configuration, const NativeUpdater &updater,
                           const QJsonObject &params) {
    require(configuration["fieldbus_enabled"] == true, "Fieldbus service is not configured",
            "fieldbus", "not-configured");
    require(params.size() == 3 && params["action"].isString() && params["params"].isObject() &&
                params["confirmed"] == true, "Confirmed fieldbus request required", "authorization");
    FileLock control(updater.controlLock());
    HardwareProcess process;
    QProcessEnvironment environment;
    environment.insert("PATH", "/usr/sbin:/usr/bin:/sbin:/bin");
    environment.insert("LANG", "C.UTF-8");
    process.setProcessEnvironment(environment);
    process.setProcessChannelMode(QProcess::SeparateChannels);
    // Executable, manifest and state locations are system owned, never RPC parameters.
    process.start("/usr/bin/dshancontrol-fieldbus-worker", {
                  "/usr/share/dshancontrol/platform.json", "/data/dshancontrol/platform"});
    require(process.waitForStarted(3000), "Cannot start fieldbus executor", "fieldbus", "executor-unavailable");
    const auto input = QJsonDocument(params).toJson(QJsonDocument::Compact);
    process.write(input);
    process.closeWriteChannel();
    QElapsedTimer elapsed; elapsed.start();
    QByteArray output, errors;
    bool overflow = false;
    while (process.state() != QProcess::NotRunning) {
        process.waitForFinished(50);
        output += process.readAllStandardOutput();
        errors += process.readAllStandardError();
        overflow = output.size() > 65536 || errors.size() > 8192;
        if (overflow || elapsed.elapsed() > 15000) {
            if (process.state() != QProcess::NotRunning) {
                ::kill(-pid_t(process.processId()), SIGKILL);
                process.waitForFinished(3000);
            }
            throw Failure(overflow ? "response-too-large" : "timeout", "fieldbus",
                          "Fieldbus operation aborted; outcome may be unknown; no automatic retry");
        }
    }
    output += process.readAllStandardOutput();
    require(output.size() <= 65536, "Fieldbus response exceeds limit", "fieldbus");
    const auto result = strictJson(output).toObject();
    require(process.exitStatus() == QProcess::NormalExit && result["ok"].isBool(),
            "Invalid executor response", "fieldbus", "executor-failed");
    if (process.exitCode() || result["ok"] != true)
        throw Failure(result["code"].toString("operation-failed"), "fieldbus",
                      result["error"].toString("Fieldbus request failed"));
    require(result["result"].isObject(), "Missing fieldbus result", "fieldbus");
    return result["result"].toObject();
}
