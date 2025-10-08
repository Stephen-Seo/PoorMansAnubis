// ISC License
//
// Copyright (c) 2025 Stephen Seo
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
// OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THIS SOFTWARE.

#ifndef SEODISPARATE_COM_POOR_MANS_ANUBIS_SQL_DB_H_
#define SEODISPARATE_COM_POOR_MANS_ANUBIS_SQL_DB_H_

// standard library includes
#include <cstdint>
#include <cstring>
#include <format>
#include <mutex>
#include <optional>
#include <print>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_set>
#include <vector>

// third party includes
#include <sqlite3.h>

constexpr int ALLOWED_IP_TIMEOUT_MINUTES = 30;
constexpr int CHALLENGE_TIMEOUT_MINUTES = 3;

namespace PMA_SQL {
enum class ErrorT {
  SUCCESS,
  FAILED_TO_OPEN_DB,
  FAILED_TO_INIT_DB,
  DB_ALREADY_FAILED_TO_INIT,
  FAILED_TO_FETCH_FROM_SEQ_ID,
  FAILED_TO_PREPARE_EXEC_GENERIC,
  EXEC_GENERIC_INVALID_STATE,
  CLIENT_IP_DOES_NOT_MATCH_STORED_IP,
  FAILED_TO_FETCH_FROM_ALLOWED_IPS,
  FAILED_TO_FETCH_FROM_ID_TO_PORT
};

std::string error_t_to_string(ErrorT err);

class SQLITECtx {
 public:
  SQLITECtx();
  SQLITECtx(std::string sqlite_path);
  ~SQLITECtx();

  SQLITECtx(const SQLITECtx &) = delete;
  SQLITECtx *operator=(const SQLITECtx &) = delete;

  SQLITECtx(SQLITECtx &&);
  SQLITECtx *operator=(SQLITECtx &&);

  void *get_ctx() const;

  sqlite3 *get_sqlite_ctx() const;

  std::mutex &get_mutex();

  std::lock_guard<std::mutex> get_mutex_lock_guard();

 private:
  std::mutex mutex;
  void *ctx;
};

void exec_sqlite_stmt_str_cleanup(void *ud);

template <unsigned long long IDX>
std::tuple<ErrorT, std::string> exec_sqlite_statement(
    const SQLITECtx &ctx, std::string stmt,
    std::optional<sqlite3_stmt *> sqli3_stmt);

template <unsigned long long IDX, typename Arg, typename... Args>
std::tuple<ErrorT, std::string> exec_sqlite_statement(
    const SQLITECtx &ctx, std::string stmt,
    std::optional<sqlite3_stmt *> sqli3_stmt, Arg arg, Args... args);

template <typename... SArgs>
struct SqliteStmtRow {
  std::tuple<std::optional<SArgs>...> row;

  template <unsigned long long IDX>
  void print_row() const;

  template <unsigned long long IDX>
  static std::tuple<ErrorT, std::string,
                    std::optional<std::vector<SqliteStmtRow<SArgs...>>>>
  exec_sqlite_stmt_fetch_rows(const SQLITECtx &ctx, std::string stmt,
                              std::optional<sqlite3_stmt *> sqli3_stmt,
                              std::vector<SqliteStmtRow<SArgs...>> v);

  template <unsigned long long IDX, typename Arg, typename... Args>
  static std::tuple<ErrorT, std::string,
                    std::optional<std::vector<SqliteStmtRow<SArgs...>>>>
  exec_sqlite_stmt_fetch_rows(const SQLITECtx &ctx, std::string stmt,
                              std::optional<sqlite3_stmt *> sqli3_stmt,
                              std::vector<SqliteStmtRow<SArgs...>> v);

  template <unsigned long long IDX>
  static std::tuple<ErrorT, std::string,
                    std::optional<std::vector<SqliteStmtRow<SArgs...>>>>
  exec_sqlite_stmt_with_rows(const SQLITECtx &ctx, std::string stmt,
                             std::optional<sqlite3_stmt *> sqli3_stmt);

