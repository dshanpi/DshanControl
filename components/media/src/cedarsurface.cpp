#include "surfaceguardian.h"
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <sdk_disp/sunxi_display2.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

namespace
{
static_assert(sizeof(disp_layer_config) == 208, "Selected S4 display v1 ABI changed");
static_assert(sizeof(disp_layer_config2) == 248, "Selected S4 display v2 ABI changed");
class CedarSurface final : public MediaSurfaceBackend
{
    int fd = -1;
    bool get(unsigned channel, disp_layer_config &value)
    {
        value = {}; value.channel = channel;
        unsigned long args[4] = {0, reinterpret_cast<unsigned long>(&value), 1, 0};
        return ::ioctl(fd, DISP_LAYER_GET_CONFIG, args) == 0;
    }
    static bool sameQt(const disp_layer_config &a, const disp_layer_config &b)
    {
        return a.enable == b.enable && a.channel == b.channel && a.layer_id == b.layer_id &&
               a.info.mode == b.info.mode && a.info.zorder == b.info.zorder &&
               a.info.fb.format == b.info.fb.format &&
               !std::memcmp(&a.info.screen_win, &b.info.screen_win, sizeof(a.info.screen_win)) &&
               !std::memcmp(a.info.fb.addr, b.info.fb.addr, sizeof(a.info.fb.addr)) &&
               !std::memcmp(a.info.fb.size, b.info.fb.size, sizeof(a.info.fb.size));
    }
    static bool matches(const disp_layer_config &value, const MediaSurfaceRegion &a,
                        const MediaSurfaceRegion &b)
    {
        auto match = [&](const MediaSurfaceRegion &r) {
            return r.width >= 8 && r.height >= 8 &&
                   value.info.screen_win.x == r.x && value.info.screen_win.y == r.y &&
                   value.info.screen_win.width == unsigned(r.width) &&
                   value.info.screen_win.height == unsigned(r.height);
        };
        return match(a) || match(b);
    }
    static void frameWait()
    {
        timespec remaining{0, 50000000};
        while (::nanosleep(&remaining, &remaining) && errno == EINTR) {}
    }
  public:
    CedarSurface() : fd(::open("/dev/disp", O_RDWR | O_CLOEXEC | O_NOFOLLOW)) {}
    ~CedarSurface() override { if (fd >= 0) ::close(fd); }
    int descriptor() const override { return fd; }
    bool initiallyIdle() override
    {
        disp_layer_config video{};
        return fd >= 0 && get(0, video) && !video.enable;
    }
    bool retire(const MediaSurfaceRegion &current, const MediaSurfaceRegion &previous) override
    {
        disp_layer_config video{}, qt{}, after{};
        if (!get(0, video) || !get(1, qt)) return false;
        // The lease precedes TPlayerCreate. Only its channel0/layer0 region may
        // be retired. A changed enabled layer belongs to an unknown producer.
        if (video.enable)
        {
            if (video.channel != 0 || video.layer_id != 0 ||
                video.info.mode != LAYER_MODE_BUFFER || video.info.zorder != 0 ||
                !matches(video, current, previous)) return false;
            video.enable = false;
            unsigned long args[4] = {0, reinterpret_cast<unsigned long>(&video), 1, 0};
            if (::ioctl(fd, DISP_LAYER_SET_CONFIG, args) != 0) return false;
            frameWait();
        }
        // Vendor5.4 unmaps the previous backed-up applied layer set before each
        // config2 commit. Two disabled commits retire both deferred references.
        for (int i = 0; i != 2; ++i)
        {
            if (!get(0, after) || after.enable) return false;
            disp_layer_config2 empty{}; empty.channel = 0; empty.layer_id = 0;
            unsigned long args[4] = {0, reinterpret_cast<unsigned long>(&empty), 1, 0};
            if (::ioctl(fd, DISP_LAYER_SET_CONFIG2, args) != 0) return false;
            frameWait();
        }
        disp_layer_config afterQt{};
        return get(0, after) && !after.enable && get(1, afterQt) && sameQt(qt, afterQt);
    }
};
}
std::unique_ptr<MediaSurfaceBackend> cedarSurfaceBackend()
{
    return std::unique_ptr<MediaSurfaceBackend>(new CedarSurface);
}
