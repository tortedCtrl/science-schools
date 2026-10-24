#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <optional>
#include <string>

#include "common/error.hpp"

namespace schools::openalex {

// A single keep-alive HTTPS connection. NOT thread-safe: one session per
// coroutine. Reconnects lazily after errors or "Connection: close".
class HttpsSession {
 public:
  using Response =
      boost::beast::http::response<boost::beast::http::string_body>;

  HttpsSession(boost::asio::any_io_executor executor,
               boost::asio::ssl::context& ssl_context, std::string host,
               std::string user_agent);

  // Network and TLS failures are returned as Errc::kNetwork; any HTTP status
  // (including 4xx/5xx) is a successful transfer and is left to the caller.
  boost::asio::awaitable<Result<Response>> Get(std::string target);

 private:
  boost::asio::awaitable<Status> Connect();
  boost::asio::awaitable<Result<Response>> Perform(std::string target);

  boost::asio::any_io_executor executor_;
  boost::asio::ssl::context& ssl_context_;
  std::string host_;
  std::string user_agent_;
  std::optional<boost::beast::ssl_stream<boost::beast::tcp_stream>> stream_;
};

}  // namespace schools::openalex
