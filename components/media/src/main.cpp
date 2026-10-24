// The vendor player runs in a child process; its blocking calls and stdout never
// share the Qt GUI event loop or the structured protocol output.
#include "../../runtime/src/storage.h"
#include "surfaceguardian.h"
#include <QFileInfo>
#include <stdexcept>
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMetaObject>
#include <QRect>
#include <QSocketNotifier>
#include <QTextStream>
#include <QTimer>
#include <atomic>
#include <csignal>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef DSHAN_CEDAR_PLAYER
#include <tplayer.h>
#endif

namespace
{
volatile sig_atomic_t terminationRequested = 0;
void requestTermination(int) { terminationRequested = 1; }
void exact(const QJsonObject &object, QStringList keys)
{
    auto actual = object.keys();
    actual.sort();
    keys.sort();
    require(actual == keys, "Invalid media request fields", "request");
}
QJsonObject failure(const QString &code) { return {{"code", code}}; }
class MediaWorker : public QObject
{
    QFile output;
    QTimer poll;
    QJsonObject status;
    QRect region;
    QByteArray input;
    int heldFile = -1;
    std::atomic<int> generation{0};
    int preparingTicks = 0;
    bool autostart = false, prepared = false;
#ifdef DSHAN_CEDAR_PLAYER
    MediaSurfaceGuardian *surface;
    bool surfaceArmed = false;
    TPlayer *handle = nullptr;
    struct Callback
    {
        MediaWorker *player;
        int generation;
    };
    Callback *callback = nullptr;
    static int notify(void *context, int event, int extra, void *)
    {
        const auto value = static_cast<Callback *>(context);
        auto *self = value->player;
        const auto identity = value->generation;
        QMetaObject::invokeMethod(
            self,
            [self, identity, event, extra]() {
                if (identity != self->generation.load())
                    return;
                try
                {
                    self->event(event, extra);
                }
                catch (const Failure &error)
                {
                    try { self->fail(error.code); }
                    catch (const std::exception &) { self->surfaceFailure(); }
                }
                catch (const std::exception &) { self->surfaceFailure(); }
            },
            Qt::QueuedConnection);
        return 0;
    }
    void event(int event, int extra)
    {
        if (!handle)
            return;
        if (event == TPLAYER_NOTIFY_PREPARED)
        {
            prepared = true;
            preparingTicks = 0;
            auto *info = TPlayerGetMediaInfo(handle);
            require(info && info->nVideoStreamNum > 0 && info->pVideoStreamInfo, "No video stream",
                    "media");
            require(info->pVideoStreamInfo->nWidth > 0 && info->pVideoStreamInfo->nWidth <= 8192 &&
                        info->pVideoStreamInfo->nHeight > 0 &&
                        info->pVideoStreamInfo->nHeight <= 8192 && info->nDurationMs >= 0,
                    "Invalid video metadata", "media");
            status["width"] = info->pVideoStreamInfo->nWidth;
            status["height"] = info->pVideoStreamInfo->nHeight;
            status["codec_id"] = info->pVideoStreamInfo->eCodecFormat;
            status["seekable"] = bool(info->bSeekable);
            status["duration_ms"] = double(info->nDurationMs);
            status["state"] = "prepared";
            if (autostart)
            {
                if (TPlayerStart(handle) != 0)
                {
                    fail("start-failed");
                    return;
                }
                status["state"] = "starting";
            }
        }
        else if (event == TPLAYER_NOTIFY_PICTURE_STARTING_SHOW)
        {
            status["first_frame"] = true;
            if (status["state"] != "paused")
                status["state"] = "playing";
        }
        else if (event == TPLAYER_NOTIFY_PLAYBACK_COMPLETE)
        {
            LayerCtrlHideVideo(handle->mLayerCtrl);
            status["state"] = "completed";
        }
        else if (event == TPLAYER_NOTIFY_SEEK_COMPLETE)
        {
            status["seeking"] = false;
        }
        else if (event == TPLAYER_NOTIFY_BUFFER_START)
        {
            status["buffering"] = true;
        }
        else if (event == TPLAYER_NOTIFY_BUFFER_END)
        {
            status["buffering"] = false;
        }
        else if (event == TPLAYER_NOTIFY_NOT_SEEKABLE)
        {
            status["seekable"] = false;
            status["seeking"] = false;
        }
        else if (event == TPLAYER_NOTIFY_MEDIA_ERROR)
        {
            status["vendor_error"] = extra;
            fail("decode-failed");
            return;
        }
        else if (event >= TPLAYER_NOTIFY_DISP_RECT_SET_ERROR &&
                 event <= TPLAYER_NOTIFY_DISP_DISLOCATION_ERROR)
        {
            fail("display-region-failed");
            return;
        }
        emitStatus();
    }
#endif
  public:
    explicit MediaWorker(int fd, MediaSurfaceGuardian *guardian)
#ifdef DSHAN_CEDAR_PLAYER
        : surface(guardian)
#endif
    {
#ifndef DSHAN_CEDAR_PLAYER
        Q_UNUSED(guardian);
#endif
        require(fd >= 0 && output.open(fd, QIODevice::WriteOnly, QFileDevice::AutoCloseHandle),
                "Cannot open media protocol output");
        status = {{"backend",
#ifdef DSHAN_CEDAR_PLAYER
                   "cedar-tplayer"
#else
                   "unavailable"
#endif
                  },
                  {"available",
#ifdef DSHAN_CEDAR_PLAYER
                   true
#else
                   false
#endif
                  },
                  {"capabilities", QJsonArray{"suspend-during-prepare", "visibility-state"}},
                  {"state", "idle"},
                  {"first_frame", false},
                  {"hardware_decode_confirmed", false},
                  {"position_ms", 0},
                  {"duration_ms", 0},
                  {"seekable", false},
                  {"seeking", false}};
        int flags = ::fcntl(STDIN_FILENO, F_GETFL);
        require(flags >= 0 && ::fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) == 0,
                "Cannot configure media input");
        auto *notifier = new QSocketNotifier(STDIN_FILENO, QSocketNotifier::Read, this);
        connect(notifier, &QSocketNotifier::activated, this, [this](int) { read(); });
        poll.setInterval(500);
        connect(&poll, &QTimer::timeout, this, [this]() {
            try {
#ifdef DSHAN_CEDAR_PLAYER
            if (handle && prepared)
            {
                int position = 0, duration = 0;
                if (TPlayerGetCurrentPosition(handle, &position) == 0 && position >= 0)
                    status["position_ms"] = position;
                if (TPlayerGetDuration(handle, &duration) == 0 && duration >= 0)
                    status["duration_ms"] = duration;
                float fps = 0;
                if (TPlayerGetVideoDispFramerate(handle, &fps) == 0 && fps >= 0 && fps <= 240)
                    status["display_fps"] = fps;
                if (status["state"] == "starting" && status["first_frame"] != true &&
                    ++preparingTicks > 20)
                {
                    fail("first-frame-timeout");
                    return;
                }
                emitStatus();
            }
            else if (handle && ++preparingTicks > 30)
                fail("prepare-timeout");
#endif
            }
            catch (const std::exception &) { surfaceFailure(); }
        });
        poll.start();
        emitStatus();
    }
    ~MediaWorker() override
    {
        try { close(); }
        catch (...) { /* The guardian still holds the worker pidfd and lease. */ }
    }
    void surfaceFailure()
    {
        // No new decoder is allowed after an ownership-channel failure.
        // The independent process observes our exit and finishes retirement.
        ++generation;
        autostart = false;
        status["state"] = "error";
        status["error"] = "surface-cleanup-failed";
        status["first_frame"] = false;
        emitStatus();
        QCoreApplication::exit(1);
    }
    void send(QJsonObject value)
    {
        value["protocol"] = 1;
        output.write(QJsonDocument(value).toJson(QJsonDocument::Compact) + '\n');
        output.flush();
    }
    void emitStatus()
    {
        auto value = status;
        value["event"] = "status";
        send(value);
    }
    void close()
    {
        ++generation;
        // Completion callbacks from this decoder are now invalid. Clear their
        // pending state here; cancellation cannot wait for SEEK_COMPLETE.
        status["seeking"] = false;
        status["buffering"] = false;
        status["seekable"] = false;
        prepared = false;
        autostart = false;
#ifdef DSHAN_CEDAR_PLAYER
        if (handle || surfaceArmed)
            surface->beginClose();
        if (handle)
        {
            TPlayerStop(handle);
            // Hide after rendering stops: an in-flight frame must not restore
            // a plane hidden before Stop. The independent guardian drains its
            // deferred DMA references after Destroy, even on SIGKILL.
            LayerCtrlHideVideo(handle->mLayerCtrl);
            TPlayerDestroy(handle);
            handle = nullptr;
        }
        if (surfaceArmed)
        {
            surface->retired();
            surfaceArmed = false;
        }
        delete callback;
        callback = nullptr;
#endif
        if (heldFile >= 0)
        {
            ::close(heldFile);
            heldFile = -1;
        }
    }
    void fail(const QString &code)
    {
        close();
        status["state"] = "error";
        status["error"] = code;
        status["first_frame"] = false;
        emitStatus();
    }
    void rectangle(const QJsonObject &params)
    {
        exact(params, {"x", "y", "width", "height"});
        for (const auto &key : {"x", "y", "width", "height"})
            require(positiveInteger(params[key], true) && params[key].toDouble() <= 8192 &&
                        params[key].toInt() % 2 == 0,
                    "Even physical display coordinates required", "media");
        require(params["width"].toInt() >= 8 && params["height"].toInt() >= 8,
                "Invalid video display area", "media");
#ifdef DSHAN_CEDAR_PLAYER
        const int fb = ::open("/dev/fb0", O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
        fb_var_screeninfo screen{};
        const bool readable = fb >= 0 && ::ioctl(fb, FBIOGET_VSCREENINFO, &screen) == 0;
        if (fb >= 0)
            ::close(fb);
        require(readable && screen.xres > 0 && screen.yres > 0 && screen.xres <= 8192 &&
                    screen.yres <= 8192,
                "Cannot inspect physical output bounds", "media");
        require(params["x"].toInt() + params["width"].toInt() <= int(screen.xres) &&
                    params["y"].toInt() + params["height"].toInt() <= int(screen.yres),
                "Video rectangle exceeds physical output", "media");
#endif
        const QRect next(params["x"].toInt(), params["y"].toInt(), params["width"].toInt(),
                         params["height"].toInt());
        if (next == region)
            return;
#ifdef DSHAN_CEDAR_PLAYER
        if (handle)
        {
            const bool resume = status["state"] == "playing" || status["state"] == "starting";
            if (resume && TPlayerPause(handle) != 0)
                throw Failure("resize-pause-failed", "media", "Cannot quiesce video for resize");
            // This SDK's SetDisplayRect only updates a cached rectangle. Its
            // public hide operation commits that rectangle and resets isPlaying;
            // the next video frame re-enables the plane at the new dimensions.
            // Pause first so an in-flight frame cannot restore the old plane.
            surface->arm(next.x(), next.y(), next.width(), next.height());
            TPlayerSetDisplayRect(handle, next.x(), next.y(), unsigned(next.width()),
                                  unsigned(next.height()));
            LayerCtrlHideVideo(handle->mLayerCtrl);
            if (status["visible"] == true)
                TPlayerSetVideoDisplay(handle, true);
            if (resume && TPlayerStart(handle) != 0)
            {
                fail("resize-resume-failed");
                throw Failure("resize-resume-failed", "media", "Cannot resume resized video");
            }
        }
#endif
        region = next;
    }
    void command(const QString &name, const QJsonObject &params)
    {
        if (name == "snapshot")
        {
            exact(params, {});
            emitStatus();
            return;
        }
        if (name == "rect")
        {
            rectangle(params);
            return;
        }
        if (name == "close")
        {
            exact(params, {});
            close();
            status["state"] = "idle";
            status["first_frame"] = false;
            status["visible"] = false;
            status["position_ms"] = 0;
            status["duration_ms"] = 0;
            emitStatus();
            return;
        }
#ifndef DSHAN_CEDAR_PLAYER
        Q_UNUSED(params);
        throw Failure("decoder-unavailable", "media", "This build has no Cedar player");
#else
        if (name == "open")
        {
            exact(params, {"path", "volume"});
            require(region.isValid() && positiveInteger(params["volume"], true) &&
                        params["volume"].toDouble() <= 100,
                    "Display region and volume required", "media");
            const auto path = params["path"].toString();
            require(path.startsWith('/') && !path.contains(QChar(0)), "Local media file required",
                    "media");
            close();
            int fd = ::open(QFile::encodeName(path).constData(),
                            O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
            struct stat st;
            if (fd < 0 || ::fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_size <= 0)
            {
                if (fd >= 0)
                    ::close(fd);
                throw Failure("file-unavailable", "media",
                              "Readable nonempty regular media file required");
            }
            heldFile = fd;
            surface->arm(region.x(), region.y(), region.width(), region.height());
            surfaceArmed = true;
            handle = TPlayerCreate(CEDARX_PLAYER);
            if (!handle)
            {
                fail("player-create-failed");
                return;
            }
            callback = new Callback{this, generation.load()};
            require(TPlayerSetNotifyCallback(handle, &MediaWorker::notify, callback) == 0,
                    "Cannot install player callback", "media");
            TPlayerSetVideoDisplay(handle, true);
            status["visible"] = true;
            TPlayerSetDisplayRect(handle, region.x(), region.y(), unsigned(region.width()),
                                  unsigned(region.height()));
            const int soundResult =
                TPlayerSetVolume(handle, (params["volume"].toInt() * 40 + 50) / 100);
            status["audio_available"] = soundResult >= 0;
            status["volume"] = params["volume"];
            TPlayerSetHoldLastPicture(handle, 0);
            const auto held = QString("/proc/self/fd/%1").arg(heldFile).toUtf8();
            if (TPlayerSetDataSource(handle, held.constData(), nullptr) != 0)
            {
                fail("source-failed");
                return;
            }
            status["state"] = "preparing";
            status["error"] = QJsonValue();
            status["position_ms"] = 0;
            status["duration_ms"] = 0;
            status["first_frame"] = false;
            status["seeking"] = false;
            status["buffering"] = false;
            preparingTicks = 0;
            autostart = true;
            if (TPlayerPrepareAsync(handle) != 0)
            {
                fail("prepare-failed");
                return;
            }
            emitStatus();
            return;
        }
        if (name == "suspend")
        {
            exact(params, {});
            autostart = false;
            if (handle)
            {
                if (prepared && (status["state"] == "playing" || status["state"] == "starting"))
                {
                    if (TPlayerPause(handle) != 0)
                    {
                        fail("suspend-failed");
                        return;
                    }
                    status["state"] = "paused";
                }
                // Hide after the render thread has paused, so a queued frame
                // cannot re-enable the layer after the visibility transition.
                LayerCtrlHideVideo(handle->mLayerCtrl);
            }
            status["visible"] = false;
            emitStatus();
            return;
        }
        if (name == "visible")
        {
            exact(params, {"value"});
            require(params["value"].isBool(), "Invalid display visibility", "media");
            if (handle)
            {
                if (params["value"].toBool())
                    TPlayerSetVideoDisplay(handle, true);
                else
                    // TPlayerSetVideoDisplay(false) only changes a vendor flag;
                    // a paused decoder will never submit another frame to hide it.
                    LayerCtrlHideVideo(handle->mLayerCtrl);
            }
            status["visible"] = params["value"];
            emitStatus();
            return;
        }
        require(handle && prepared, "Player is not prepared", "media");
        if (name == "pause")
        {
            exact(params, {});
            require(TPlayerPause(handle) == 0, "Pause failed", "media");
            LayerCtrlHideVideo(handle->mLayerCtrl);
            status["state"] = "paused";
        }
        else if (name == "play")
        {
            exact(params, {});
            require(TPlayerStart(handle) == 0, "Play failed", "media");
            status["state"] = status["first_frame"] == true ? "playing" : "starting";
        }
        else if (name == "seek")
        {
            exact(params, {"position_ms"});
            require(status["seekable"] == true && status["seeking"] != true &&
                        positiveInteger(params["position_ms"], true) &&
                        params["position_ms"].toDouble() <= status["duration_ms"].toDouble() &&
                        params["position_ms"].toDouble() <= 2147483647,
                    "Invalid seek target", "media");
            require(TPlayerSeekTo(handle, params["position_ms"].toInt()) == 0, "Seek failed",
                    "media");
            status["seeking"] = true;
        }
        else if (name == "volume")
        {
            exact(params, {"value"});
            require(positiveInteger(params["value"], true) && params["value"].toDouble() <= 100,
                    "Invalid volume", "media");
            require(TPlayerSetVolume(handle, (params["value"].toInt() * 40 + 50) / 100) >= 0,
                    "Volume failed", "media");
            status["volume"] = params["value"];
        }
        else if (name == "mute")
        {
            exact(params, {"value"});
            require(params["value"].isBool(), "Invalid mute", "media");
            require(TPlayerSetAudioMute(handle, params["value"].toBool()) == 0, "Mute failed",
                    "media");
            status["muted"] = params["value"];
        }
        else if (name == "loop")
        {
            exact(params, {"value"});
            require(params["value"].isBool(), "Invalid loop", "media");
            require(TPlayerSetLooping(handle, params["value"].toBool()) == 0, "Loop setting failed",
                    "media");
            status["loop"] = params["value"];
        }
        else
            throw Failure("unknown-command", "media", "Unknown media command");
        emitStatus();
#endif
    }
    void read()
    {
        try { readRequests(); }
        catch (const std::exception &) { surfaceFailure(); }
    }
    void readRequests()
    {
        char bytes[8192];
        ssize_t n = ::read(STDIN_FILENO, bytes, sizeof(bytes));
        if (n == 0)
        {
            close();
            QCoreApplication::quit();
            return;
        }
        if (n < 0)
            return;
        input.append(bytes, int(n));
        if (input.size() > 65536)
        {
            close();
            QCoreApplication::exit(2);
            return;
        }
        int end;
        while ((end = input.indexOf('\n')) >= 0)
        {
            const auto bytes = input.left(end);
            input.remove(0, end + 1);
            QString requestId, commandName;
            try
            {
                auto value = strictJson(bytes);
                require(value.isObject(), "Media request object required");
                const auto root = value.toObject();
                exact(root, {"protocol", "request_id", "command", "params"});
                require(root["protocol"] == 1 && root["request_id"].isString() &&
                            root["request_id"].toString().size() <= 64 &&
                            root["command"].isString() && root["params"].isObject(),
                        "Invalid media envelope");
                requestId = root["request_id"].toString();
                commandName = root["command"].toString();
                command(commandName, root["params"].toObject());
                send({{"event", "reply"}, {"request_id", requestId}, {"accepted", true}});
            }
            catch (const Failure &error)
            {
                if (commandName == "open")
                    fail(error.code);
                send({{"event", "reply"},
                      {"request_id", requestId},
                      {"accepted", false},
                      {"error", failure(error.code)}});
            }
        }
    }
};
} // namespace
int main(int argc, char **argv)
{
    const int protocol = ::dup(STDOUT_FILENO);
    if (protocol < 0 || ::dup2(STDERR_FILENO, STDOUT_FILENO) < 0)
        return 2;
    const pid_t parent = ::getppid();
    if (parent == 1)
        return 2;
    struct sigaction action{};
    action.sa_handler = requestTermination;
    ::sigemptyset(&action.sa_mask);
    if (::sigaction(SIGTERM, &action, nullptr) || ::sigaction(SIGINT, &action, nullptr) ||
        ::prctl(PR_SET_PDEATHSIG, SIGTERM) || ::getppid() != parent)
        return 2;
    try
    {
        std::unique_ptr<MediaSurfaceGuardian> surface;
#ifdef DSHAN_CEDAR_PLAYER
        const auto heartbeat = qEnvironmentVariable("DSHANCONTROL_HMI_HEARTBEAT");
        const auto runtime = heartbeat.isEmpty() ? QString("/run/dshancontrol/hmi")
                                                : QFileInfo(heartbeat).absolutePath();
        // Must fork before QCoreApplication or TPlayer creates any threads.
        surface.reset(new MediaSurfaceGuardian(QFile::encodeName(runtime).constData(),
                                                cedarSurfaceBackend()));
#endif
        QCoreApplication app(argc, argv);
        MediaWorker player(protocol, surface.get());
        QTimer termination;
        termination.setInterval(50);
        QObject::connect(&termination, &QTimer::timeout, &app, [&]() {
            if (terminationRequested)
            {
                try { player.close(); app.quit(); }
                catch (const std::exception &) { player.surfaceFailure(); }
            }
        });
        termination.start();
        return app.exec();
    }
    catch (const Failure &) { return 1; }
    catch (const std::exception &error)
    {
        // Diagnostics remain on stderr; never contaminate the structured pipe.
        ::dprintf(STDERR_FILENO, "Media ownership failure: %s\n", error.what());
        return 1;
    }
}
