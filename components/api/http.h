#pragma once
#include "../native/common.h"
#include <QTcpServer>
#include <QThreadPool>
#include <QUrl>
#include <atomic>
#include <functional>
struct Request {
  QByteArray method, body;
  QUrl url;
  QMap<QByteArray, QByteArray> headers;
};
struct Response {
  int status = 200;
  QByteArray body, type = "application/json; charset=utf-8";
  QMap<QByteArray, QByteArray> headers;
  Response() = default;
  Response(const QJsonObject &value, int status = 200)
      : status(status),
        body(QJsonDocument(value).toJson(QJsonDocument::Compact)) {}
};
class HttpServer : public QTcpServer {
  QThreadPool workers;
  std::atomic<int> active{0};
  std::function<Response(const Request &)> handler;
  void incomingConnection(qintptr descriptor) override;

public:
  explicit HttpServer(std::function<Response(const Request &)> handler);
  ~HttpServer() override;
};
