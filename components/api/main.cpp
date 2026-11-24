#include "api.h"
#include <QTimer>
#include <csignal>
#include <sys/stat.h>
namespace {
volatile sig_atomic_t stopping = 0;
void stop(int) { stopping = 1; }
} // namespace
int main(int argc, char **argv) {
  ::umask(0077);
  QCoreApplication application(argc, argv);
  try {
    const auto address = Native::env("OMNIGATE_LISTEN_ADDRESS", "127.0.0.1");
    Native::check(address == "127.0.0.1",
                  "Native local API must bind IPv4 loopback");
    auto port = Native::integer(Native::env("OMNIGATE_LISTEN_PORT", "8080"),
                                "port", 1, 65535);
    Api api;
    HttpServer server([&](const Request &r) { return api.handle(r); });
    Native::check(server.listen(QHostAddress::LocalHost, port),
                  "Cannot listen on local API port");
    std::signal(SIGTERM, stop);
    std::signal(SIGINT, stop);
    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, &application, [&] {
      if (stopping)
        application.quit();
    });
    timer.start(100);
    Native::output(
        {{"ready", true}, {"backend", "native-cpp"}, {"port", int(port)}});
    return application.exec();
  } catch (const Failure &e) {
    Native::output({{"ok", false}, {"error", e.detail}});
    return 1;
  }
}
