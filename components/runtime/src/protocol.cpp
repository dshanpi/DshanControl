#include "protocol.h"
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <openssl/evp.h>
#include <openssl/pem.h>

const QStringList TargetFields = {
    "board_id", "hardware_revision", "architecture", "float_abi",
    "libc",     "runtime_id",        "contract_id",  "updater_protocol"};
namespace {
bool semver(const QString &value) {
  return QRegularExpression("^[0-9]+\\.[0-9]+\\.[0-9]+(?:-[0-9A-Za-z.-]+)?$")
      .match(value)
      .hasMatch();
}
bool versionAtLeast(const QString &minimum) {
  if (!QRegularExpression("^[0-9]+\\.[0-9]+\\.[0-9]+$")
           .match(minimum)
           .hasMatch())
    return false;
  auto parts = minimum.split('.');
  const int actual[] = {0, 1, 0};
  for (int i = 0; i < 3; ++i) {
    bool valid;
    qint64 n = parts[i].toLongLong(&valid);
    if (!valid)
      return false;
    if (actual[i] < n)
      return false;
    if (actual[i] > n)
      return true;
  }
  return true;
}
} // namespace
void validateTarget(const QJsonObject &target) {
  for (const auto &key : TargetFields) {
    if (key == "updater_protocol") {
      require(target[key].isDouble() && target[key].toDouble() == 1,
              "Unsupported updater protocol");
      continue;
    }
    auto value = target[key].toString();
    require(identifier(value) && value != "unknown" && value != "pending",
            "Missing/unknown target field: " + key);
  }
}
void validateManifest(const QJsonObject &m, bool publicRelease) {
  require(m["schema_version"].isDouble() && m["schema_version"].toDouble() == 1,
          "Unsupported manifest schema");
  require(m["app_id"] == "dshancontrol" &&
              m["repository"] == "dshanpi/DshanControl",
          "Wrong product/repository");
  require(m["release_kind"] == "application", "Not an application release");
  require(m["channel"] == "stable" || m["channel"] == "prerelease",
          "Unknown channel");
  require(m["release_state"] == "draft" || m["release_state"] == "qualified",
          "Unknown release state");
  QString version = m["version"].toString();
  require(identifier(version) && semver(version) &&
              m["tag"] == "app-v" + version,
          "Version/tag mismatch");
  require(m["channel"] != "stable" || !version.contains('-'),
          "Prerelease version on stable channel");
  require(QRegularExpression("^[a-f0-9]{40}$")
              .match(m["commit"].toString())
              .hasMatch(),
          "Full application commit required");
  require(positiveInteger(m["sequence"]) && positiveInteger(m["data_schema"]),
          "Invalid sequence/data schema");
  require(versionAtLeast(m["minimum_updater_version"].toString()),
          "Updater too old or invalid minimum version");
  auto from = m["from_versions"].toArray();
  require(m["from_versions"].isArray() && !from.isEmpty(),
          "Explicit upgrade path required");
  for (const auto &v : from)
    require(v.isString() && identifier(v.toString()), "Invalid upgrade source");
  auto targets = m["targets"].toArray();
  require(m["targets"].isArray() && !targets.isEmpty(), "No target packages");
  QSet<QByteArray> seen;
  for (const auto &value : targets) {
    require(value.isObject(), "Invalid target entry");
    auto target = value.toObject();
    validateTarget(target);
    QJsonArray identity;
    for (const auto &key : TargetFields)
      identity.append(target[key]);
    auto key = QJsonDocument(identity).toJson(QJsonDocument::Compact);
    require(!seen.contains(key), "Duplicate target identity");
    seen.insert(key);
    require(identifier(target["filename"].toString()) &&
                target["filename"].toString().endsWith(".tar.gz"),
            "Invalid target filename");
    require(sha256(target["sha256"]) && positiveInteger(target["size"]) &&
                positiveInteger(target["unpacked_size"]),
            "Invalid target sizes/hash");
    if (publicRelease || m["release_state"] == "qualified") {
      auto e = target["validation"].toObject();
      for (const auto &gate : QStringList{
               "host_tests", "target_build", "board_function",
               "compatibility_rejection", "signature_rejection", "power_loss",
               "rollback", "data_preservation", "maintenance_interlock"})
        require(e[gate] == "passed", "Release gate incomplete: " + gate);
      for (const auto &field : QStringList{
               "system_image_sha256", "system_contract_sha256", "log_sha256"})
        require(sha256(e[field]), "Validation hash missing");
      require(!e["tester"].toString().isEmpty() &&
                  !e["tested_at"].toString().isEmpty(),
              "Validation author/time missing");
    }
  }
  for (const auto &field : QStringList{"source", "build_info"}) {
    auto asset = m[field].toObject();
    require(m[field].isObject() && identifier(asset["filename"].toString()) &&
                sha256(asset["sha256"]),
            "Source/build-info binding missing");
  }
  if (publicRelease)
    require(m["release_state"] == "qualified",
            "Draft cannot enter production update feed");
}
QJsonObject matchTarget(const QJsonObject &m, const QJsonObject &contract,
                        const QJsonObject &installed, qint64 highWater) {
  validateManifest(m, false);
  validateTarget(contract);
  require(contract["app_id"] == "dshancontrol" &&
              contract["repository"] == "dshanpi/DshanControl",
          "Wrong installed product/repository");
  require(contract["updater_version"] == "0.1.0",
          "Installed updater/contract mismatch");
  auto channels = contract["allowed_channels"].toArray();
  require(contract["allowed_channels"].isArray() && !channels.isEmpty(),
          "Explicit channels required");
  for (const auto &channel : channels)
    require(channel == "stable" || channel == "prerelease",
            "Invalid allowed channel");
  require(channels.contains(m["channel"]), "Channel not allowed");
  auto version = installed["version"].toString();
  require(identifier(version) && version != "unknown" && version != "pending",
          "Missing installed version");
  require(m["sequence"].toDouble() > highWater,
          "Replayed or downgraded release");
  require(m["from_versions"].toArray().contains(version),
          "No upgrade path from installed version");
  require(positiveInteger(installed["data_schema"]) &&
              positiveInteger(contract["data_schema"]) &&
              m["data_schema"] == installed["data_schema"] &&
              m["data_schema"] == contract["data_schema"],
          "Data migration unsupported by protocol v1");
  QJsonObject selected;
  int count = 0;
  for (const auto &v : m["targets"].toArray()) {
    auto target = v.toObject();
    bool match = true;
    for (const auto &field : TargetFields)
      match = match && target[field] == contract[field];
    if (match) {
      selected = target;
      ++count;
    }
  }
  require(count == 1, "No exact board/ABI/runtime/contract match");
  return selected;
}
void verifySignature(const QByteArray &manifest, const QByteArray &signature,
                     const QString &publicKey) {
  auto bytes = readBytes(publicKey, 16384);
  BIO *bio = BIO_new_mem_buf(bytes.constData(), bytes.size());
  require(bio, "Cannot open trust root", "signature");
  EVP_PKEY *key = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
  BIO_free(bio);
  require(key, "Invalid system-owned trust root", "signature");
  if (EVP_PKEY_base_id(key) != EVP_PKEY_RSA || EVP_PKEY_bits(key) < 2048) {
    EVP_PKEY_free(key);
    throw Failure("signature-rejected", "signature",
                  "Unsupported/weak system key");
  }
  EVP_MD_CTX *ctx = EVP_MD_CTX_new();
  int ok =
      ctx &&
      EVP_DigestVerifyInit(ctx, nullptr, EVP_sha256(), nullptr, key) == 1 &&
      EVP_DigestVerifyUpdate(ctx, manifest.constData(), manifest.size()) == 1 &&
      EVP_DigestVerifyFinal(
          ctx, reinterpret_cast<const unsigned char *>(signature.constData()),
          signature.size()) == 1;
  EVP_MD_CTX_free(ctx);
  EVP_PKEY_free(key);
  require(ok, "Manifest signature rejected", "signature", "signature-rejected");
}
