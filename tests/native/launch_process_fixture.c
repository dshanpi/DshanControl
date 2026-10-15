#define _GNU_SOURCE
#include <dirent.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <unistd.h>

int main(int argc,char **argv) {
  int extra=0;
  DIR *directory=opendir("/proc/self/fd");
  if(!directory)return 2;
  struct dirent *entry;
  while((entry=readdir(directory))) {
    char *end;long value=strtol(entry->d_name,&end,10);
    if(*entry->d_name && !*end && value>2 && value!=dirfd(directory))++extra;
  }
  closedir(directory);
  printf("{\"uid\":%u,\"gid\":%u,\"nnp\":%d,\"extra_fds\":%d,\"secret_env_present\":%d,\"lease_env_present\":%d,\"role\":\"%s\"}\n",
      (unsigned)getuid(),(unsigned)getgid(),prctl(PR_GET_NO_NEW_PRIVS,0UL,0UL,0UL,0UL),extra,
      getenv("DSHANCONTROL_TEST_SECRET")!=NULL,
      getenv("DSHANCONTROL_UPDATE_LEASE_FD")!=NULL || getenv("DSHANCONTROL_CONTROL_LEASE_FD")!=NULL,
      argc>1?argv[1]:"missing");
  fflush(stdout);
  /* Fail-safe lifetime for isolated tests only; production binaries have no such alarm. */
  alarm(60);
  for(;;)pause();
}
