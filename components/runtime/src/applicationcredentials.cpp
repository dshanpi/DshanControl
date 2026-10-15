#include "applicationcredentials.h"
#include <errno.h>
#include <grp.h>
#include <linux/capability.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

// These constants are present in the selected Linux5.4 UAPI; the fixed older
// cross-toolchain libc headers predate the ambient-capability interface.
#ifndef PR_CAP_AMBIENT
#define PR_CAP_AMBIENT 47
#define PR_CAP_AMBIENT_IS_SET 1
#define PR_CAP_AMBIENT_RAISE 2
#define PR_CAP_AMBIENT_CLEAR_ALL 4
#endif

namespace {
CredentialError error(CredentialStage stage, int number = errno) {
  return {stage, number};
}
bool readCapabilities(__user_cap_data_struct (&data)[2]) {
  __user_cap_header_struct header{};
  header.version = _LINUX_CAPABILITY_VERSION_3;
  return ::syscall(SYS_capget, &header, data) == 0;
}
bool writeCapabilities(bool bind) {
  __user_cap_header_struct header{};
  header.version = _LINUX_CAPABILITY_VERSION_3;
  __user_cap_data_struct data[2]{};
  if (bind)
    data[0].effective = data[0].permitted = data[0].inheritable =
        1U << CAP_NET_BIND_SERVICE;
  return ::syscall(SYS_capset, &header, data) == 0;
}
bool sameGroups(const ApplicationCredentials &profile) {
  gid_t actual[64];
  int count = ::getgroups(64, actual);
  if (count != profile.groupCount)
    return false;
  for (int i = 0; i < count; ++i) {
    bool found = false;
    for (int j = 0; j < count; ++j)
      if (actual[i] == profile.groups[j])
        found = true;
    if (!found)
      return false;
  }
  return true;
}
} // namespace

CredentialError applyApplicationCredentials(const ApplicationCredentials &p) {
  if (p.uid == 0 || p.uid > 2147483647U || p.gid == 0 || p.gid > 2147483647U ||
      p.groupCount < 0 || p.groupCount > 64 ||
      p.lastKernelCapability < CAP_NET_BIND_SERVICE ||
      p.lastKernelCapability > 63)
    return error(CredentialStage::InvalidProfile, EINVAL);
  for (int i = 0; i < p.groupCount; ++i) {
    if (p.groups[i] == 0 || p.groups[i] > 2147483647U)
      return error(CredentialStage::InvalidProfile, EINVAL);
    for (int j = 0; j < i; ++j)
      if (p.groups[j] == p.groups[i])
        return error(CredentialStage::InvalidProfile, EINVAL);
  }
  const bool privileged = ::geteuid() == 0;
  if (privileged) {
    if (::setgroups(p.groupCount, p.groups) != 0)
      return error(CredentialStage::Groups);
    for (int cap = 0; cap <= p.lastKernelCapability; ++cap)
      if (!(p.bindPrivilegedPort && cap == CAP_NET_BIND_SERVICE) &&
          ::prctl(PR_CAPBSET_DROP, static_cast<unsigned long>(cap), 0UL, 0UL,
                  0UL) != 0)
        return error(CredentialStage::Bounding);
    if (::prctl(PR_SET_KEEPCAPS, 1UL, 0UL, 0UL, 0UL) != 0)
      return error(CredentialStage::KeepCapabilities);
    if (::setresgid(p.gid, p.gid, p.gid) != 0)
      return error(CredentialStage::GroupIdentity);
    if (::setresuid(p.uid, p.uid, p.uid) != 0)
      return error(CredentialStage::UserIdentity);
  } else {
    // A nonroot test/process can only retain its existing identity and remove
    // privileges; it cannot request a capability or change supplemental groups.
    uid_t r, e, s;
    gid_t gr, ge, gs;
    if (p.bindPrivilegedPort || ::getresuid(&r, &e, &s) != 0 ||
        ::getresgid(&gr, &ge, &gs) != 0 || r != p.uid || e != p.uid ||
        s != p.uid || gr != p.gid || ge != p.gid || gs != p.gid ||
        !sameGroups(p))
      return error(CredentialStage::InvalidProfile, EPERM);
  }
  if (!writeCapabilities(p.bindPrivilegedPort))
    return error(CredentialStage::Capabilities);
  if (::prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_CLEAR_ALL, 0UL, 0UL, 0UL) != 0)
    return error(CredentialStage::Ambient);
  if (p.bindPrivilegedPort &&
      ::prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE,
              static_cast<unsigned long>(CAP_NET_BIND_SERVICE), 0UL, 0UL) != 0)
    return error(CredentialStage::Ambient);
  if (privileged && ::prctl(PR_SET_KEEPCAPS, 0UL, 0UL, 0UL, 0UL) != 0)
    return error(CredentialStage::KeepCapabilities);
  if (::prctl(PR_SET_NO_NEW_PRIVS, 1UL, 0UL, 0UL, 0UL) != 0)
    return error(CredentialStage::NoNewPrivileges);
  uid_t r, e, s;
  gid_t gr, ge, gs;
  __user_cap_data_struct capabilities[2]{};
  const auto mask = p.bindPrivilegedPort ? 1U << CAP_NET_BIND_SERVICE : 0U;
  if (::getresuid(&r, &e, &s) != 0 || ::getresgid(&gr, &ge, &gs) != 0 ||
      r != p.uid || e != p.uid || s != p.uid || gr != p.gid || ge != p.gid ||
      gs != p.gid || !sameGroups(p) ||
      ::prctl(PR_GET_NO_NEW_PRIVS, 0UL, 0UL, 0UL, 0UL) != 1 ||
      !readCapabilities(capabilities) || capabilities[0].effective != mask ||
      capabilities[0].permitted != mask ||
      capabilities[0].inheritable != mask || capabilities[1].effective != 0 ||
      capabilities[1].permitted != 0 || capabilities[1].inheritable != 0)
    return error(CredentialStage::Verification, EPERM);
  for (int cap = 0; cap <= p.lastKernelCapability; ++cap) {
    const auto allowed =
        p.bindPrivilegedPort && cap == CAP_NET_BIND_SERVICE ? 1 : 0;
    if (::prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_IS_SET,
                static_cast<unsigned long>(cap), 0UL, 0UL) != allowed ||
        (privileged && ::prctl(PR_CAPBSET_READ, static_cast<unsigned long>(cap),
                               0UL, 0UL, 0UL) != allowed))
      return error(CredentialStage::Verification, EPERM);
  }
  return {CredentialStage::None, 0};
}
