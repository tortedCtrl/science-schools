#pragma once

#include <boost/asio.hpp>
#include <cstdint>
#include <string>

#include "api/router.hpp"

namespace schools::api {

struct ServerOptions {
  std::string address{"0.0.0.0"};
  std::uint16_t port{8080};
};

class HttpServer {
 public:
  HttpServer(boost::asio::io_context& ioc, ServerOptions options,
             const Router& router)
      : ioc_{ioc}, options_{std::move(options)}, router_{router} {}

  Status Start();

 private:
  boost::asio::awaitable<void> Accept();
  boost::asio::awaitable<void> Session(boost::asio::ip::tcp::socket socket);

  boost::asio::io_context& ioc_;
  ServerOptions options_;
  const Router& router_;
  boost::asio::ip::tcp::acceptor acceptor_{ioc_};
};

}  // namespace schools::api
