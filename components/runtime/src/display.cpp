#include "display.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <cerrno>
#include <cmath>
#include <memory>

namespace {
int driverValue(const QString &path) {
  // sysfs reports a page-sized stat length even for a short numeric attribute.
  QFile file(path);
  require(file.open(QIODevice::ReadOnly), "Backlight attribute unavailable",
          "display", "driver-unavailable");
  auto bytes = file.read(65);
  require(bytes.size() <= 64 && file.error() == QFile::NoError,
          "Backlight attribute read failed", "display", "driver-read-failed");
  bytes = bytes.trimmed();
  require(QRegularExpression("^[0-9]{1,5}$")
              .match(QString::fromLatin1(bytes))
              .hasMatch(),
          "Invalid backlight driver value", "display", "driver-value-invalid");
  return bytes.toInt();
}
} // namespace

QJsonObject displayBrightness(const QJsonObject &configuration,
                              const NativeUpdater &updater,
                              const QJsonObject &params, bool write) {
  auto binding = configuration["display"].toObject();
  auto channel = binding["channel"].toString();
  auto name = binding["backlight"].toString();
  require(
      !channel.isEmpty() &&
          QRegularExpression("^[a-zA-Z0-9_-]{1,64}$").match(name).hasMatch(),
      "System backlight binding missing", "identity", "display-unbound");
  auto manifest = readJson(configuration["platform_manifest"].toString());
  auto system = updater.systemContract();
  require(manifest["schema_version"].toDouble() == 1 &&
              manifest["board_id"] == system["board_id"] &&
              manifest["architecture"] == system["architecture"] &&
              manifest["channels"].toObject()[channel].toObject()["kind"] ==
                  "display",
          "Display binding does not match system board", "identity",
          "display-board-mismatch");
  require(params["channel"].isString() && params["channel"] == channel,
          "Display channel is not bound to this board", "identity",
          "display-channel-mismatch");
  require(params.size() == (write ? 3 : 1) &&
              (!write ||
               (positiveInteger(params["value"]) &&
                params["value"].toDouble() >= 10 &&
                params["value"].toDouble() <= 255 &&
                params["confirmed"].isBool() && params["confirmed"].toBool())),
          "Brightness requires a confirmed integer 10..255; no path overrides",
          "authorization", "display-request-invalid");
  auto root = configuration["backlight_root"].toString("/sys/class/backlight");
  auto updateConfig = readJson(configuration["updater_config"].toString());
  require(root == "/sys/class/backlight" ||
              (updateConfig["development_mode"].isBool() &&
               updateConfig["development_mode"].toBool()),
          "Alternate backlight roots are restricted to development fixtures",
          "identity", "display-root-invalid");
  require(QDir::isAbsolutePath(root) && QDir::cleanPath(root) == root,
          "Invalid system backlight root", "identity");
  auto path = root + '/' + name + "/brightness";
  int maximum = driverValue(root + '/' + name + "/max_brightness");
  require(maximum > 0 && maximum <= 65535, "Invalid backlight maximum",
          "display", "driver-value-invalid");
  std::unique_ptr<FileLock> ownership;
  int requestedNative = -1;
  if (write) {
    ownership.reset(new FileLock(updater.controlLock()));
    int native = static_cast<int>(
        std::lround(params["value"].toDouble() * maximum / 255.0));
    // Always keep an illuminated minimum even on low-resolution drivers.
    native = qMax(1, native);
    requestedNative = native;
    QFile output(path);
    errno = 0;
    if (!output.open(QIODevice::WriteOnly))
      throw Failure(QString::number(errno), "display-write",
                    output.errorString());
    auto bytes = QByteArray::number(native) + '\n';
    errno = 0;
    if (output.write(bytes) != bytes.size() || !output.flush())
      throw Failure(QString::number(errno), "display-write",
                    output.errorString());
    output.close();
  }
  int actual = driverValue(path);
  require(actual >= 0 && actual <= maximum, "Backlight readback out of range",
          "display", "driver-value-invalid");
  require(!write || actual == requestedNative,
          "Backlight driver did not retain the requested value", "display",
          "driver-readback-mismatch");
  return {
      {"channel", channel},
      {"brightness", static_cast<int>(std::lround(actual * 255.0 / maximum))},
      {"driver_brightness", actual},
      {"driver_maximum", maximum},
      {"source", "driver-configured-brightness"}};
}
