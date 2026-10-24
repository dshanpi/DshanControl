
// Appended to the real worker translation unit by test_media_close_state.py.
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const int sink = ::open("/dev/null", O_WRONLY | O_CLOEXEC);
    if (sink < 0 || argc != 2) return 2;
    MediaWorker player(sink, nullptr);
    player.status["state"] = "paused";
    player.status["seeking"] = true;
    player.status["buffering"] = true;
    player.status["seekable"] = true;
    player.status["position_ms"] = 18888;
    player.status["duration_ms"] = 27052;
    const int generation = player.generation.load();
    const QString mode = QString::fromLocal8Bit(argv[1]);
    if (mode == "invalid") {
        bool rejected = false;
        try { player.command("close", {{"unexpected", true}}); }
        catch (const Failure &) { rejected = true; }
        return rejected && player.generation == generation &&
               player.status["seeking"] == true && player.status["buffering"] == true ? 0 : 3;
    }
    if (mode == "error") player.fail("fixture-error");
    else player.command("close", {});
    if (mode == "repeat") player.command("close", {});
    if (player.status["seeking"] != false || player.status["buffering"] != false ||
        player.status["seekable"] != false) {
        ::dprintf(STDERR_FILENO, "Cancelled media still reports pending/capable seek or buffering\n");
        return 4;
    }
    if (player.generation <= generation) return 5;
    if (mode == "error")
        return player.status["state"] == "error" && player.status["error"] == "fixture-error" ? 0 : 6;
    return player.status["state"] == "idle" && player.status["position_ms"] == 0 &&
           player.status["duration_ms"] == 0 ? 0 : 7;
}