  template <unsigned long long IDX, typename Arg, typename... Args>
  static std::tuple<ErrorT, std::string,
                    std::optional<std::vector<SqliteStmtRow<SArgs...>>>>
  exec_sqlite_stmt_with_rows(const SQLITECtx &ctx, std::string stmt,
                             std::optional<sqlite3_stmt *> sqli3_stmt, Arg arg,
                             Args... args);
};

// string is err message.
std::tuple<SQLITECtx, ErrorT, std::string> init_sqlite(std::string filepath);

// string is err message.
std::tuple<ErrorT, std::string> cleanup_stale_id_to_ports(const SQLITECtx &ctx);

// string is err message.
std::tuple<ErrorT, std::string> cleanup_stale_challenges(const SQLITECtx &ctx);

// string is err message.
std::tuple<ErrorT, std::string> cleanup_stale_entries(const SQLITECtx &ctx);

// last string is id.
std::tuple<ErrorT, std::string, std::string> init_id_to_port(SQLITECtx &ctx,
                                                             uint16_t port);

// On error, first string is err message. On SUCCESS, first string is challenge
// in base64 and second string is hashed answer.
// uint64_t is id.
std::tuple<ErrorT, std::string, std::string, uint64_t> generate_challenge(
    SQLITECtx &ctx, uint64_t digits, std::string client_ip, std::string id);

// string is error msg, uint16_t is destination port of initial challenge
// generation request.
std::tuple<ErrorT, std::string, uint16_t> verify_answer(SQLITECtx &ctx,
                                                        std::string answer,
                                                        std::string ipaddr,
                                                        uint64_t id);

std::tuple<ErrorT, std::string, std::unordered_set<uint16_t>>
get_allowed_ip_ports(SQLITECtx &ctx, std::string ipaddr);
}  // namespace PMA_SQL

////////////////////////////////////////////////////////////////////////////////
// templated implementations
////////////////////////////////////////////////////////////////////////////////

template <unsigned long long IDX>
std::tuple<PMA_SQL::ErrorT, std::string> PMA_SQL::exec_sqlite_statement(
    const PMA_SQL::SQLITECtx &ctx, std::string stmt,
    std::optional<sqlite3_stmt *> sqli3_stmt) {
  if (!sqli3_stmt.has_value()) {
    return {PMA_SQL::ErrorT::EXEC_GENERIC_INVALID_STATE,
            "sqli3_stmt is nullopt"};
  }

  int ret = sqlite3_step(sqli3_stmt.value());
  if (ret != SQLITE_OK && ret != SQLITE_DONE) {
    sqlite3_finalize(sqli3_stmt.value());
    return {PMA_SQL::ErrorT::EXEC_GENERIC_INVALID_STATE,
            "Failed to step generic exec stmt"};
  }

  ret = sqlite3_finalize(sqli3_stmt.value());
  if (ret != SQLITE_OK) {
    return {PMA_SQL::ErrorT::EXEC_GENERIC_INVALID_STATE,
            "Failed to finalize generic exec stmt"};
  }

  return {PMA_SQL::ErrorT::SUCCESS, "Success"};
}

template <unsigned long long IDX, typename Arg, typename... Args>
std::tuple<PMA_SQL::ErrorT, std::string> PMA_SQL::exec_sqlite_statement(
    const PMA_SQL::SQLITECtx &ctx, std::string stmt,
    std::optional<sqlite3_stmt *> sqli3_stmt, Arg arg, Args... args) {
  if (sqli3_stmt.has_value()) {
    if constexpr (std::is_integral_v<Arg>) {
      if (sizeof(Arg) > 4) {
        int ret = sqlite3_bind_int64(sqli3_stmt.value(), IDX, arg);
        if (ret != SQLITE_OK) {
          sqlite3_finalize(sqli3_stmt.value());
          return {PMA_SQL::ErrorT::FAILED_TO_PREPARE_EXEC_GENERIC,
                  "Bind int64 failed"};
        }
        return exec_sqlite_statement<IDX + 1, Args...>(ctx, stmt, sqli3_stmt,
                                                       args...);
      } else {
        int ret = sqlite3_bind_int(sqli3_stmt.value(), IDX, arg);
        if (ret != SQLITE_OK) {
          sqlite3_finalize(sqli3_stmt.value());
          return {PMA_SQL::ErrorT::FAILED_TO_PREPARE_EXEC_GENERIC,
                  "Bind int failed"};
        }
        return exec_sqlite_statement<IDX + 1, Args...>(ctx, stmt, sqli3_stmt,
                                                       args...);
      }
    } else if constexpr (std::is_same_v<Arg, std::string>) {
      char *buf = new char[arg.size() + 1];
      std::memcpy(buf, arg.c_str(), arg.size() + 1);
      int ret = sqlite3_bind_text(sqli3_stmt.value(), IDX, buf, arg.size(),
                                  exec_sqlite_stmt_str_cleanup);
      if (ret != SQLITE_OK) {
        sqlite3_finalize(sqli3_stmt.value());
        return {PMA_SQL::ErrorT::FAILED_TO_PREPARE_EXEC_GENERIC,
                "Bind text failed"};
      }
      return exec_sqlite_statement<IDX + 1, Args...>(ctx, stmt, sqli3_stmt,
                                                     args...);
    } else {
      // TODO handle more than integers and strings
      int ret = sqlite3_bind_null(sqli3_stmt.value(), IDX);
      if (ret != SQLITE_OK) {
        sqlite3_finalize(sqli3_stmt.value());
        return {PMA_SQL::ErrorT::FAILED_TO_PREPARE_EXEC_GENERIC,
                "Bind NULL failed"};
      }
      return exec_sqlite_statement<IDX + 1, Args...>(ctx, stmt, sqli3_stmt,
                                                     args...);
    }
  } else {
    sqli3_stmt = nullptr;
    int ret = sqlite3_prepare_v2(ctx.get_sqlite_ctx(), stmt.c_str(),
                                 stmt.size(), &sqli3_stmt.value(), nullptr);
    if (ret != SQLITE_OK) {
      return {PMA_SQL::ErrorT::FAILED_TO_PREPARE_EXEC_GENERIC,
              "sqlite3_prepare_v2 failed"};
    }
    return exec_sqlite_statement<1, Arg, Args...>(ctx, stmt, sqli3_stmt, arg,
                                                  args...);
  }
}

