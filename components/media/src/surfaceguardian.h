#pragma once
#include <memory>

// The guardian owns no decoder/Qt objects. Its process survives the worker and
// waits for stable pidfds before touching only the leased Cedar video surface.
struct MediaSurfaceRegion { int x = 0, y = 0, width = 0, height = 0; };
class MediaSurfaceBackend
{
  public:
    virtual ~MediaSurfaceBackend() = default;
    virtual int descriptor() const = 0;
    virtual bool initiallyIdle() = 0;
    virtual bool retire(const MediaSurfaceRegion &current,
                        const MediaSurfaceRegion &previous) = 0;
};
class MediaSurfaceGuardian
{
    int control = -1;
    int child = -1;
    bool exchange(char command, int x = 0, int y = 0, int width = 0, int height = 0);
  public:
    MediaSurfaceGuardian(const char *runtimeDirectory,
                         std::unique_ptr<MediaSurfaceBackend> backend);
    ~MediaSurfaceGuardian();
    MediaSurfaceGuardian(const MediaSurfaceGuardian &) = delete;
    MediaSurfaceGuardian &operator=(const MediaSurfaceGuardian &) = delete;
    void arm(int x, int y, int width, int height);
    void beginClose();
    void retired();
};
std::unique_ptr<MediaSurfaceBackend> cedarSurfaceBackend();
