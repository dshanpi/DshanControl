#include "http.h"
#include <QRunnable>
#include <QTcpSocket>
using namespace Native;
namespace {
void httpRequire(bool ok, const QString &message, int code = 400) {
  if (!ok)
    throw Failure(QString::number(code), "http", message);
}
Request readRequest(QTcpSocket &socket) {
  QByteArray data;
  QElapsedTimer deadline;
  deadline.start();
  int end = -1;
  auto receive = [&] {
    httpRequire(deadline.elapsed() < 10000, "HTTP read deadline exceeded", 408);
    socket.waitForReadyRead(qMin(200, 10000 - int(deadline.elapsed())));
    data += socket.readAll();
    httpRequire(data.size() <= 2 * 1024 * 1024 + 16384,
                "HTTP request too large", 413);
    httpRequire(socket.state() != QAbstractSocket::UnconnectedState,
                "Incomplete HTTP request");
  };
  while ((end = data.indexOf("\r\n\r\n")) < 0) {
    httpRequire(data.size() <= 16384, "HTTP headers too large", 431);
    receive();
  }
  httpRequire(end <= 16384, "HTTP headers too large", 431);
  auto lines = data.left(end).split('\n');
  httpRequire(!lines.isEmpty(), "Missing HTTP request line");
  auto first = lines.takeFirst().trimmed().split(' ');
  httpRequire(first.size() == 3 &&
                  (first[2] == "HTTP/1.1" || first[2] == "HTTP/1.0"),
              "Invalid HTTP request line");
  httpRequire(
      QList<QByteArray>{"GET", "HEAD", "POST", "PUT", "DELETE"}.contains(
          first[0]),
      "HTTP method unsupported", 405);
  httpRequire(first[1].startsWith('/') && !first[1].startsWith("//") &&
                  first[1].size() <= 4096,
              "Invalid HTTP target");
  Request request;
  request.method = first[0];
  request.url = QUrl::fromEncoded(first[1], QUrl::StrictMode);
  httpRequire(request.url.isValid() && !request.url.path().contains(QChar(0)) &&
                  !request.url.path().contains('\\'),
              "Invalid URL");
  for (auto line : lines) {
    httpRequire(!line.startsWith(' ') && !line.startsWith('\t'),
                "Folded headers rejected");
    line = line.trimmed();
    int colon = line.indexOf(':');
    httpRequire(colon > 0, "Invalid HTTP header");
    auto key = line.left(colon).toLower(),
         value = line.mid(colon + 1).trimmed();
    httpRequire(QRegularExpression("^[a-z0-9!#$%&'*+.^_`|~-]+$")
                        .match(QString::fromLatin1(key))
                        .hasMatch() &&
                    !request.headers.contains(key),
                "Duplicate or invalid header");
    httpRequire(!value.contains('\r') && !value.contains('\n'),
                "Invalid header value");
    request.headers[key] = value;
  }
  httpRequire(!request.headers.contains("transfer-encoding") &&
                  !request.headers.contains("expect"),
              "Transfer encoding/Expect unsupported");
  httpRequire(QRegularExpression(
                  "^(?:127\\.0\\.0\\.1|localhost|\\[::1\\])(?::[0-9]{1,5})?$")
                  .match(QString::fromLatin1(request.headers.value("host")))
                  .hasMatch(),
              "Loopback Host required", 403);
  if (request.headers.contains("origin")) {
    QUrl origin(QString::fromUtf8(request.headers["origin"]));
    httpRequire(origin.scheme() == "http" &&
                    origin.authority().toLatin1() ==
                        request.headers.value("host") &&
                    origin.path().isEmpty() && !origin.hasQuery() &&
                    !origin.hasFragment(),
                "Cross-origin request rejected", 403);
  }
  httpRequire(request.headers.value("sec-fetch-site") != "cross-site",
              "Cross-site request rejected", 403);
  qint64 size = 0;
  if (request.headers.contains("content-length")) {
    auto raw = request.headers["content-length"];
    bool ok;
    size = raw.toLongLong(&ok);
    httpRequire(ok &&
                    QRegularExpression("^[0-9]+$")
                        .match(QString::fromLatin1(raw))
                        .hasMatch() &&
                    size >= 0 && size <= 2 * 1024 * 1024,
                "Invalid body length", 413);
  }
  while (data.size() < end + 4 + size)
    receive();
  httpRequire(data.size() == end + 4 + size,
              "Pipelined or ambiguous request rejected");
  request.body = data.mid(end + 4, size);
  return request;
}
class Job : public QRunnable {
  std::function<void()> fn;

public:
  explicit Job(std::function<void()> f) : fn(std::move(f)) {}
  void run() override { fn(); }
};
} // namespace
HttpServer::HttpServer(std::function<Response(const Request &)> callback)
    : handler(std::move(callback)) {
  workers.setMaxThreadCount(4);
  setMaxPendingConnections(16);
}
HttpServer::~HttpServer() {
  close();
  workers.waitForDone();
}
void HttpServer::incomingConnection(qintptr descriptor) {
  if (active.fetch_add(1) >= 16) {
    active.fetch_sub(1);
    ::close(descriptor);
    return;
  }
  workers.start(new Job([this, descriptor] {
    QTcpSocket socket;
    Response response;
    bool head = false;
    if (!socket.setSocketDescriptor(descriptor)) {
      ::close(descriptor);
      active.fetch_sub(1);
      return;
    }
    try {
      auto request = readRequest(socket);
      head = request.method == "HEAD";
      response = handler(request);
    } catch (const Failure &e) {
      bool numeric = false;
      int code = e.stage == "http" ? e.code.toInt(&numeric) : 400;
      response =
          Response({{"ok", false}, {"error", e.detail}}, numeric ? code : 400);
    } catch (const std::exception &) {
      response = Response({{"ok", false}, {"error", "内部服务错误"}}, 500);
    }
    QByteArray result = "HTTP/1.1 " + QByteArray::number(response.status) +
                        " " + (response.status < 400 ? "OK" : "Error") + "\r\n";
    response.headers["Content-Type"] = response.type;
    response.headers["Content-Length"] =
        QByteArray::number(response.body.size());
    response.headers["Connection"] = "close";
    response.headers["X-Content-Type-Options"] = "nosniff";
    response.headers["X-Frame-Options"] = "DENY";
    response.headers["Referrer-Policy"] = "no-referrer";
    response.headers["Content-Security-Policy"] =
        "default-src 'self'; style-src 'self' 'unsafe-inline'; script-src "
        "'self' 'unsafe-inline'; img-src 'self' data:; connect-src 'self'; "
        "frame-ancestors 'none'; base-uri 'self'";
    response.headers["Cache-Control"] = "no-store";
    for (auto i = response.headers.begin(); i != response.headers.end(); ++i)
      result += i.key() + ": " + i.value() + "\r\n";
    result += "\r\n";
    if (!head)
      result += response.body;
    socket.write(result);
    QElapsedTimer timer;
    timer.start();
    while (socket.bytesToWrite() > 0 && timer.elapsed() < 5000)
      if (!socket.waitForBytesWritten(500))
        break;
    socket.disconnectFromHost();
    active.fetch_sub(1);
  }));
}