template <typename... Args>
template <unsigned long long IDX>
void PMA_SQL::SqliteStmtRow<Args...>::print_row() const {
  if constexpr (IDX >= std::tuple_size_v<decltype(row)>) {
    std::println();
    return;
  } else {
    if (std::get<IDX>(row).has_value()) {
      std::print("Col {}: {}  ", IDX, std::get<IDX>(row).value());
    } else {
      std::print("Col {}: NULL  ", IDX);
    }
    print_row<IDX + 1>();
  }
}

template <typename... SArgs>
template <unsigned long long IDX>
std::tuple<PMA_SQL::ErrorT, std::string,
           std::optional<std::vector<PMA_SQL::SqliteStmtRow<SArgs...>>>>
PMA_SQL::SqliteStmtRow<SArgs...>::exec_sqlite_stmt_fetch_rows(
    const PMA_SQL::SQLITECtx &ctx, std::string stmt,
    std::optional<sqlite3_stmt *> sqli3_stmt,
    std::vector<PMA_SQL::SqliteStmtRow<SArgs...>> v) {
  int ret = sqlite3_step(sqli3_stmt.value());
  if (ret == SQLITE_DONE) {
    sqlite3_finalize(sqli3_stmt.value());
    return {ErrorT::SUCCESS, {}, v};
  } else if (ret == SQLITE_ROW) {
    v.push_back({});
    return exec_sqlite_stmt_fetch_rows<0, SArgs...>(ctx, stmt, sqli3_stmt,
                                                    std::move(v));
  } else {
    sqlite3_finalize(sqli3_stmt.value());
    return {ErrorT::EXEC_GENERIC_INVALID_STATE, "sqlite3_step failed",
            std::nullopt};
  }
}

template <typename... SArgs>
template <unsigned long long IDX, typename Arg, typename... Args>
std::tuple<PMA_SQL::ErrorT, std::string,
           std::optional<std::vector<PMA_SQL::SqliteStmtRow<SArgs...>>>>
PMA_SQL::SqliteStmtRow<SArgs...>::exec_sqlite_stmt_fetch_rows(
    const PMA_SQL::SQLITECtx &ctx, std::string stmt,
    std::optional<sqlite3_stmt *> sqli3_stmt,
    std::vector<PMA_SQL::SqliteStmtRow<SArgs...>> v) {
  if constexpr (std::is_integral_v<Arg>) {
    if (sizeof(Arg) > 4) {
      uint64_t val = sqlite3_column_int64(sqli3_stmt.value(), IDX);
      std::get<IDX>(v.back().row) = val;
    } else {
      int val = sqlite3_column_int(sqli3_stmt.value(), IDX);
      std::get<IDX>(v.back().row) = val;
    }
  } else if constexpr (std::is_same_v<Arg, std::string>) {
    std::string val = reinterpret_cast<const char *>(
        sqlite3_column_text(sqli3_stmt.value(), IDX));
    std::get<IDX>(v.back().row) = val;
  } else {
    sqlite3_finalize(sqli3_stmt.value());
    return {ErrorT::EXEC_GENERIC_INVALID_STATE, "Arg not int or std::string",
            std::nullopt};
  }
  return exec_sqlite_stmt_fetch_rows<IDX + 1, Args...>(ctx, stmt, sqli3_stmt,
                                                       std::move(v));
}

template <typename... SArgs>
template <unsigned long long IDX>
std::tuple<PMA_SQL::ErrorT, std::string,
           std::optional<std::vector<PMA_SQL::SqliteStmtRow<SArgs...>>>>
