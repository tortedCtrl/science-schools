#include "repository/author_repository.hpp"

#include "repository/sql_util.hpp"

namespace schools::repository {
namespace asio = boost::asio;
namespace mysql = boost::mysql;

asio::awaitable<Result<std::optional<domain::Author>>> AuthorRepository::Find(
    std::int64_t openalex_id) {
  auto lease = co_await db_.Acquire();
  if (!lease) {
    co_return std::unexpected(lease.error());
  }
  mysql::any_connection& conn{lease->get()};

  mysql::results result;
  mysql::diagnostics diag;
  const auto [ec] = co_await conn.async_execute(
      mysql::with_params(
          "SELECT openalex_id, orcid, name FROM authors WHERE openalex_id = {}",
          openalex_id),
      result, diag, kAsTuple);
  if (ec) {
    co_return std::unexpected(DbError(ec, diag));
  }

  if (result.rows().empty()) {
    co_return std::optional<domain::Author>{};
  }
  const mysql::row_view row{result.rows()[0]};
  domain::Author author;
  author.openalex_id = AsInt(row[0]);
  author.orcid = AsOptionalString(row[1]);
  author.name = AsString(row[2]);
  co_return author;
}

asio::awaitable<Result<std::vector<domain::FieldCount>>>
AuthorRepository::TopFields(std::int64_t openalex_id, int limit) {
  auto lease = co_await db_.Acquire();
  if (!lease) {
    co_return std::unexpected(lease.error());
  }
  mysql::any_connection& conn{lease->get()};

  mysql::results result;
  mysql::diagnostics diag;
  const auto [ec] = co_await conn.async_execute(
      mysql::with_params(
          "SELECT f.id, f.name, af.works_count "
          "FROM author_fields af JOIN fields f ON f.id = af.field_id "
          "WHERE af.author_id = {} ORDER BY af.works_count DESC LIMIT {}",
          openalex_id, limit),
      result, diag, kAsTuple);
  if (ec) {
    co_return std::unexpected(DbError(ec, diag));
  }

  std::vector<domain::FieldCount> fields;
  for (const mysql::row_view row : result.rows()) {
    fields.push_back({static_cast<std::int32_t>(AsInt(row[0])),
                      AsString(row[1]),
                      static_cast<std::int32_t>(AsInt(row[2]))});
  }
  co_return fields;
}

}  // namespace schools::repository
