#include "../../components/media/src/surfaceguardian.h"
#include <cstdio>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

// Compile-time test backend only. The production executable has no fixture
// selection/environment, and real S4 ioctls have a separate ARM qualification.
struct Shared { int enabled, x, y, width, height, retires, qt, foreign; };
class Fake final : public MediaSurfaceBackend
{
    int file;
    Shared *state;
  public:
    explicit Fake(const char *path) : file(::open(path, O_RDWR | O_CLOEXEC | O_NOFOLLOW))
    {
        if (file < 0) throw std::runtime_error("fixture file");
        state = static_cast<Shared *>(::mmap(nullptr, sizeof(Shared), PROT_READ | PROT_WRITE,
                                             MAP_SHARED, file, 0));
        if (state == MAP_FAILED) throw std::runtime_error("fixture mmap");
    }
    ~Fake() override { ::munmap(state, sizeof(Shared)); ::close(file); }
    int descriptor() const override { return file; }
    bool initiallyIdle() override { return state->enabled == 0; }
    bool retire(const MediaSurfaceRegion &a, const MediaSurfaceRegion &b) override
    {
        auto matches = [&](const MediaSurfaceRegion &r) {
            return r.width && state->x == r.x && state->y == r.y &&
                   state->width == r.width && state->height == r.height;
        };
        if (state->foreign || (state->enabled && !matches(a) && !matches(b))) return false;
        state->enabled = 0; ++state->retires;
        return true;
    }
    void play(int x, int y) { state->x = x; state->y = y; state->width = 312;
                              state->height = 174; state->enabled = 1; }
};
int main(int argc, char **argv)
{
    if (argc != 3) return 2;
    try
    {
        Fake producer(argv[2]);
        MediaSurfaceGuardian guardian(argv[1], std::unique_ptr<MediaSurfaceBackend>(new Fake(argv[2])));
        std::cout << "READY\n" << std::flush;
        std::string command;
        while (std::getline(std::cin, command))
        {
            if (command == "play") { guardian.arm(236, 192, 312, 174); producer.play(236, 192); }
            else if (command == "resize-before-commit") guardian.arm(300, 200, 312, 174);
            else if (command == "close") { guardian.beginClose(); guardian.retired(); }
            else if (command == "block-close") { guardian.beginClose(); for (;;) ::pause(); }
            else if (command == "quit") break;
            else return 3;
            std::cout << "ACK\n" << std::flush;
        }
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 4; }
}