PMA_SQL::SqliteStmtRow<SArgs...>::exec_sqlite_stmt_with_rows(
    const PMA_SQL::SQLITECtx &ctx, std::string stmt,
    std::optional<sqlite3_stmt *> sqli3_stmt) {
  if (!sqli3_stmt.has_value()) {
    sqli3_stmt = nullptr;
    int ret = sqlite3_prepare_v2(ctx.get_sqlite_ctx(), stmt.c_str(),
                                 stmt.size(), &sqli3_stmt.value(), nullptr);
    if (ret != SQLITE_OK) {
      return {PMA_SQL::ErrorT::FAILED_TO_PREPARE_EXEC_GENERIC,
              "sqlite3_prepare_v2 failed", std::nullopt};
    }
  }

  int ret = sqlite3_step(sqli3_stmt.value());
  if (ret == SQLITE_DONE) {
    sqlite3_finalize(sqli3_stmt.value());
    return {ErrorT::SUCCESS, {}, std::nullopt};
  } else if (ret == SQLITE_ROW) {
    std::vector<SqliteStmtRow<SArgs...>> v;
    v.push_back({});
    return exec_sqlite_stmt_fetch_rows<0, SArgs...>(ctx, stmt, sqli3_stmt,
                                                    std::move(v));
  } else {
    sqlite3_finalize(sqli3_stmt.value());
    return {ErrorT::EXEC_GENERIC_INVALID_STATE, "sqlite3_step failed",
            std::nullopt};
  }
}

template <typename... SArgs>
template <unsigned long long IDX, typename Arg, typename... Args>
std::tuple<PMA_SQL::ErrorT, std::string,
           std::optional<std::vector<PMA_SQL::SqliteStmtRow<SArgs...>>>>
PMA_SQL::SqliteStmtRow<SArgs...>::exec_sqlite_stmt_with_rows(
    const PMA_SQL::SQLITECtx &ctx, std::string stmt,
    std::optional<sqlite3_stmt *> sqli3_stmt, Arg arg, Args... args) {
  if (sqli3_stmt.has_value()) {
    if constexpr (std::is_integral_v<Arg>) {
      if (sizeof(Arg) > 4) {
        int ret = sqlite3_bind_int64(sqli3_stmt.value(), IDX, arg);
        if (ret != SQLITE_OK) {
          sqlite3_finalize(sqli3_stmt.value());
          return {PMA_SQL::ErrorT::FAILED_TO_PREPARE_EXEC_GENERIC,
                  "Bind int64 failed", std::nullopt};
        }
        return exec_sqlite_stmt_with_rows<IDX + 1, Args...>(
            ctx, stmt, sqli3_stmt, args...);
      } else {
        int ret = sqlite3_bind_int(sqli3_stmt.value(), IDX, arg);
        if (ret != SQLITE_OK) {
          sqlite3_finalize(sqli3_stmt.value());
          return {PMA_SQL::ErrorT::FAILED_TO_PREPARE_EXEC_GENERIC,
                  "Bind int failed", std::nullopt};
        }
        return exec_sqlite_stmt_with_rows<IDX + 1, Args...>(
            ctx, stmt, sqli3_stmt, args...);
      }
    } else if constexpr (std::is_same_v<Arg, std::string>) {
      char *buf = new char[arg.size() + 1];
      std::memcpy(buf, arg.c_str(), arg.size() + 1);
      int ret = sqlite3_bind_text(sqli3_stmt.value(), IDX, buf, arg.size(),
                                  exec_sqlite_stmt_str_cleanup);
      if (ret != SQLITE_OK) {
        sqlite3_finalize(sqli3_stmt.value());
        return {PMA_SQL::ErrorT::FAILED_TO_PREPARE_EXEC_GENERIC,
                "Bind text failed", std::nullopt};
      }
      return exec_sqlite_stmt_with_rows<IDX + 1, Args...>(ctx, stmt, sqli3_stmt,
                                                          args...);
    } else {
      // TODO handle more than integers and strings
      int ret = sqlite3_bind_null(sqli3_stmt.value(), IDX);
      if (ret != SQLITE_OK) {
        sqlite3_finalize(sqli3_stmt.value());
        return {PMA_SQL::ErrorT::FAILED_TO_PREPARE_EXEC_GENERIC,
                "Bind NULL failed", std::nullopt};
      }
      return exec_sqlite_stmt_with_rows<IDX + 1, Args...>(ctx, stmt, sqli3_stmt,
                                                          args...);
    }
  } else {
    sqli3_stmt = nullptr;
    int ret = sqlite3_prepare_v2(ctx.get_sqlite_ctx(), stmt.c_str(),
                                 stmt.size(), &sqli3_stmt.value(), nullptr);
    if (ret != SQLITE_OK) {
      return {PMA_SQL::ErrorT::FAILED_TO_PREPARE_EXEC_GENERIC,
              "sqlite3_prepare_v2 failed", std::nullopt};
    }
    return exec_sqlite_stmt_with_rows<1, Arg, Args...>(ctx, stmt, sqli3_stmt,
                                                       arg, args...);
  }
}

#endif
