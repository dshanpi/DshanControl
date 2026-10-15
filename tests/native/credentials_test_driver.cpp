// Isolated credential/exec probe. This test executable is never installed.
#include "../../components/runtime/src/applicationcredentials.h"
#include "../../components/runtime/src/storage.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QTextStream>
#include <arpa/inet.h>
#include <errno.h>
#include <grp.h>
#include <linux/capability.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>

int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  try {
    require(argc == 3 || argc == 4, "Probe operation and argument required");
    const auto operation = QString::fromLocal8Bit(argv[1]);
    if (operation == "apply") {
      const auto input = readJson(QString::fromLocal8Bit(argv[2]));
      require(input["uid"].isDouble() && input["gid"].isDouble() &&
                  input["bind"].isBool() && input["groups"].isArray() &&
                  input["groups"].toArray().size() <= 64,
              "Invalid credential fixture");
      for (const auto &name : {QString("uid"), QString("gid")})
        require(positiveInteger(input[name], true) &&
                    input[name].toDouble() <= 2147483647,
                "Invalid fixture identity range");
      ApplicationCredentials profile{};
      profile.uid = uid_t(input["uid"].toDouble());
      profile.gid = gid_t(input["gid"].toDouble());
      profile.bindPrivilegedPort = input["bind"].toBool();
      for (const auto &group : input["groups"].toArray()) {
        require(positiveInteger(group, true) && group.toDouble() <= 2147483647,
                "Invalid fixture group");
        profile.groups[profile.groupCount++] = gid_t(group.toDouble());
      }
      bool valid = false;
      profile.lastKernelCapability =
          QString::fromUtf8(readBytes("/proc/sys/kernel/cap_last_cap", 128))
              .trimmed()
              .toInt(&valid);
      require(valid, "Kernel capability bound unavailable");
      if (input.contains("last_cap"))
        profile.lastKernelCapability = input["last_cap"].toInt(-1);
      const auto executable =
          QFile::encodeName(QCoreApplication::applicationFilePath());
      const auto port = QByteArray::number(input["port"].toInt(0));
      const auto sample = QFile::encodeName(input["sample"].toString());
      auto result = applyApplicationCredentials(profile);
      if (result.stage != CredentialStage::None) {
        QTextStream(stderr)
            << QJsonDocument(QJsonObject{{"stage", int(result.stage)},
                                         {"errno", result.error}})
                   .toJson(QJsonDocument::Compact)
            << '\n';
        return 1;
      }
      // Do not expose the private fixture configuration or inherited
      // environment to the post-drop process. Only the already-open output pipe
      // is retained.
      char *args[] = {const_cast<char *>(executable.constData()),
                      const_cast<char *>("inspect"),
                      const_cast<char *>(port.constData()),
                      const_cast<char *>(sample.constData()), nullptr};
      char path[] = "PATH=/usr/bin:/bin", home[] = "HOME=/nonexistent",
           lang[] = "LANG=C";
      char *environment[] = {path, home, lang, nullptr};
      ::execve(executable.constData(), args, environment);
      throw Failure("probe-exec", "test",
                    "Post-drop fixture exec failed; errno=" +
                        QString::number(errno));
    }
    require(operation == "inspect", "Unknown fixture operation");
    QJsonObject output;
    const auto status = readBytes("/proc/self/status", 65536);
    for (const auto &line : status.split('\n')) {
      auto split = line.indexOf(':');
      if (split < 0)
        continue;
      auto name = QString::fromLatin1(line.left(split));
      if (QStringList{"Uid", "Gid", "Groups", "CapInh", "CapPrm", "CapEff",
                      "CapBnd", "CapAmb", "NoNewPrivs"}
              .contains(name))
        output[name] = QString::fromLatin1(line.mid(split + 1)).trimmed();
    }
    bool valid = false;
    auto port = QString::fromLocal8Bit(argv[2]).toInt(&valid);
    require(valid && port >= 0 && port < 1024, "Invalid low-port fixture");
    if (port) {
      const auto socket = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
      require(socket >= 0, "Probe socket unavailable");
      struct sockaddr_in address {};
      address.sin_family = AF_INET;
      address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
      address.sin_port = htons(port);
      auto result = ::bind(socket, reinterpret_cast<sockaddr *>(&address),
                           sizeof(address));
      auto number = result == 0 ? 0 : errno;
      ::close(socket);
      output["port"] = port;
      output["bind_errno"] = number;
    }
    const auto regainUid = ::setresuid(0, 0, 0);
    output["regain_uid_errno"] = regainUid == 0 ? 0 : errno;
    const auto regainGid = ::setresgid(0, 0, 0);
    output["regain_gid_errno"] = regainGid == 0 ? 0 : errno;
    __user_cap_header_struct header{};
    header.version = _LINUX_CAPABILITY_VERSION_3;
    __user_cap_data_struct caps[2]{};
    caps[0].permitted = caps[0].effective = caps[0].inheritable =
        1U << CAP_DAC_OVERRIDE;
    const auto regainCap = ::syscall(SYS_capset, &header, caps);
    output["regain_extra_capability_errno"] = regainCap == 0 ? 0 : errno;
    if (argc == 4) {
      QFile sample(QString::fromLocal8Bit(argv[3]));
      output["sample_readable"] = sample.open(QIODevice::ReadOnly);
    }
    ApplicationCredentials second{};
    second.uid = ::getuid() + 1;
    second.gid = ::getgid();
    second.groupCount = ::getgroups(64, second.groups);
    bool boundValid = false;
    second.lastKernelCapability =
        QString::fromUtf8(readBytes("/proc/sys/kernel/cap_last_cap", 128))
            .trimmed()
            .toInt(&boundValid);
    require(boundValid, "Kernel capability bound unavailable");
    const auto changed = applyApplicationCredentials(second);
    output["second_identity_stage"] = int(changed.stage);
    output["second_identity_errno"] = changed.error;
    QTextStream(stdout) << QJsonDocument(output).toJson(QJsonDocument::Compact)
                        << '\n';
    return 0;
  } catch (const Failure &failure) {
    QTextStream(stderr) << QJsonDocument(
                               QJsonObject{{"code", failure.code},
                                           {"stage", failure.stage},
                                           {"message", failure.detail}})
                               .toJson(QJsonDocument::Compact)
                        << '\n';
    return 1;
  }
}
