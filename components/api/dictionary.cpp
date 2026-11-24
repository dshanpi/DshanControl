#include "api.h"
#include <QSaveFile>
#include <cstring>
using namespace Native;
namespace {
using Dictionary = QMap<QString, QMap<QString, QString>>;
Dictionary parseDictionary(const QByteArray &bytes) {
  check(!bytes.isEmpty() && bytes.size() <= 512 * 1024 && !bytes.contains('\0'),
        "EDS file exceeds limits");
  auto text = QString::fromUtf8(bytes);
  if (text.toUtf8() != bytes)
    text = QString::fromLatin1(bytes);
  if (text.startsWith(QChar(0xfeff)))
    text.remove(0, 1);
  Dictionary result;
  QString section;
  for (auto line : text.split('\n')) {
    line = line.trimmed();
    if (line.isEmpty() || line.startsWith(';') || line.startsWith('#'))
      continue;
    if (line.startsWith('[')) {
      check(line.endsWith(']') && line.size() <= 130, "Invalid EDS section");
      section = line.mid(1, line.size() - 2).toLower();
      check(!section.isEmpty() && !result.contains(section) &&
                result.size() < 4096,
            "Duplicate or excessive EDS sections");
      result[section] = {};
      continue;
    }
    int split = line.indexOf('=');
    check(!section.isEmpty() && split > 0 && line.size() <= 4096,
          "Invalid EDS assignment");
    auto key = line.left(split).trimmed().toLower(),
         value = line.mid(split + 1).trimmed();
    check(!key.isEmpty() && !result[section].contains(key),
          "Duplicate EDS member");
    result[section][key] = value;
  }
  check(result.contains("fileinfo") && result.contains("deviceinfo"),
        "EDS FileInfo and DeviceInfo required");
  bool objects = false;
  for (auto i = result.begin(); i != result.end(); ++i)
    if (QRegularExpression("^[0-9a-f]{4}(?:sub[0-9a-f]{1,2})?$")
            .match(i.key())
            .hasMatch()) {
      objects = true;
      check(i.value().contains("parametername") &&
                !i.value()["parametername"].isEmpty(),
            "EDS object name required");
      if (i.value().contains("datatype"))
        integer(i.value()["datatype"], "EDS data type", 1, 65535);
    }
  check(objects, "EDS object dictionary is empty");
  return result;
}
QMap<QString, QString> objectDefinition(const Dictionary &dictionary, int index,
                                        int sub) {
  auto section = QString("%1").arg(index, 4, 16, QChar('0'));
  auto key = section + "sub" + QString::number(sub, 16);
  if (dictionary.contains(key))
    return dictionary[key];
  key = section + "sub" + QString("%1").arg(sub, 2, 16, QChar('0'));
  if (dictionary.contains(key))
    return dictionary[key];
  check(sub == 0 && dictionary.contains(section),
        "PDO object not present in bound EDS");
  return dictionary[section];
}
QByteArray encodeValue(const QJsonValue &value, int type, int bits) {
  check(bits >= 1 && bits <= 64, "Invalid mapped bit length");
  quint64 raw = 0;
  const QMap<int, int> signedTypes{{2, 8},     {3, 16},    {4, 32},
                                   {0x10, 24}, {0x12, 40}, {0x13, 48},
                                   {0x14, 56}, {0x15, 64}};
  const QMap<int, int> unsignedTypes{{5, 8},     {6, 16},    {7, 32},
                                     {0x16, 24}, {0x18, 40}, {0x19, 48},
                                     {0x1a, 56}, {0x1b, 64}};
  if (type == 1) {
    check(value.isBool() && (bits == 1 || bits == 8),
          "Boolean PDO value required");
    raw = value.toBool();
  } else if (type == 8 || type == 0x11) {
    check(value.isDouble() && std::isfinite(value.toDouble()) &&
              bits == (type == 8 ? 32 : 64),
          "Finite PDO real required");
    if (type == 8) {
      float n = value.toDouble();
      check(std::isfinite(n), "REAL32 overflow");
      quint32 u;
      std::memcpy(&u, &n, 4);
      raw = u;
    } else {
      double n = value.toDouble();
      std::memcpy(&raw, &n, 8);
    }
  } else {
    const bool signedType = signedTypes.contains(type);
    check(signedType || unsignedTypes.contains(type),
          "PDO variable type is not supported");
    check(bits <= (signedType ? signedTypes[type] : unsignedTypes[type]),
          "PDO map exceeds EDS type width");
    QString text =
        value.isString()
            ? value.toString()
            : QString::number(integer(value, "PDO value", -9007199254740991LL,
                                      9007199254740991LL));
    check(QRegularExpression("^-?(?:0[xX][0-9a-fA-F]+|[0-9]+)$")
              .match(text)
              .hasMatch(),
          "Invalid PDO integer");
    int base = text.contains('x', Qt::CaseInsensitive) ? 16 : 10;
    bool ok;
    if (signedType) {
      qint64 v = text.toLongLong(&ok, base);
      check(ok && (bits == 64 || (v >= -(qint64(1) << (bits - 1)) &&
                                  v < (qint64(1) << (bits - 1)))),
            "Signed PDO value out of range");
      raw = quint64(v);
    } else {
      raw = text.toULongLong(&ok, base);
      check(ok && !text.startsWith('-') &&
                (bits == 64 || raw < (quint64(1) << bits)),
            "Unsigned PDO value out of range");
    }
  }
  QByteArray bytes(8, 0);
  for (int i = 0; i < 8; ++i)
    bytes[i] = char((raw >> (8 * i)) & 255);
  return bytes;
}
} // namespace
Response Api::dictionaryRoute(const Request &r) {
  check(r.method == "POST", "POST required");
  const auto directory = env("OMNIGATE_EDS_DIR", "/data/dshancontrol/web/eds");
  if (r.url.path() == "/api/canopen/eds") {
    auto type = QString::fromLatin1(r.headers.value("content-type"));
    auto match =
        QRegularExpression(
            "^multipart/form-data;\\s*boundary=(?:\"([^\"]+)\"|([^;\\s]+))$")
            .match(type);
    check(match.hasMatch(), "Multipart EDS upload required");
    auto boundary =
        (match.captured(1).isEmpty() ? match.captured(2) : match.captured(1))
            .toLatin1();
    check(boundary.size() <= 70 &&
              QRegularExpression("^[A-Za-z0-9'()+_,./:=?-]+$")
                  .match(QString::fromLatin1(boundary))
                  .hasMatch(),
          "Invalid multipart boundary");
    const auto delimiter = "--" + boundary;
    check(r.body.startsWith(delimiter + "\r\n") &&
              r.body.endsWith(delimiter + "--\r\n"),
          "Incomplete multipart upload");
    QMap<QString, QByteArray> values;
    QString filename;
    int cursor = delimiter.size() + 2;
    while (cursor < r.body.size()) {
      int end = r.body.indexOf("\r\n\r\n", cursor);
      check(end >= cursor && end - cursor <= 4096,
            "Multipart headers exceed limit");
      auto headers = r.body.mid(cursor, end - cursor).split('\n');
      QString disposition;
      for (auto line : headers) {
        line = line.trimmed();
        int colon = line.indexOf(':');
        check(colon > 0, "Invalid multipart header");
        if (line.left(colon).toLower() == "content-disposition") {
          check(disposition.isEmpty(), "Duplicate disposition");
          disposition = QString::fromUtf8(line.mid(colon + 1).trimmed());
        }
      }
      auto part = QRegularExpression("^form-data;\\s*name=\"(file|node_id)\"(?:"
                                     ";\\s*filename=\"([^\"]+)\")?$")
                      .match(disposition);
      check(part.hasMatch(), "Unsupported upload part");
      auto name = part.captured(1);
      check(!values.contains(name), "Duplicate multipart field");
      int next = r.body.indexOf("\r\n" + delimiter, end + 4);
      check(next >= end + 4, "Incomplete multipart data");
      values[name] = r.body.mid(end + 4, next - end - 4);
      if (name == "file")
        filename = part.captured(2);
      cursor = next + 2 + delimiter.size();
      if (r.body.mid(cursor, 4) == "--\r\n") {
        check(cursor + 4 == r.body.size(), "Trailing multipart data");
        break;
      }
      check(r.body.mid(cursor, 2) == "\r\n", "Invalid multipart separator");
      cursor += 2;
    }
    check(QRegularExpression("^[A-Za-z0-9][A-Za-z0-9_.-]{0,100}\\.(?:eds|dcf)$",
                             QRegularExpression::CaseInsensitiveOption)
              .match(filename)
              .hasMatch(),
          "EDS filename must be a simple .eds or .dcf basename");
    parseDictionary(values["file"]);
    QJsonValue node;
    if (values.contains("node_id"))
      node =
          int(integer(QString::fromUtf8(values["node_id"]), "node_id", 1, 127));
    check(QDir::isAbsolutePath(directory) &&
              !QFileInfo(directory).isSymLink() && QDir().mkpath(directory),
          "Invalid EDS directory");
    const auto path = directory + "/" + filename;
    check(!QFileInfo(path).isSymLink(), "Linked EDS file rejected");
    QMutexLocker lock(&mutex);
    QSaveFile file(path);
    check(file.open(QIODevice::WriteOnly) &&
              file.write(values["file"]) == values["file"].size() &&
              file.commit(),
          "Cannot save EDS");
    syncDirectory(directory);
    if (!node.isNull()) {
      auto cfg = configuration(), canopen = cfg["canopen"].toObject(),
           nodes = canopen["nodes"].toObject();
      nodes[QString::number(node.toInt())] = QJsonObject{{"eds", path}};
      canopen["nodes"] = nodes;
      cfg["canopen"] = canopen;
      durableJson(configPath, cfg);
    }
    return QJsonObject{{"ok", true}, {"filename", filename}, {"node_id", node}};
  }
  check(r.headers.value("content-type").startsWith("application/json"),
        "JSON PDO request required");
  auto p = object(r.body);
  fields(p, {"interface", "node_id", "pdo", "values"}, {"node_id"});
  int node = integer(p["node_id"], "node_id", 1, 127);
  const auto path = configuration()["canopen"]
                        .toObject()["nodes"]
                        .toObject()[QString::number(node)]
                        .toObject()["eds"]
                        .toString();
  const auto canonicalDirectory = QFileInfo(directory).canonicalFilePath();
  check(!canonicalDirectory.isEmpty() && !path.isEmpty() &&
            !QFileInfo(path).isSymLink() &&
            QFileInfo(QFileInfo(path).canonicalFilePath()).absolutePath() ==
                canonicalDirectory,
        "Node requires an EDS in the private dictionary directory");
  const auto dictionary = parseDictionary(readBytes(path, 512 * 1024));
  QJsonArray tpdo, rpdo;
  bool writing = r.url.path() == "/api/canopen/pdo/transmit";
  check(writing || r.url.path() == "/api/canopen/pdo/read",
        "Unknown PDO operation");
  if (writing)
    rpdo.append(int(
        integer(p.contains("pdo") ? p["pdo"] : QJsonValue(1), "pdo", 1, 512)));
  else
    for (auto i = dictionary.begin(); i != dictionary.end(); ++i) {
      if (!QRegularExpression("^[0-9a-f]{4}$").match(i.key()).hasMatch())
        continue;
      int index = i.key().toInt(nullptr, 16);
      if (index >= 0x1800 && index <= 0x19ff)
        tpdo.append(index - 0x1800 + 1);
      if (index >= 0x1400 && index <= 0x15ff)
        rpdo.append(index - 0x1400 + 1);
    }
  QJsonObject request{{"node_id", node}, {"tpdo", tpdo}, {"rpdo", rpdo}};
  if (p.contains("interface"))
    request["interface"] = p["interface"];
  auto result = action("canopen_pdo_read", request);
  if (writing) {
    auto maps = result["rpdo"].toArray();
    check(maps.size() == 1 && p["values"].isObject(),
          "RPDO variables required");
    auto map = maps[0].toObject(), values = p["values"].toObject();
    auto variables = map["variables"].toArray();
    QSet<QString> used;
    QByteArray bytes((map["bits"].toInt() + 7) / 8, 0);
    int offset = 0;
    for (auto v : variables) {
      auto variable = v.toObject();
      int index = variable["index"].toInt(), sub = variable["subindex"].toInt(),
          bits = variable["length"].toInt();
      auto definition = objectDefinition(dictionary, index, sub);
      auto name = definition["parametername"];
      check(values.contains(name) && !used.contains(name),
            "Every mapped RPDO variable must be provided exactly once");
      used.insert(name);
      int type = integer(definition["datatype"], "EDS data type", 1, 65535);
      auto encoded = encodeValue(values[name], type, bits);
      for (int bit = 0; bit < bits; ++bit)
        if (quint8(encoded[bit / 8]) & (1 << (bit % 8)))
          bytes[(offset + bit) / 8] = char(quint8(bytes[(offset + bit) / 8]) |
                                           (1 << ((offset + bit) % 8)));
      offset += bits;
    }
    check(used.size() == values.size(), "Unknown RPDO variable");
    request.remove("tpdo");
    request.remove("rpdo");
    request["pdo"] = rpdo[0];
    request["mapping"] = map;
    request["data"] = QString::fromLatin1(bytes.toHex());
    return action("canopen_pdo_transmit", request);
  }
  for (auto direction : QStringList{"tpdo", "rpdo"}) {
    QJsonArray maps;
    for (auto m : result[direction].toArray()) {
      auto map = m.toObject();
      QJsonArray variables;
      for (auto v : map["variables"].toArray()) {
        auto variable = v.toObject();
        auto definition =
            objectDefinition(dictionary, variable["index"].toInt(),
                             variable["subindex"].toInt());
        variable["name"] = definition["parametername"];
        variables.append(variable);
      }
      map["variables"] = variables;
      maps.append(map);
    }
    result[direction] = maps;
  }
  return result;
}
