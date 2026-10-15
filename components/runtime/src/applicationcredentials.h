#pragma once
#include <sys/types.h>

// Prepared in the parent. The apply function uses only POSIX/kernel calls and
// fixed stack storage so it can also run between fork and exec.
struct ApplicationCredentials {
  uid_t uid;
  gid_t gid;
  gid_t groups[64];
  int groupCount;
  int lastKernelCapability;
  bool bindPrivilegedPort;
};
enum class CredentialStage {
  None,
  InvalidProfile,
  Groups,
  Bounding,
  KeepCapabilities,
  GroupIdentity,
  UserIdentity,
  Capabilities,
  Ambient,
  NoNewPrivileges,
  Verification
};
struct CredentialError {
  CredentialStage stage;
  int error;
};
CredentialError
applyApplicationCredentials(const ApplicationCredentials &profile);
