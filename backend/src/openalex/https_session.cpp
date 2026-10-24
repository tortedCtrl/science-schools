#include "openalex/https_session.hpp"

#include <openssl/ssl.h>

#include <chrono>
#include <utility>

#include "common/async.hpp"

namespace schools::openalex {
namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;

namespace {
constexpr auto kConnectTimeout{std::chrono::seconds(30)};
constexpr auto kRequestTimeout{std::chrono::seconds(60)};
constexpr std::size_t kMaxBodyBytes{256UZ * 1024 * 1024};
}  // namespace

HttpsSession::HttpsSession(asio::any_io_executor executor,
                           asio::ssl::context& ssl_context, std::string host,
                           std::string user_agent)
    : executor_{std::move(executor)},
      ssl_context_{ssl_context},
      host_{std::move(host)},
      user_agent_{std::move(user_agent)} {}

asio::awaitable<Status> HttpsSession::Connect() {
  stream_.emplace(executor_, ssl_context_);
  if (SSL_set_tlsext_host_name(stream_->native_handle(), host_.c_str()) != 1) {
    co_return MakeError(Errc::kNetwork, "Failed to set TLS SNI host name");
  }
  boost::system::error_code verify_ec;
  stream_->set_verify_callback(asio::ssl::host_name_verification(host_),
                               verify_ec);
  if (verify_ec) {
    co_return std::unexpected(
        FromBoost(Errc::kNetwork, "TLS verify setup", verify_ec));
  }

  asio::ip::tcp::resolver resolver(executor_);
  const auto [resolve_ec, endpoints] =
      co_await resolver.async_resolve(host_, "443", kAsTuple);
  if (resolve_ec) {
    co_return std::unexpected(
        FromBoost(Errc::kNetwork, "DNS resolve " + host_, resolve_ec));
  }

  beast::get_lowest_layer(*stream_).expires_after(kConnectTimeout);
  const auto [connect_ec, endpoint] =
      co_await beast::get_lowest_layer(*stream_).async_connect(endpoints,
                                                               kAsTuple);
  if (connect_ec) {
    co_return std::unexpected(
        FromBoost(Errc::kNetwork, "TCP connect " + host_, connect_ec));
  }

  const auto [handshake_ec] = co_await stream_->async_handshake(
      asio::ssl::stream_base::client, kAsTuple);
  if (handshake_ec) {
    co_return std::unexpected(
        FromBoost(Errc::kNetwork, "TLS handshake " + host_, handshake_ec));
  }
  co_return Status{};
}

asio::awaitable<Result<HttpsSession::Response>> HttpsSession::Perform(
    std::string target) {
  if (!stream_) {
    auto connected = co_await Connect();
    if (!connected) {
      co_return std::unexpected(connected.error());
    }
  }

  http::request<http::empty_body> request{http::verb::get, target, 11};
  request.set(http::field::host, host_);
  request.set(http::field::user_agent, user_agent_);
  request.set(http::field::accept, "application/json");

  beast::get_lowest_layer(*stream_).expires_after(kRequestTimeout);
  const auto [write_ec, written] =
      co_await http::async_write(*stream_, request, kAsTuple);
  if (write_ec) {
    co_return std::unexpected(
        FromBoost(Errc::kNetwork, "HTTP write", write_ec));
  }

  beast::flat_buffer buffer;
  http::response_parser<http::string_body> parser;
  parser.body_limit(kMaxBodyBytes);
  const auto [read_ec, read] =
      co_await http::async_read(*stream_, buffer, parser, kAsTuple);
  if (read_ec) {
    co_return std::unexpected(FromBoost(Errc::kNetwork, "HTTP read", read_ec));
  }
  co_return parser.release();
}

asio::awaitable<Result<HttpsSession::Response>> HttpsSession::Get(
    std::string target) {
  auto response = co_await Perform(std::move(target));
  if (!response || !response->keep_alive()) {
    stream_.reset();  // force a fresh connection for the next request
  }
  co_return response;
}

}  // namespace schools::openalex
