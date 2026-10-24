#pragma once

#include <algorithm>
#include <boost/asio.hpp>
#include <boost/mysql.hpp>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "common/async.hpp"
#include "common/error.hpp"

namespace schools::repository {

inline constexpr std::size_t kBulkChunkRows{2000};

// Wraps a Boost.MySQL error (+ server diagnostics) into our Error type.
Error DbError(const boost::system::error_code& ec,
              const boost::mysql::diagnostics& diag);

// Executes one parameterless statement.
boost::asio::awaitable<Status> ExecSimple(boost::mysql::any_connection* conn,
                                          std::string_view sql);

// Exception-free field accessors. NULL or an unexpected type yields a neutral
// default; the SELECT lists in the repositories fix the column types.
std::int64_t AsInt(const boost::mysql::field_view& field);
float AsFloat(const boost::mysql::field_view& field);
std::string AsString(const boost::mysql::field_view& field);
std::optional<std::string> AsOptionalString(
    const boost::mysql::field_view& field);

// Multi-row INSERT in chunks of kBulkChunkRows rows: one round trip per chunk
// instead of one per row. `sql` must contain a single "{}" placeholder for the
// VALUES list; `formatter` writes one "(...)" tuple with
// boost::mysql::format_sql_to (client-side escaping, injection-safe).
template <typename Row, typename Formatter>
boost::asio::awaitable<Status> BulkExecute(
    boost::mysql::any_connection* conn, boost::mysql::constant_string_view sql,
    std::span<const Row> rows, Formatter formatter) {
  for (std::size_t offset = 0; offset < rows.size(); offset += kBulkChunkRows) {
    const auto chunk{
        rows.subspan(offset, std::min(kBulkChunkRows, rows.size() - offset))};
    boost::mysql::results result;
    boost::mysql::diagnostics diag;
    const auto [ec] = co_await conn->async_execute(
        boost::mysql::with_params(sql,
                                  boost::mysql::sequence(chunk, formatter)),
        result, diag, kAsTuple);
    if (ec) {
      co_return std::unexpected(DbError(ec, diag));
    }
  }
  co_return Status{};
}

}  // namespace schools::repository
