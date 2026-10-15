#include "applicationauthorization.h"
#include <QJsonArray>
#include <QSet>
#include <unistd.h>

namespace {
const QString Policy = "s4-roles-v1";
void check(bool value, const QString &message) {
  require(value, message, "authorization", "peer-policy");
}
} // namespace
void validateApplicationPeers(const QJsonObject &service,
                              const QJsonObject &updater) {
  check(!service.contains("peer_policy") || service["peer_policy"].isString(),
        "Invalid application peer policy");
  const auto policy = service["peer_policy"].toString();
  check(policy.isEmpty() || policy == Policy,
        "Unsupported application peer policy");
  const auto health = updater["application_health"].toObject();
  const bool productionManaged = !updater["development_mode"].toBool(false) &&
                                 health["launch_receipts_required"] == true;
  check(!productionManaged || policy == Policy,
        "Managed production roles require explicit peer policy");
  if (policy.isEmpty())
    return;
  check(::getuid() == 0 && ::geteuid() == 0,
        "Application peer policy requires root system owner");
  check(service["client_gid"] == 400,
        "Dedicated application IPC group required");
  const auto entries = service["allowed_uids"].toArray();
  QSet<int> uids;
  for (const auto &entry : entries) {
    check(positiveInteger(entry, true) && entry.toDouble() <= 2147483647,
          "Invalid application peer UID");
    uids.insert(int(entry.toDouble()));
  }
  check(entries.size() == 3 && uids == QSet<int>{0, 400, 401},
        "Dedicated application peer identities required");
}
void authorizeApplicationPeer(const QJsonObject &service, uid_t uid,
                              const QString &method) {
  if (service["peer_policy"].toString() != Policy || uid == 0)
    return;
  const QSet<QString> shared{"display.brightness.get", "display.brightness.set",
                             "service.health"};
  const QSet<QString> hmi{"update.identity", "update.status", "update.start"};
  const bool allowed =
      (uid == 400 || uid == 401) &&
      (shared.contains(method) || (uid == 401 && hmi.contains(method)));
  require(allowed, "Method is not available to this application role",
          "authorization", "method-denied");
}
