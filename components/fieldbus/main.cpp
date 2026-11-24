#include "../native/common.h"
#include <cstring>
#include <fcntl.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <memory>
#include <net/if.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <termios.h>
using namespace Native;
namespace {
struct FD {
  int value = -1;
  explicit FD(int n) : value(n) {}
  ~FD() {
    if (value >= 0)
      ::close(value);
  }
  operator int() const { return value; }
};
bool readable(int fd, int timeout) {
  pollfd p{fd, POLLIN, 0};
  int n;
  do {
    n = ::poll(&p, 1, timeout);
  } while (n < 0 && errno == EINTR);
  check(n >= 0, "Poll failed", "IOError");
  return n > 0 && (p.revents & POLLIN);
}
QString device(const QJsonObject &manifest, const QString &kind,
               const QJsonValue &requested = {}, const QString &role = {}) {
  if (!role.isEmpty())
    check(manifest["capabilities"].toArray().contains(role),
          "Capability not declared");
  QStringList values;
  const auto channels = manifest["channels"].toObject();
  for (auto i = channels.begin(); i != channels.end(); ++i) {
    const auto v = i.value().toObject();
    if (v["kind"] == kind && (role.isEmpty() || v["role"] == role))
      values << v["device"].toString();
  }
  QString selected = requested.isUndefined() && values.size() == 1
                         ? values.first()
                         : requested.toString();
  check(values.contains(selected) && !selected.isEmpty(),
        "Device is not mapped to this board");
  return selected;
}
int canSocket(const QString &name, int filter = -1) {
  check(name.toUtf8().size() < IFNAMSIZ, "Invalid CAN interface");
  FD fd(::socket(PF_CAN, SOCK_RAW | SOCK_CLOEXEC, CAN_RAW));
  check(fd >= 0, "CAN socket unavailable", "IOError");
  ifreq request{};
  std::strncpy(request.ifr_name, name.toUtf8().constData(), IFNAMSIZ - 1);
  check(::ioctl(fd, SIOCGIFINDEX, &request) == 0, "CAN interface unavailable",
        "IOError");
  if (filter >= 0) {
    can_filter f{canid_t(filter), CAN_SFF_MASK | CAN_EFF_FLAG | CAN_RTR_FLAG};
    check(::setsockopt(fd, SOL_CAN_RAW, CAN_RAW_FILTER, &f, sizeof(f)) == 0,
          "CAN filter failed");
  }
  sockaddr_can address{};
  address.can_family = AF_CAN;
  address.can_ifindex = request.ifr_ifindex;
  check(::bind(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) ==
            0,
        "CAN bind failed", "IOError");
  const auto result = fd.value;
  fd.value = -1;
  return result;
}
void sendCan(int fd, canid_t id, const QByteArray &data) {
  check(data.size() <= 8, "Classic CAN payload too large");
  can_frame frame{};
  frame.can_id = id;
  frame.can_dlc = data.size();
  std::memcpy(frame.data, data.constData(), data.size());
  check(::write(fd, &frame, sizeof(frame)) == sizeof(frame),
        "CAN send failed; outcome unknown", "IOError");
}
can_frame receiveCan(int fd, int timeout) {
  check(readable(fd, timeout), "CAN response timed out", "TimeoutError");
  can_frame frame{};
  check(::read(fd, &frame, sizeof(frame)) == sizeof(frame) &&
            frame.can_dlc <= 8,
        "Invalid CAN frame", "InvalidResponseError");
  return frame;
}
QByteArray sdoResponse(int fd, int node) {
  auto f = receiveCan(fd, 1000);
  check(f.can_id == canid_t(0x580 + node) && f.can_dlc == 8,
        "Invalid SDO response", "InvalidResponseError");
  if (f.data[0] == 0x80) {
    QByteArray code(reinterpret_cast<char *>(f.data + 4), 4);
    throw Failure("SdoAbortedError", "fieldbus",
                  "SDO aborted (little endian): " + code.toHex());
  }
  return QByteArray(reinterpret_cast<char *>(f.data), 8);
}
QByteArray transferSdo(int fd, int node, int index, int sub,
                       const QByteArray *payload = nullptr) {
  const bool writing = payload != nullptr;
  QElapsedTimer budget;
  budget.start();
  QByteArray request(8, 0);
  request[0] = writing ? (payload->size() <= 4
                              ? char(0x23 | ((4 - payload->size()) << 2))
                              : char(0x21))
                       : char(0x40);
  request[1] = char(index);
  request[2] = char(index >> 8);
  request[3] = char(sub);
  if (writing)
    for (int i = 0; i < 4; ++i)
      request[4 + i] = payload->size() <= 4
                           ? (i < payload->size() ? payload->at(i) : char(0))
                           : char((quint32(payload->size()) >> (8 * i)) & 255);
  sendCan(fd, 0x600 + node, request);
  auto response = sdoResponse(fd, node);
  check(response.mid(1, 3) == request.mid(1, 3), "SDO object mismatch",
        "InvalidResponseError");
  if (writing) {
    check(quint8(response[0]) == 0x60, "SDO write not acknowledged",
          "InvalidResponseError");
    if (payload->size() > 4)
      for (int offset = 0, toggle = 0; offset < payload->size(); toggle ^= 1) {
        check(budget.elapsed() < 4000,
              "SDO download budget exceeded; outcome unknown", "TimeoutError");
        auto chunk = payload->mid(offset, 7);
        bool last = offset + chunk.size() == payload->size();
        request.fill(0);
        request[0] =
            char((toggle << 4) | ((7 - chunk.size()) << 1) | (last ? 1 : 0));
        for (int i = 0; i < chunk.size(); ++i)
          request[1 + i] = chunk[i];
        sendCan(fd, 0x600 + node, request);
        response = sdoResponse(fd, node);
        check(quint8(response[0]) == (0x20 | (toggle << 4)),
              "Invalid SDO download acknowledgement", "InvalidResponseError");
        offset += chunk.size();
      }
    return {};
  }
  const int control = quint8(response[0]);
  check((control & 0xe0) == 0x40, "Invalid SDO upload response",
        "InvalidResponseError");
  QByteArray data;
  if (control & 2)
    return response.mid(4, (control & 1) ? 4 - ((control >> 2) & 3) : 4);
  quint32 expected = 0;
  if (control & 1)
    for (int i = 0; i < 4; ++i)
      expected |= quint32(quint8(response[4 + i])) << (8 * i);
  check(expected <= 4096, "SDO upload too large");
  int toggle = 0;
  for (;;) {
    check(budget.elapsed() < 4000 && data.size() <= 4096,
          "SDO upload budget exceeded", "TimeoutError");
    request.fill(0);
    request[0] = char(0x60 | (toggle << 4));
    sendCan(fd, 0x600 + node, request);
    response = sdoResponse(fd, node);
    int c = quint8(response[0]);
    check((c & 0xe0) == 0 && ((c >> 4) & 1) == toggle,
          "Invalid SDO segment toggle", "InvalidResponseError");
    int unused = (c >> 1) & 7;
    check((c & 1) || unused == 0, "Invalid SDO segment size",
          "InvalidResponseError");
    data += response.mid(1, 7 - unused);
    check(data.size() <= 4096, "SDO upload too large");
    if (c & 1)
      break;
    toggle ^= 1;
  }
  check(!(control & 1) || quint32(data.size()) == expected, "SDO size mismatch",
        "InvalidResponseError");
  return data;
}
QJsonObject diagnostic(int fd, int node, int index, int sub = 0) {
  auto data = transferSdo(fd, node, index, sub);
  QJsonValue number;
  if (data.size() <= 6) {
    quint64 n = 0;
    for (int i = 0; i < data.size(); ++i)
      n |= quint64(quint8(data[i])) << (i * 8);
    number = double(n);
  }
  return {{"hex", QString::fromLatin1(data.toHex(' '))},
          {"length", data.size()},
          {"unsigned", number}};
}
quint16 crc(const QByteArray &data) {
  quint16 v = 0xffff;
  for (auto c : data) {
    v ^= quint8(c);
    for (int i = 0; i < 8; ++i)
      v = (v >> 1) ^ ((v & 1) ? 0xa001 : 0);
  }
  return v;
}
void word(QByteArray &data, int v) {
  data += char((v >> 8) & 255);
  data += char(v & 255);
}
speed_t baud(int value) {
  const QMap<int, speed_t> values{
      {300, B300},         {600, B600},         {1200, B1200},
      {2400, B2400},       {4800, B4800},       {9600, B9600},
      {19200, B19200},     {38400, B38400},     {57600, B57600},
      {115200, B115200},   {230400, B230400},   {460800, B460800},
      {500000, B500000},   {576000, B576000},   {921600, B921600},
      {1000000, B1000000}, {1500000, B1500000}, {2000000, B2000000},
      {2500000, B2500000}, {3000000, B3000000}, {3500000, B3500000},
      {4000000, B4000000}};
  check(values.contains(value), "Unsupported serial baudrate");
  return values[value];
}
QJsonObject modbus(const QJsonObject &manifest, const QString &state,
                   const QString &action, const QJsonObject &p) {
  if (action == "rs485_config") {
    fields(p, {"port", "baudrate", "unit_id", "poll_ms", "parity", "stopbits"},
           {"port", "baudrate", "unit_id", "poll_ms"});
    auto rate = int(integer(p["baudrate"], "baudrate", 300, 4000000));
    baud(rate);
    auto parity = p.value("parity").toString("N");
    check(QStringList{"N", "E", "O"}.contains(parity), "Invalid parity");
    QJsonObject config{
        {"port", device(manifest, "serial", p["port"], "modbus-rtu")},
        {"baudrate", rate},
        {"unit_id", int(integer(p["unit_id"], "unit_id", 1, 247))},
        {"poll_ms", int(integer(p["poll_ms"], "poll_ms", 50, 3600000))},
        {"parity", parity},
        {"stopbits",
         int(integer(p.value("stopbits").isUndefined() ? QJsonValue(1)
                                                       : p["stopbits"],
                     "stopbits", 1, 2))}};
    check(QDir::isAbsolutePath(state) && !QFileInfo(state).isSymLink() &&
              QDir().mkpath(state),
          "Invalid private state directory");
    check(QFile::setPermissions(state, QFile::ReadOwner | QFile::WriteOwner |
                                           QFile::ExeOwner),
          "Cannot protect state directory");
    durableJson(state + "/rs485.json", config);
    config.insert("configured", true);
    config.insert("polling", false);
    config.insert("device_acknowledged", false);
    return config;
  }
  check(action == "rs485_read" || action == "rs485_write",
        "Unknown Modbus operation");
  fields(p, {"port", "unit_id", "address", "count", "function", "value"},
         {"address"});
  const auto c = readJson(state + "/rs485.json");
  auto port = device(manifest, "serial",
                     p.value("port").isUndefined() ? c["port"] : p["port"],
                     "modbus-rtu");
  check(port == c["port"].toString(), "Serial configuration changed");
  int unit = integer(p.contains("unit_id") ? p["unit_id"] : c["unit_id"],
                     "unit_id", 1, 247),
      address = integer(p["address"], "address", 0, 65535);
  int count = integer(p.contains("count") ? p["count"] : QJsonValue(1), "count",
                      1, 125);
  check(address + count <= 65536, "Register range overflows");
  bool writing = action == "rs485_write";
  int function = integer(p.contains("function") ? p["function"]
                                                : QJsonValue(writing ? 6 : 3),
                         "function", 1, 16);
  check(writing ? function == 6 : (function == 3 || function == 4),
        "Unsupported Modbus function");
  int value = writing ? integer(p["value"], "value", 0, 65535) : count;
  auto speed = baud(integer(c["baudrate"], "baudrate", 300, 4000000));
  auto parity = c["parity"].toString();
  check(QStringList{"N", "E", "O"}.contains(parity), "Invalid saved parity");
  FD fd(::open(port.toUtf8().constData(),
               O_RDWR | O_NOCTTY | O_CLOEXEC | O_NONBLOCK));
  check(fd >= 0, "Serial open failed", "IOError");
  check(::ioctl(fd, TIOCEXCL) == 0, "Serial port busy", "IOError");
  struct Term {
    int fd;
    termios old{};
    bool valid = false;
    ~Term() {
      if (valid)
        ::tcsetattr(fd, TCSANOW, &old);
      ::ioctl(fd, TIOCNXCL);
    }
  } term{fd};
  check(::tcgetattr(fd, &term.old) == 0, "Cannot read serial configuration",
        "IOError");
  term.valid = true;
  auto options = term.old;
  ::cfmakeraw(&options);
  options.c_cflag =
      (options.c_cflag & ~(CSIZE | PARENB | PARODD | CSTOPB | CRTSCTS)) | CS8 |
      CLOCAL | CREAD;
  if (parity != "N")
    options.c_cflag |= PARENB;
  if (parity == "O")
    options.c_cflag |= PARODD;
  if (c["stopbits"] == 2)
    options.c_cflag |= CSTOPB;
  ::cfsetispeed(&options, speed);
  ::cfsetospeed(&options, speed);
  check(::tcsetattr(fd, TCSANOW, &options) == 0 &&
            ::tcflush(fd, TCIOFLUSH) == 0,
        "Cannot configure serial port", "IOError");
  QByteArray request;
  request += char(unit);
  request += char(function);
  word(request, address);
  word(request, value);
  auto checksum = crc(request);
  request += char(checksum & 255);
  request += char(checksum >> 8);
  check(::write(fd, request.constData(), request.size()) == request.size(),
        "Incomplete RTU request; outcome unknown", "IOError");
  QByteArray response;
  QElapsedTimer timer;
  timer.start();
  int expected = writing ? 8 : 5 + count * 2;
  while (response.size() < expected && timer.elapsed() < 1000) {
    if (!readable(fd, qMax(1, 1000 - int(timer.elapsed()))))
      break;
    char buffer[256];
    auto n = ::read(fd, buffer, sizeof(buffer));
    if (n > 0)
      response.append(buffer, n);
    else
      check(n < 0 && (errno == EAGAIN || errno == EINTR), "Serial read failed",
            "IOError");
    if (response.size() >= 2 && (quint8(response[1]) & 0x80))
      expected = 5;
  }
  check(!response.isEmpty(), "No Modbus response", "NoResponseError");
  check(response.size() == expected, "Incomplete or excess RTU response",
        "InvalidResponseError");
  auto actual = quint8(response[response.size() - 2]) |
                (quint16(quint8(response.back())) << 8);
  check(crc(response.left(response.size() - 2)) == actual,
        "Modbus checksum rejected", "InvalidResponseError");
  check(quint8(response[0]) == unit, "Modbus unit mismatch",
        "InvalidResponseError");
  check(!(quint8(response[1]) & 0x80),
        "Modbus exception: " + QString::number(quint8(response[2])),
        "SlaveReportedException");
  check(quint8(response[1]) == function, "Modbus function mismatch",
        "InvalidResponseError");
  QJsonArray values;
  if (writing) {
    check(response == request, "Modbus write echo mismatch",
          "InvalidResponseError");
    values.append(value);
  } else {
    check(quint8(response[2]) == count * 2, "Modbus byte count mismatch",
          "InvalidResponseError");
    for (int i = 0; i < count; ++i)
      values.append((quint8(response[3 + 2 * i]) << 8) |
                    quint8(response[4 + 2 * i]));
  }
  return {{"port", port},
          {"unit_id", unit},
          {"address", address},
          {"values", values},
          {"device_acknowledged", true},
          {"timestamp", QDateTime::currentMSecsSinceEpoch() / 1000.0}};
}
quint32 unsignedSdo(int fd, int node, int index, int sub) {
  const auto data = transferSdo(fd, node, index, sub);
  check(!data.isEmpty() && data.size() <= 4, "Expected numeric PDO object");
  quint32 result = 0;
  for (int i = 0; i < data.size(); ++i)
    result |= quint32(quint8(data[i])) << (8 * i);
  return result;
}
QJsonObject readPdo(int fd, int node, int number, bool transmit) {
  const auto communication = (transmit ? 0x1800 : 0x1400) + number - 1,
             mapping = (transmit ? 0x1a00 : 0x1600) + number - 1;
  auto cob = unsignedSdo(fd, node, communication, 1),
       type = unsignedSdo(fd, node, communication, 2),
       count = unsignedSdo(fd, node, mapping, 0);
  check(type <= 255 && count <= 64, "Invalid PDO configuration");
  QJsonArray variables;
  int length = 0;
  for (quint32 sub = 1; sub <= count; ++sub) {
    auto entry = unsignedSdo(fd, node, mapping, sub);
    int bits = entry & 255;
    check(bits > 0 && bits <= 64 && length + bits <= 64,
          "PDO mapping exceeds classic CAN");
    variables.append(QJsonObject{{"index", int(entry >> 16)},
                                 {"subindex", int((entry >> 8) & 255)},
                                 {"length", bits}});
    length += bits;
  }
  const bool extended = cob & (1U << 29);
  auto id = cob & 0x1fffffff;
  check(extended || id <= 0x7ff, "Invalid standard PDO COB-ID");
  return {{"number", number},
          {"cob_id", double(id)},
          {"enabled", !(cob & (1U << 31))},
          {"extended", extended},
          {"transmission_type", double(type)},
          {"variables", variables},
          {"bits", length}};
}
QJsonObject execute(const QJsonObject &manifest, const QString &state,
                    const QString &action, const QJsonObject &p) {
  if (action == "canopen_pdo_read" || action == "canopen_pdo_transmit") {
    check(manifest["capabilities"].toArray().contains("canopen"),
          "CANopen unavailable");
    fields(
        p,
        action == "canopen_pdo_read"
            ? QSet<QString>{"interface", "node_id", "tpdo", "rpdo"}
            : QSet<QString>{"interface", "node_id", "pdo", "mapping", "data"},
        {"node_id"});
    auto name = device(manifest, "can", p["interface"]);
    int node = integer(p["node_id"], "node_id", 1, 127);
    if (action == "canopen_pdo_transmit") {
      int number = integer(p["pdo"], "pdo", 1, 512);
      check(p["mapping"].isObject() && p["data"].isString() &&
                QRegularExpression("^(?:[0-9a-fA-F]{2}){1,8}$")
                    .match(p["data"].toString())
                    .hasMatch(),
            "Invalid RPDO frame");
      FD fd(canSocket(name, 0x580 + node));
      auto mapping = readPdo(fd, node, number, false);
      auto bytes = QByteArray::fromHex(p["data"].toString().toLatin1());
      check(mapping == p["mapping"].toObject(),
            "PDO mapping changed; refresh before transmitting");
      check(mapping["enabled"] == true && mapping["bits"].toInt() > 0 &&
                bytes.size() == (mapping["bits"].toInt() + 7) / 8,
            "RPDO disabled or frame length mismatch");
      sendCan(fd,
              quint32(mapping["cob_id"].toDouble()) |
                  (mapping["extended"].toBool() ? CAN_EFF_FLAG : 0),
              bytes);
      return {{"ok", true},
              {"node_id", node},
              {"pdo", number},
              {"queued", true},
              {"device_acknowledged", false}};
    }
    QMap<QString, QList<int>> selections;
    int total = 0;
    for (auto direction : QStringList{"tpdo", "rpdo"}) {
      check(p[direction].isArray(), "Explicit PDO selection required");
      QSet<int> seen;
      for (auto n : p[direction].toArray()) {
        int number = integer(n, "pdo", 1, 512);
        check(!seen.contains(number), "Duplicate PDO selection");
        seen.insert(number);
        selections[direction].append(number);
        ++total;
      }
    }
    check(total >= 1 && total <= 32, "PDO read requires 1..32 mappings");
    FD fd(canSocket(name, 0x580 + node));
    QJsonObject result{{"ok", true}};
    for (auto direction : QStringList{"tpdo", "rpdo"}) {
      QJsonArray mappings;
      for (int n : selections[direction])
        mappings.append(readPdo(fd, node, n, direction == "tpdo"));
      result[direction] = mappings;
    }
    return result;
  }
  if (action == "service") {
    fields(p, {"service", "operation"}, {"service", "operation"});
    const QMap<QString, QString> scripts{{"ntp", "/etc/init.d/S49ntp"},
                                         {"bluetooth", "/usr/bin/bt-speaker"}};
    const auto name = p["service"].toString(),
               operation = p["operation"].toString();
    check(scripts.contains(name) &&
              QStringList{"start", "stop", "restart"}.contains(operation),
          "Service operation unavailable");
    check(name != "bluetooth" ||
              manifest["capabilities"].toArray().contains("bluetooth"),
          "Bluetooth unavailable");
    check(QFileInfo(scripts[name]).isExecutable(),
          "Service executable unavailable");
    auto result = command(scripts[name], {operation}, 10000);
    check(
        !QRegularExpression("\\bFAIL\\s*$", QRegularExpression::MultilineOption)
             .match(QString::fromUtf8(result))
             .hasMatch(),
        "Service reported failure");
    return {{"returncode", 0}, {"output", QString::fromUtf8(result)}};
  }
  if (action == "canopen_scan") {
    fields(p, {"interface", "limit", "wait"});
    check(manifest["capabilities"].toArray().contains("canopen"),
          "CANopen unavailable");
    auto name = device(manifest, "can", p["interface"]);
    int limit = integer(p.contains("limit") ? p["limit"] : QJsonValue(127),
                        "limit", 1, 127);
    double wait = p.contains("wait") ? p["wait"].toDouble(-1) : .8;
    check(std::isfinite(wait) && wait >= .1 && wait <= 5, "Invalid scan wait");
    FD fd(canSocket(name));
    QByteArray request(8, 0);
    request[0] = 0x40;
    request[2] = 0x10;
    for (int node = 1; node <= limit; ++node)
      sendCan(fd, 0x600 + node, request);
    QSet<int> found;
    QElapsedTimer budget;
    budget.start();
    while (budget.elapsed() < int(wait * 1000)) {
      if (!readable(fd, qMax(1, int(wait * 1000) - int(budget.elapsed()))))
        break;
      auto frame = receiveCan(fd, 1);
      if (frame.can_id >= 0x581 && frame.can_id <= canid_t(0x580 + limit) &&
          frame.can_dlc == 8 && frame.data[1] == 0 && frame.data[2] == 0x10 &&
          frame.data[3] == 0)
        found.insert(frame.can_id - 0x580);
      else if (frame.can_id >= 0x701 &&
               frame.can_id <= canid_t(0x700 + limit) && frame.can_dlc == 1)
        found.insert(frame.can_id - 0x700);
    }
    auto nodes = found.values();
    std::sort(nodes.begin(), nodes.end());
    QJsonArray result;
    for (int node : nodes)
      result.append(node);
    return {{"ok", true}, {"interface", name}, {"nodes", result}};
  }
  if (action == "canopen_node") {
    fields(p, {"interface", "node_id", "heartbeat_wait"}, {"node_id"});
    check(manifest["capabilities"].toArray().contains("canopen"),
          "CANopen unavailable");
    auto name = device(manifest, "can", p["interface"]);
    int node = integer(p["node_id"], "node_id", 1, 127);
    double wait =
        p.contains("heartbeat_wait") ? p["heartbeat_wait"].toDouble(-1) : .5;
    check(std::isfinite(wait) && wait >= 0 && wait <= 3,
          "Invalid heartbeat wait");
    FD fd(canSocket(name, 0x580 + node)),
        heartbeat(canSocket(name, 0x700 + node));
    QJsonObject result{{"ok", true},
                       {"interface", name},
                       {"node_id", node},
                       {"device_type", diagnostic(fd, node, 0x1000)}};
    auto get = [&](int index, int sub = 0) -> QJsonValue {
      try {
        return diagnostic(fd, node, index, sub);
      } catch (const Failure &) {
        return QJsonValue::Null;
      }
    };
    result["error_register"] = get(0x1001);
    result["heartbeat_producer_ms"] = get(0x1017);
    result["identity"] = QJsonObject{{"vendor_id", get(0x1018, 1)},
                                     {"product_code", get(0x1018, 2)},
                                     {"revision", get(0x1018, 3)},
                                     {"serial", get(0x1018, 4)}};
    result["nmt_state"] = QJsonValue::Null;
    result["heartbeat_error"] = "";
    if (wait > 0) {
      if (readable(heartbeat, int(wait * 1000))) {
        auto f = receiveCan(heartbeat, 1);
        const QMap<int, QString> states{{0, "INITIALISING"},
                                        {4, "STOPPED"},
                                        {5, "OPERATIONAL"},
                                        {127, "PRE-OPERATIONAL"}};
        check(f.can_dlc == 1 && states.contains(f.data[0]),
              "Invalid heartbeat");
        result["nmt_state"] = states[f.data[0]];
      } else
        result["heartbeat_error"] = "Heartbeat timed out";
    }
    return result;
  }
  if (action == "can_apply") {
    fields(p,
           {"interface", "bitrate", "restart_ms", "fd", "data_bitrate",
            "loopback"},
           {"bitrate"});
    auto name = device(manifest, "can", p["interface"]);
    int rate = integer(p["bitrate"], "bitrate", 10000, 1000000),
        restart = integer(p.contains("restart_ms") ? p["restart_ms"]
                                                   : QJsonValue(100),
                          "restart_ms", 0, 60000);
    auto fd = boolean(p, "fd"), loop = boolean(p, "loopback");
    check(!fd || manifest["capabilities"].toArray().contains("can-fd"),
          "CAN-FD unsupported by this board");
    QStringList extra;
    if (fd)
      extra =
          QStringList{"dbitrate",
                      QString::number(integer(p["data_bitrate"], "data_bitrate",
                                              rate, 8000000)),
                      "fd", "on"};
    command("/sbin/ip", {"link", "set", "dev", name, "down"});
    command("/sbin/ip", QStringList{"link", "set", "dev", name, "type", "can",
                                    "bitrate", QString::number(rate),
                                    "restart-ms", QString::number(restart),
                                    "loopback", loop ? "on" : "off"} +
                            extra);
    command("/sbin/ip", {"link", "set", "dev", name, "up"});
    return {
        {"interface", name},
        {"bitrate", rate},
        {"fd", fd},
        {"loopback", loop},
        {"driver", strictJson(command("/sbin/ip", {"-details", "-json", "link",
                                                   "show", "dev", name}))}};
  }
  if (action == "can_send" || action == "can_receive") {
    fields(p, {"interface", "id", "data", "extended", "timeout_ms"},
           action == "can_send" ? QSet<QString>{"id", "data"}
                                : QSet<QString>{});
    auto name = device(manifest, "can", p["interface"]);
    auto timeout =
        integer(p.contains("timeout_ms") ? p["timeout_ms"] : QJsonValue(1000),
                "timeout_ms", 10, 3000);
    if (action == "can_send") {
      auto extended = boolean(p, "extended");
      auto id = integer(p["id"], "id", 0, extended ? 0x1fffffff : 0x7ff);
      check(p["data"].isString() &&
                QRegularExpression("^(?:[0-9a-fA-F]{2}){0,8}$")
                    .match(p["data"].toString())
                    .hasMatch(),
            "Invalid classic CAN data");
      auto data = QByteArray::fromHex(p["data"].toString().toLatin1());
      FD socket(canSocket(name));
      sendCan(socket, id | (extended ? CAN_EFF_FLAG : 0), data);
      return {{"interface", name},
              {"queued", true},
              {"device_acknowledged", false},
              {"bytes", data.size()}};
    }
    FD socket(canSocket(name));
    auto frame = receiveCan(socket, timeout);
    return {{"interface", name},
            {"id", int(frame.can_id & CAN_EFF_MASK)},
            {"flags", double(frame.can_id & ~CAN_EFF_MASK)},
            {"data",
             QString::fromLatin1(
                 QByteArray(reinterpret_cast<char *>(frame.data), frame.can_dlc)
                     .toHex())},
            {"timestamp", QDateTime::currentMSecsSinceEpoch() / 1000.0}};
  }
  if (action.startsWith("canopen_")) {
    check(manifest["capabilities"].toArray().contains("canopen"),
          "CANopen unavailable");
    check(QStringList{"canopen_nmt", "canopen_sdo_read", "canopen_sdo_write"}
              .contains(action),
          "Unsupported CANopen operation");
    fields(p,
           action == "canopen_nmt"
               ? QSet<QString>{"interface", "node_id", "state"}
               : QSet<QString>{"interface", "node_id", "index", "subindex",
                               "size", "value", "data"},
           {"node_id"});
    auto name = device(manifest, "can", p["interface"]);
    int node =
        integer(p["node_id"], "node_id", action == "canopen_nmt" ? 0 : 1, 127);
    if (action == "canopen_nmt") {
      const QMap<QString, int> states{{"OPERATIONAL", 1},
                                      {"STOPPED", 2},
                                      {"PRE-OPERATIONAL", 128},
                                      {"RESET", 129},
                                      {"RESET COMMUNICATION", 130}};
      auto s = p["state"].toString();
      check(states.contains(s), "Invalid NMT state");
      FD fd(canSocket(name));
      QByteArray bytes;
      bytes += char(states[s]);
      bytes += char(node);
      sendCan(fd, 0, bytes);
      return {{"node_id", node},
              {"state", s},
              {"queued", true},
              {"device_acknowledged", false}};
    }
    int index = integer(p["index"], "index", 0, 65535),
        sub = integer(p.contains("subindex") ? p["subindex"] : QJsonValue(0),
                      "subindex", 0, 255);
    bool writing = action == "canopen_sdo_write";
    QByteArray payload;
    if (writing) {
      if (p.contains("data")) {
        check(!p.contains("value") && !p.contains("size") &&
                  p["data"].isString() && p["data"].toString().size() <= 8192 &&
                  QRegularExpression("^(?:[0-9a-fA-F]{2})+$")
                      .match(p["data"].toString())
                      .hasMatch(),
              "Invalid SDO byte payload");
        payload = QByteArray::fromHex(p["data"].toString().toLatin1());
      } else {
        int size = integer(p.contains("size") ? p["size"] : QJsonValue(4),
                           "size", 1, 4);
        qint64 value =
            integer(p["value"], "value", -(qint64(1) << (size * 8 - 1)),
                    (qint64(1) << (size * 8)) - 1);
        for (int i = 0; i < size; ++i)
          payload += char((quint64(value) >> (8 * i)) & 255);
      }
    }
    FD fd(canSocket(name, 0x580 + node));
    auto data = transferSdo(fd, node, index, sub, writing ? &payload : nullptr);
    QJsonObject result{{"node_id", node},
                       {"index", index},
                       {"subindex", sub},
                       {"device_acknowledged", true}};
    if (writing) {
      result["written"] = payload.size();
      return result;
    }
    result.insert("data", QString::fromLatin1(data.toHex()));
    return result;
  }
  if (action.startsWith("rs485_"))
    return modbus(manifest, state, action, p);
  if (action.startsWith("ethercat_")) {
    QSet<QString> allowed{"interface"};
    if (action == "ethercat_sdo_read" || action == "ethercat_sdo_write")
      allowed.unite({"slave", "index", "subindex", "value"});
    else if (action == "ethercat_cycle")
      allowed.unite({"count", "period_us"});
    else
      check(action == "ethercat_scan", "Unknown EtherCAT operation");
    fields(p, allowed);
    QStringList args{device(manifest, "ethernet", p["interface"], "ethercat")};
    if (action == "ethercat_scan")
      args << "scan";
    else if (action == "ethercat_cycle") {
      auto count = integer(p.contains("count") ? p["count"] : QJsonValue(100),
                           "count", 1, 10000),
           period = integer(p.contains("period_us") ? p["period_us"]
                                                    : QJsonValue(1000),
                            "period_us", 100, 1000000);
      check(count * period <= 5000000, "Cycle exceeds five-second budget");
      args << "cycle" << QString::number(count) << QString::number(period);
    } else {
      args << (action == "ethercat_sdo_read" ? "sdo-read" : "sdo-write")
           << QString::number(integer(p["slave"], "slave", 1, 65535))
           << "0x" + QString::number(integer(p["index"], "index", 0, 65535), 16)
           << QString::number(integer(p.contains("subindex") ? p["subindex"]
                                                             : QJsonValue(0),
                                      "subindex", 0, 255));
      if (action == "ethercat_sdo_write") {
        check(p["value"].isString() &&
                  QRegularExpression(
                      "^[0-9A-Fa-f]{2}(?:\\s+[0-9A-Fa-f]{2}){0,31}$")
                      .match(p["value"].toString())
                      .hasMatch(),
              "Expected 1..32 hexadecimal bytes");
        args << p["value"].toString();
      }
    }
    return object(command("/usr/bin/omnigate-ethercat", args, 10000));
  }
  throw Failure("ValueError", "fieldbus", "Unsupported fieldbus operation");
}
} // namespace
int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  try {
    check(argc == 3, "Usage: fieldbus-worker MANIFEST STATE");
    QFile input;
    input.open(stdin, QIODevice::ReadOnly);
    auto bytes = input.read(65537);
    check(bytes.size() <= 65536, "Request too large");
    auto request = object(bytes);
    fields(request, {"action", "params", "confirmed"},
           {"action", "params", "confirmed"});
    check(request["confirmed"] == true && request["action"].isString() &&
              request["params"].isObject(),
          "Confirmed operation required");
    output({{"ok", true},
            {"result",
             execute(readJson(argv[1]), argv[2], request["action"].toString(),
                     request["params"].toObject())}});
    return 0;
  } catch (const Failure &e) {
    output({{"ok", false}, {"code", e.code}, {"error", e.detail}});
    return 1;
  } catch (const std::exception &) {
    output({{"ok", false},
            {"code", "InternalError"},
            {"error", "Native executor failed"}});
    return 1;
  }
}
