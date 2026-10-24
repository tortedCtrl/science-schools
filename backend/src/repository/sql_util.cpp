#include "repository/sql_util.hpp"

#include <fmt/format.h>

namespace schools::repository {

Error DbError(const boost::system::error_code& ec,
              const boost::mysql::diagnostics& diag) {
  std::string message{ec.message()};
  if (!diag.server_message().empty()) {
    message += fmt::format(" (server: {})", diag.server_message());
  }
  if (!diag.client_message().empty()) {
    message += fmt::format(" (client: {})", diag.client_message());
  }
  return Error{Errc::kDatabase, std::move(message)};
}

boost::asio::awaitable<Status> ExecSimple(boost::mysql::any_connection* conn,
                                          std::string_view sql) {
  boost::mysql::results result;
  boost::mysql::diagnostics diag;
  const auto [ec] = co_await conn->async_execute(sql, result, diag, kAsTuple);
  if (ec) {
    co_return std::unexpected(DbError(ec, diag));
  }
  co_return Status{};
}

std::int64_t AsInt(const boost::mysql::field_view& field) {
  if (field.is_int64()) {
    return field.get_int64();
  }
  if (field.is_uint64()) {
    return static_cast<std::int64_t>(field.get_uint64());
  }
  return 0;
}

float AsFloat(const boost::mysql::field_view& field) {
  if (field.is_float()) {
    return field.get_float();
  }
  if (field.is_double()) {
    return static_cast<float>(field.get_double());
  }
  return 0.0F;
}

std::string AsString(const boost::mysql::field_view& field) {
  return field.is_string() ? std::string(field.get_string()) : std::string{};
}

std::optional<std::string> AsOptionalString(
    const boost::mysql::field_view& field) {
  if (!field.is_string()) {
    return std::nullopt;
  }
  return std::string(field.get_string());
}

}  // namespace schools::repository
