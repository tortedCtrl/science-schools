#include "api/http_server.hpp"

#include <spdlog/spdlog.h>

#include <boost/beast.hpp>
#include <chrono>
#include <string>
#include <string_view>
#include <utility>

#include "common/async.hpp"
#include "common/url.hpp"

namespace schools::api {
namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;

namespace {

constexpr auto kIdleTimeout{std::chrono::seconds(30)};
constexpr std::size_t kMaxRequestBytes{64UZ * 1024};

Request ToRequest(const http::request<http::string_body>& raw) {
  const std::string_view target{raw.target().data(), raw.target().size()};
  const auto question{target.find('?')};
  Request request;
  request.method = std::string(raw.method_string());
  request.path = UrlDecode(target.substr(0, question));
  if (question != std::string_view::npos) {
    request.query = ParseQueryString(target.substr(question + 1));
  }
  return request;
}

http::response<http::string_body> ToHttpResponse(unsigned version,
                                                 bool keep_alive,
                                                 Response response) {
  http::response<http::string_body> result{
      static_cast<http::status>(response.status), version};
  result.set(http::field::server, "scientific-schools");
  result.set(http::field::content_type, response.content_type);
  result.set(http::field::access_control_allow_origin, "*");
  result.set(http::field::access_control_allow_methods, "GET, OPTIONS");
  result.set(http::field::access_control_allow_headers, "Content-Type");
  result.keep_alive(keep_alive);
  result.body() = std::move(response.body);
  result.prepare_payload();
  return result;
}

bool IsExpectedDisconnect(const boost::system::error_code& ec) {
  return ec == http::error::end_of_stream ||
         ec == asio::error::operation_aborted || ec == beast::error::timeout ||
         ec == asio::error::connection_reset || ec == asio::error::eof;
}

}  // namespace

Status HttpServer::Start() {
  boost::system::error_code ec;
  const auto address{asio::ip::make_address(options_.address, ec)};
  if (ec) {
    return std::unexpected(
        FromBoost(Errc::kInvalidArgument, "HTTP address", ec));
  }
  const asio::ip::tcp::endpoint endpoint{address, options_.port};

  acceptor_.open(endpoint.protocol(), ec);
  if (ec) {
    return std::unexpected(FromBoost(Errc::kNetwork, "acceptor open", ec));
  }
  acceptor_.set_option(asio::socket_base::reuse_address(true), ec);
  acceptor_.bind(endpoint, ec);
  if (ec) {
    return std::unexpected(FromBoost(Errc::kNetwork, "acceptor bind", ec));
  }
  acceptor_.listen(asio::socket_base::max_listen_connections, ec);
  if (ec) {
    return std::unexpected(FromBoost(Errc::kNetwork, "acceptor listen", ec));
  }

  spdlog::info("HTTP server listening on {}:{}", options_.address,
               options_.port);
  asio::co_spawn(ioc_, Accept(), asio::detached);
  return {};
}

asio::awaitable<void> HttpServer::Accept() {
  while (true) {
    auto [ec, socket] = co_await acceptor_.async_accept(kAsTuple);
    if (ec) {
      if (ec == asio::error::operation_aborted) {
        co_return;
      }
      spdlog::warn("accept failed: {}", ec.message());
      continue;
    }
    asio::co_spawn(ioc_, Session(std::move(socket)), asio::detached);
  }
}

asio::awaitable<void> HttpServer::Session(asio::ip::tcp::socket socket) {
  beast::tcp_stream stream{std::move(socket)};
  beast::flat_buffer buffer;
  while (true) {
    http::request_parser<http::string_body> parser;
    parser.body_limit(kMaxRequestBytes);
    stream.expires_after(kIdleTimeout);
    auto [read_ec, read_bytes] =
        co_await http::async_read(stream, buffer, parser, kAsTuple);
    (void)read_bytes;
    if (read_ec) {
      if (!IsExpectedDisconnect(read_ec)) {
        spdlog::debug("HTTP read error: {}", read_ec.message());
      }
      break;
    }
    const auto raw{parser.release()};
    const bool keep_alive{raw.keep_alive()};

    Response response = co_await router_.Handle(ToRequest(raw));

    auto [write_ec, write_bytes] = co_await http::async_write(
        stream, ToHttpResponse(raw.version(), keep_alive, std::move(response)),
        kAsTuple);
    (void)write_bytes;
    if (write_ec) {
      if (!IsExpectedDisconnect(write_ec)) {
        spdlog::debug("HTTP write error: {}", write_ec.message());
      }
      break;
    }
    if (!keep_alive) {
      break;
    }
  }
  beast::error_code ignored;
  stream.socket().shutdown(asio::ip::tcp::socket::shutdown_send, ignored);
}

}  // namespace schools::api
