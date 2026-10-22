#include "surfaceguardian.h"
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdexcept>
#include <string>
#include <sys/file.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

namespace
{
#if defined(SYS_pidfd_open) && defined(SYS_pidfd_send_signal)
constexpr long PidfdOpen = SYS_pidfd_open, PidfdSignal = SYS_pidfd_send_signal;
#elif defined(__arm__)
// Audited selected S4 Linux5.4 syscall table; never signal a numeric PID.
constexpr long PidfdOpen = 434, PidfdSignal = 424;
#else
#error "Audited pidfd syscall mapping required"
#endif
constexpr int CloseMilliseconds = 2500;
struct Message { char operation; int x, y, width, height; };
struct File
{
    int fd;
    explicit File(int value = -1) : fd(value) {}
    ~File() { if (fd >= 0) ::close(fd); }
    int release() { const int result = fd; fd = -1; return result; }
};
void needed(bool value, const char *message)
{
    if (!value) throw std::runtime_error(message);
}
long long now()
{
    timespec t{};
    needed(clock_gettime(CLOCK_MONOTONIC, &t) == 0, "Media monotonic clock unavailable");
    return static_cast<long long>(t.tv_sec) * 1000 + t.tv_nsec / 1000000;
}
bool exited(int pidfd)
{
    pollfd item{pidfd, POLLIN, 0};
    const int result = ::poll(&item, 1, 0);
    needed(result >= 0 && !(item.revents & (POLLERR | POLLNVAL)),
           "Media process handle unavailable");
    return result > 0 && (item.revents & (POLLIN | POLLHUP));
}
bool signalOwned(int pidfd, int signal)
{
    return ::syscall(PidfdSignal, pidfd, signal, nullptr, 0) == 0 || errno == ESRCH;
}
int directory(const char *path)
{
    needed(path && path[0] == '/' && path[1], "Private media runtime directory required");
    File current(::open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC));
    needed(current.fd >= 0, "Cannot open root directory");
    std::string text(path);
    size_t start = 1;
    while (start < text.size())
    {
        const size_t end = text.find('/', start);
        const auto part = text.substr(start, end == std::string::npos ? end : end - start);
        needed(!part.empty() && part != "." && part != "..", "Unsafe media runtime path");
        const int next = ::openat(current.fd, part.c_str(),
                                 O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        needed(next >= 0, "Media runtime path unavailable or symlink");
        ::close(current.fd); current.fd = next;
        if (end == std::string::npos) break;
        start = end + 1;
    }
    struct stat st{};
    needed(::fstat(current.fd, &st) == 0 && st.st_uid == ::geteuid() &&
               !(st.st_mode & 0077), "Media runtime directory is not private to HMI");
    return current.release();
}
int privateFile(int parent, const char *name)
{
    File fd(::openat(parent, name, O_RDWR | O_CREAT | O_NOFOLLOW | O_CLOEXEC, 0600));
    struct stat st{};
    needed(fd.fd >= 0 && ::fstat(fd.fd, &st) == 0 && S_ISREG(st.st_mode) &&
               st.st_nlink == 1 && st.st_uid == ::geteuid() &&
               (st.st_mode & 0777) == 0600, "Unsafe private media ownership file");
    return fd.release();
}
void isolateDescriptors(int control, int worker, int owner, int lock, int device, int report)
{
    DIR *directory = ::opendir("/proc/self/fd");
    needed(directory != nullptr, "Cannot isolate media guardian descriptors");
    const int own = ::dirfd(directory);
    while (auto *entry = ::readdir(directory))
    {
        char *end = nullptr;
        const long number = ::strtol(entry->d_name, &end, 10);
        if (!end || *end || number < 0 || number > INT_MAX) continue;
        const int fd = int(number);
        if (fd != own && fd != control && fd != worker && fd != owner && fd != lock &&
            fd != device && fd != report) ::close(fd);
    }
    ::closedir(directory);
}
void reportResult(int fd, const char *cause, bool cleared, bool forced)
{
    char text[256];
    const int length = ::snprintf(text, sizeof(text),
        "{\"schema\":1,\"cause\":\"%s\",\"surface_retired\":%s,"
        "\"paired_worker_force_stopped\":%s,\"CMA_reclamation_proven\":false}\n",
        cause, cleared ? "true" : "false", forced ? "true" : "false");
    if (length > 0 && length < int(sizeof(text)) && ::ftruncate(fd, 0) == 0)
    {
        const auto written = ::pwrite(fd, text, size_t(length), 0);
        // Evidence failure is never silently converted into a surface success.
        if (written != length) ::_exit(91);
    }
}
[[noreturn]] void guardian(int socket, int worker, int owner, int lock, int report,
                          MediaSurfaceBackend &backend)
{
    // Fork happens before Qt or vendor threads. No Qt, malloc callbacks or
    // decoder APIs run here. No inherited application leases or protocol FDs.
    if (::prctl(PR_SET_PDEATHSIG, 0) || ::setsid() < 0) ::_exit(81);
    (void)::prctl(PR_SET_NAME, "dshan-media-gd");
    isolateDescriptors(socket, worker, owner, lock, backend.descriptor(), report);
    MediaSurfaceRegion region{}, previous{};
    bool armed = false, forced = false;
    long long deadline = 0;
    const char *cause = "worker-exit";
    auto retire = [&]() {
        const bool ok = !armed || backend.retire(region, previous);
        reportResult(report, cause, ok, forced);
        if (ok) armed = false;
        return ok;
    };
    // A stale success from an earlier worker must never stand for this lease.
    reportResult(report, "idle-lease", true, false);
    const char ready = '0';
    if (::send(socket, &ready, 1, MSG_NOSIGNAL) != 1) ::_exit(82);
    for (;;)
    {
        if (exited(worker)) ::_exit(retire() ? 0 : 83);
        if (!deadline && exited(owner))
        {
            cause = "hmi-exit";
            if (!signalOwned(worker, SIGTERM)) ::_exit(84);
            deadline = now() + CloseMilliseconds;
        }
        if (deadline && now() >= deadline)
        {
            // Only the stable handle for the worker that forked this guardian.
            forced = true;
            if (!signalOwned(worker, SIGKILL)) ::_exit(85);
            pollfd target{worker, POLLIN, 0};
            if (::poll(&target, 1, 1000) <= 0 || !exited(worker)) ::_exit(86);
            ::_exit(retire() ? 0 : 87);
        }
        pollfd item{socket, POLLIN, 0};
        const int count = ::poll(&item, 1, 50);
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) ::_exit(88);
        if (!count) continue;
        Message request{};
        const auto n = ::recv(socket, &request, sizeof(request), MSG_DONTWAIT);
        if (n != sizeof(request))
        {
            if (!deadline)
            {
                cause = "control-closed";
                if (!signalOwned(worker, SIGTERM)) ::_exit(89);
                deadline = now() + CloseMilliseconds;
            }
            // Avoid spinning on a closed control socket during the exit wait.
            timespec delay{0, 50000000}; ::nanosleep(&delay, nullptr);
            continue;
        }
        bool ok = true;
        if (request.operation == 'A' && !deadline)
        {
            ok = request.width >= 8 && request.height >= 8 &&
                 request.x >= 0 && request.y >= 0 && request.width <= 8192 &&
                 request.height <= 8192 && request.x <= 8192 - request.width &&
                 request.y <= 8192 - request.height &&
                 !(request.x % 2 || request.y % 2 || request.width % 2 || request.height % 2);
            if (ok)
            {
                // A signal can arrive between arming a resize and its vendor
                // commit. Both adjacent leased rectangles remain ours.
                previous = armed ? region : MediaSurfaceRegion{};
                region.x = request.x; region.y = request.y;
                region.width = request.width; region.height = request.height;
                armed = true;
                // Persist failure-closed intent before acknowledging the claim:
                // the producer cannot create a decoder/plane before this ACK.
                reportResult(report, "surface-armed", false, false);
            }
        }
        else if (request.operation == 'C')
        {
            if (!deadline)
            {
                deadline = now() + CloseMilliseconds;
                cause = "decoder-close";
            }
        }
        else if (request.operation == 'R')
        {
            ok = retire();
            if (ok && !exited(owner)) deadline = 0;
        }
        else if (request.operation == 'Q' && !armed)
        {
            const char response = '0';
            (void)::send(socket, &response, 1, MSG_NOSIGNAL);
            ::_exit(0);
        }
        else ok = false;
        const char response = ok ? '0' : '1';
        if (::send(socket, &response, 1, MSG_NOSIGNAL) != 1 && !deadline)
            deadline = now() + CloseMilliseconds;
    }
}
}
MediaSurfaceGuardian::MediaSurfaceGuardian(const char *runtime,
                                           std::unique_ptr<MediaSurfaceBackend> backend)
{
    needed(bool(backend), "Video surface backend unavailable");
    File parent(directory(runtime));
    File lease(privateFile(parent.fd, "media-surface.lock"));
    const auto deadline = now() + 2800;
    while (::flock(lease.fd, LOCK_EX | LOCK_NB))
    {
        needed((errno == EWOULDBLOCK || errno == EAGAIN) && now() < deadline,
               "Previous media surface is still owned");
        timespec delay{0, 20000000}; ::nanosleep(&delay, nullptr);
    }
    needed(backend->initiallyIdle(), "Video surface is already owned by another player");
    File report(privateFile(parent.fd, "media-surface-last.json"));
    const pid_t owner = ::getppid();
    needed(owner > 1, "Media worker owner is unavailable");
    File ownerHandle(int(::syscall(PidfdOpen, owner, 0)));
    File workerHandle(int(::syscall(PidfdOpen, ::getpid(), 0)));
    needed(ownerHandle.fd >= 0 && workerHandle.fd >= 0 && !exited(ownerHandle.fd),
           "Media owner process handles unavailable");
    int pair[2];
    needed(::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, pair) == 0,
           "Cannot create private media guardian channel");
    File first(pair[0]), second(pair[1]);
    const pid_t launched = ::fork();
    needed(launched >= 0, "Cannot create media surface guardian");
    if (launched == 0)
    {
        try { guardian(second.fd, workerHandle.fd, ownerHandle.fd, lease.fd, report.fd, *backend); }
        catch (...) { ::_exit(90); }
    }
    child = launched; control = first.release();
    char ready = '1'; pollfd item{control, POLLIN, 0};
    if (!(::poll(&item, 1, 1000) > 0 && ::recv(control, &ready, 1, 0) == 1 && ready == '0'))
    {
        ::close(control); control = -1;
        throw std::runtime_error("Media guardian did not become ready");
    }
}
bool MediaSurfaceGuardian::exchange(char command, int x, int y, int width, int height)
{
    Message message{command, x, y, width, height};
    if (::send(control, &message, sizeof(message), MSG_NOSIGNAL) != sizeof(message)) return false;
    pollfd item{control, POLLIN, 0}; char response = '1';
    return ::poll(&item, 1, 1500) > 0 && ::recv(control, &response, 1, 0) == 1 && response == '0';
}
void MediaSurfaceGuardian::arm(int x, int y, int width, int height)
{
    needed(exchange('A', x, y, width, height), "Cannot claim owned media region");
}
void MediaSurfaceGuardian::beginClose()
{
    needed(exchange('C'), "Cannot arm bounded media close");
}
void MediaSurfaceGuardian::retired()
{
    needed(exchange('R'), "Owned media surface cleanup failed");
}
MediaSurfaceGuardian::~MediaSurfaceGuardian()
{
    if (control >= 0)
    {
        (void)exchange('Q'); ::close(control);
    }
    if (child > 0)
    {
        const auto deadline = now() + 500;
        while (::waitpid(child, nullptr, WNOHANG) == 0 && now() < deadline)
        {
            timespec delay{0, 10000000}; ::nanosleep(&delay, nullptr);
        }
        // Never kill the cleanup guardian. If the decoder is still armed, its
        // stable worker handle becomes ready when this process exits.
    }
}
