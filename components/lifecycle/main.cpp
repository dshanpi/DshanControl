#include "../native/common.h"
#include <QThread>
#include <cstring>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <time.h>
using namespace Native;
namespace {
const QString config = "/etc/dshancontrol/imx6ull-maintenance.json";
const QString base = "/run/dshancontrol",
              app = "/data/dshancontrol/application";
struct stat metadata(const QString &path) {
  struct stat value{};
  check(::lstat(path.toUtf8().constData(), &value) == 0,
        "Cannot inspect path: " + path);
  check(!S_ISLNK(value.st_mode), "Linked system path: " + path);
  return value;
}
void protectedDirectory(const QString &path) {
  const auto value = metadata(path);
  check(S_ISDIR(value.st_mode) && value.st_uid == 0 && !(value.st_mode & 0022),
        "Untrusted directory: " + path);
}
QString readText(const QString &path) {
  return QString::fromUtf8(readBytes(path, 1024 * 1024)).trimmed();
}
void storage() {
  for (auto path :
       QStringList{"/etc/dshancontrol", "/data", "/data/dshancontrol", app})
    protectedDirectory(path);
  const auto stat = metadata(config);
  check(S_ISREG(stat.st_mode) && stat.st_uid == 0 && !(stat.st_mode & 0022),
        "Untrusted maintenance configuration");
  const auto cfg = readJson(config);
  check(cfg["scope"] == "unconnected-laboratory",
        "Field installation requires its own maintenance interlock");
  check(readText("/sys/class/block/mmcblk1/device/cid") ==
            cfg["emmc_cid"].toString(),
        "Wrong eMMC");
  check(readBytes("/proc/device-tree/compatible", 4096)
            .split('\0')
            .contains("fsl,imx6ull"),
        "Wrong SoC");
  const QList<QPair<int, int>> partitions{
      {16384, 65536}, {81920, 1572864}, {1654784, 524288}};
  for (int i = 0; i < partitions.size(); ++i) {
    auto prefix = "/sys/class/block/mmcblk1p" + QString::number(i + 1);
    check(integer(readText(prefix + "/start"), "partition start", 0, INT_MAX) ==
                  partitions[i].first &&
              integer(readText(prefix + "/size"), "partition size", 0,
                      INT_MAX) == partitions[i].second,
          "Partition layout changed");
  }
  const auto dev = metadata("/dev/mmcblk1p3");
  check(S_ISBLK(dev.st_mode), "Data device is not a block device");
  const auto id = QString::number(major(dev.st_rdev)) + ":" +
                  QString::number(minor(dev.st_rdev));
  int matches = 0;
  for (auto row : readText("/proc/self/mountinfo").split('\n')) {
    auto f = row.split(' ');
    if (f.size() < 10 || f[4] != "/data")
      continue;
    ++matches;
    int sep = f.indexOf("-");
    check(f[2] == id && f[3] == "/" && sep >= 6 && sep + 3 < f.size() &&
              f[sep + 1] == "ext4" && f[5].split(',').contains("rw"),
          "Wrong or read-only data mount");
  }
  check(matches == 1, "Data mount missing or ambiguous");
}
void maintenance() {
  storage();
  auto links = strictJson(command("/sbin/ip", {"-details", "-json", "link",
                                               "show", "dev", "can0"}))
                   .toArray();
  check(links.size() == 1, "CAN identity unavailable");
  auto link = links[0].toObject();
  check(link["ifname"] == "can0" && link["flags"].isArray(),
        "Invalid CAN identity");
  if (link["flags"].toArray().contains("UP"))
    check(link["linkinfo"]
              .toObject()["info_data"]
              .toObject()["ctrlmode"]
              .toArray()
              .contains("LOOPBACK"),
          "CAN field mode is active");
  if (readText("/sys/class/net/eth1/operstate") != "down")
    check(readText("/sys/class/net/eth1/carrier") == "0",
          "EtherCAT cable is connected");
  struct stat serial{};
  check(::stat("/dev/ttymxc2", &serial) == 0 && S_ISCHR(serial.st_mode),
        "RS485 device unavailable");
  for (auto pid : QDir("/proc").entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
    if (!QRegularExpression("^[0-9]+$").match(pid).hasMatch())
      continue;
    const QString directory = "/proc/" + pid + "/fd";
    for (auto fd : QDir(directory).entryList(QDir::AllEntries | QDir::System |
                                             QDir::NoDotAndDotDot)) {
      struct stat target{};
      const auto name = (directory + "/" + fd).toUtf8();
      if (::stat(name.constData(), &target) != 0) {
        check(errno == ENOENT, "Cannot inspect active file descriptor");
        continue;
      }
      check(!S_ISCHR(target.st_mode) || target.st_rdev != serial.st_rdev,
            "RS485 has an active owner");
    }
  }
}
void health() {
  storage();
  const auto current = QFileInfo(app + "/current").canonicalFilePath();
  check(!current.isEmpty() &&
            QFileInfo(current).absolutePath() == app + "/releases",
        "Current outside release directory");
  const auto identity = readJson(current + "/installed.json");
  const bool native =
      QFileInfo(current + "/bin/dshancontrol-api").isExecutable();
  const QMap<QString, QPair<int, QString>> processes{
      {"hmi", {401, current + "/bin/omnigate-hmi"}},
      {"desktop", {402, current + "/bin/dshancontrol-desktop-service"}},
      {"web",
       {400, native ? current + "/bin/dshancontrol-api"
                    : QFileInfo("/usr/bin/python3").canonicalFilePath()}}};
  for (auto i = processes.begin(); i != processes.end(); ++i) {
    auto pid = integer(readText(base + "/" + i.key() + ".pid"), "service PID",
                       2, INT_MAX);
    auto proc = "/proc/" + QString::number(pid);
    check(QFileInfo(proc + "/exe").canonicalFilePath() ==
              QFileInfo(i.value().second).canonicalFilePath(),
          i.key() + " executable mismatch");
    check(metadata(proc).st_uid == uid_t(i.value().first),
          i.key() + " UID mismatch");
    if (i.key() == "web" && !native)
      check(readBytes(proc + "/cmdline", 65536)
                .split('\0')
                .contains((current + "/lib/web/app.py").toUtf8()),
            "Legacy API executable mismatch");
  }
  auto api = object(command(
      "/usr/bin/wget",
      {"-q", "-T", "3", "-O", "-", "http://127.0.0.1:8080/api/health"}, 4000));
  check(api["ok"] == true && api["service"] == "omnigate-web" &&
            api["application_commit"] == identity["commit"],
        "Local API identity mismatch");
  if (native)
    check(api["backend"] == "native-cpp", "Native API unavailable");
  auto path = base + "/hmi/heartbeat.json";
  auto beat = readJson(path);
  // A retained heartbeat from a previous process (including a same-commit OTA)
  // must not end the startup grace period before the new GUI is actually ready.
  check(beat["pid"].toDouble() ==
                integer(readText(base + "/hmi.pid"), "HMI PID", 2, INT_MAX) &&
            beat["boot_id"].toString() ==
                readText("/proc/sys/kernel/random/boot_id"),
        "HMI heartbeat belongs to a previous process or boot");
  struct timespec now{};
  check(::clock_gettime(CLOCK_BOOTTIME, &now) == 0 &&
            beat["monotonic_seconds"].isDouble(),
        "HMI heartbeat clock unavailable");
  const double age = double(now.tv_sec) + now.tv_nsec / 1e9 -
                     beat["monotonic_seconds"].toDouble();
  check(beat["commit"] == identity["commit"] &&
            beat["gui_event_loop"] == true &&
            beat["backend_identity_verified"] == true,
        "HMI identity mismatch");
  check(age >= 0 && age < 15, "HMI heartbeat stale");
  rpc(base + "/desktop/desktop.sock", "desktop.snapshot", {}, 3000);
}
void wifiCheck() {
  for (auto path :
       QStringList{"/data", "/data/dshancontrol", "/data/dshancontrol/network"})
    protectedDirectory(path);
  auto value = metadata("/data/dshancontrol/network/wpa_supplicant.conf");
  check(S_ISREG(value.st_mode) && value.st_uid == 0 &&
            (value.st_mode & 07777) == 0600,
        "Wi-Fi profile must be root-owned regular file mode 0600");
}
void restoreDisplay() {
  storage();
  const QString settings = "/data/dshancontrol/web/hmi.json";
  if (!QFileInfo::exists(settings))
    return;
  const auto directory = metadata("/data/dshancontrol/web");
  const auto file = metadata(settings);
  check(S_ISDIR(directory.st_mode) && directory.st_uid == 400 &&
            !(directory.st_mode & 0022) && S_ISREG(file.st_mode) &&
            file.st_uid == 400 && !(file.st_mode & 0022),
        "Untrusted display settings");
  auto saved = readJson(settings);
  if (!saved.contains("brightness"))
    return;
  const int value = integer(saved["brightness"], "saved brightness", 10, 255);
  // Use the existing root runtime policy and driver mapping, never write sysfs
  // from the unprivileged API. Called after runtime startup, outside OTA hooks.
  rpc(base + "/platform.sock", "display.brightness.set",
      {{"channel", "display0"}, {"value", value}, {"confirmed", true}}, 5000);
}
} // namespace
int main(int argc, char **argv) {
  QCoreApplication application(argc, argv);
  try {
    check(argc == 2 && ::geteuid() == 0, "Root lifecycle operation required");
    QString operation = QString::fromLocal8Bit(argv[1]);
    if (operation == "storage")
      storage();
    else if (operation == "maintenance")
      maintenance();
    else if (operation == "health")
      health();
    else if (operation == "wifi-check")
      wifiCheck();
    else if (operation == "display-restore")
      restoreDisplay();
    else if (operation == "start" || operation == "stop") {
      storage();
      command("/etc/init.d/S99dshancontrol", {operation}, 100000);
      if (operation == "stop")
        for (int attempt = 0;; ++attempt) {
          bool alive = false;
          for (auto role : QStringList{"hmi", "desktop", "web"}) {
            auto path = base + "/" + role + ".pid";
            if (!QFileInfo::exists(path))
              continue;
            auto pid = integer(readText(path), "service PID", 2, INT_MAX);
            alive = alive || QFileInfo::exists("/proc/" + QString::number(pid));
          }
          if (!alive)
            break;
          check(attempt < 50, "Application did not stop");
          QThread::msleep(100);
        }
    } else
      check(false, "Unknown lifecycle operation");
    output({{"ok", true}, {"operation", operation}});
    return 0;
  } catch (const Failure &e) {
    output({{"ok", false}, {"error", e.detail}});
    return 1;
  }
}
