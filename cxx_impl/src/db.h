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
#include <string>
#include <tuple>
#include <type_traits>

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
  FAILED_TO_UPDATE_SEQ_ID,
  FAILED_TO_INSERT_TO_SEQ_ID,
  FAILED_TO_BIND_TO_SEQ_ID,
  FAILED_TO_STEP_STMT_SEQ_ID,
  ERROR_ON_FINALIZE_SEQ_ID,
  FAILED_TO_CHECK_CHALLENGE_FACTORS_ID,
  FAILED_INSERT_CHALLENGE_FACTORS,
  FAILED_TO_BIND_TO_CHALLENGE_FACTORS,
  FAILED_TO_STEP_STMT_CHALLENGE_FACTORS,
  FAILED_TO_PREPARE_SEL_FROM_CHALLENGE,
  FAILED_TO_BIND_FROM_CHALLENGE_FACTORS,
  FAILED_TO_PREPARE_EXEC_GENERIC,
  EXEC_GENERIC_INVALID_STATE
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

  template <typename SqliteT>
  SqliteT *get_sqlite_ctx() const;

  std::mutex &get_mutex();

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

// string is err message.
std::tuple<SQLITECtx, ErrorT, std::string> init_sqlite(std::string filepath);

// string is err message.
std::tuple<ErrorT, std::string> cleanup_stale_challenges(const SQLITECtx &ctx);

// string is err message.
std::tuple<ErrorT, std::string> cleanup_stale_entries(const SQLITECtx &ctx);

// On error, first string is err message. On SUCCESS, first string is challenge
// in base64 and second string is hashed answer.
// uint64_t is id.
std::tuple<ErrorT, std::string, std::string, uint64_t> generate_challenge(
    SQLITECtx &ctx, uint64_t digits, uint16_t port);

// string is error msg, uint16_t is destination port of initial challenge
// generation request.
std::tuple<ErrorT, std::string, uint16_t> verify_answer(SQLITECtx &ctx,
                                                        std::string answer,
                                                        uint64_t id);

}  // namespace PMA_SQL

////////////////////////////////////////////////////////////////////////////////
// templated implementations
////////////////////////////////////////////////////////////////////////////////

template <typename SqliteT>
SqliteT *PMA_SQL::SQLITECtx::get_sqlite_ctx() const {
  return reinterpret_cast<SqliteT *>(ctx);
}

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
          return {PMA_SQL::ErrorT::FAILED_TO_PREPARE_EXEC_GENERIC,
                  "Bind int64 failed"};
        }
        return exec_sqlite_statement<IDX + 1, Args...>(ctx, stmt, sqli3_stmt,
                                                       args...);
      } else {
        int ret = sqlite3_bind_int(sqli3_stmt.value(), IDX, arg);
        if (ret != SQLITE_OK) {
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
        return {PMA_SQL::ErrorT::FAILED_TO_PREPARE_EXEC_GENERIC,
                "Bind text failed"};
      }
      return exec_sqlite_statement<IDX + 1, Args...>(ctx, stmt, sqli3_stmt,
                                                     args...);
    } else {
      // TODO handle more than integers and strings
      int ret = sqlite3_bind_null(sqli3_stmt.value(), IDX);
      if (ret != SQLITE_OK) {
        return {PMA_SQL::ErrorT::FAILED_TO_PREPARE_EXEC_GENERIC,
                "Bind NULL failed"};
      }
      return exec_sqlite_statement<IDX + 1, Args...>(ctx, stmt, sqli3_stmt,
                                                     args...);
    }
  } else {
    sqli3_stmt = nullptr;
    int ret = sqlite3_prepare(ctx.get_sqlite_ctx<sqlite3>(), stmt.c_str(),
                              stmt.size(), &sqli3_stmt.value(), nullptr);
    if (ret != SQLITE_OK) {
      return {PMA_SQL::ErrorT::FAILED_TO_PREPARE_EXEC_GENERIC,
              "sqlite3_prepare failed"};
    }
    return exec_sqlite_statement<1, Arg, Args...>(ctx, stmt, sqli3_stmt, arg,
                                                  args...);
  }
}

#endif
