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

#include "db.h"

// Third party includes.
#include <sqlite3.h>

// Standard Library includes.
#include <cstdlib>
#include <format>
#include <optional>

// Local includes.
#include "work.h"

// Internal Functions.
std::optional<std::tuple<PMA_SQL::SQLITECtx, PMA_SQL::ErrorT, std::string> >
internal_create_sqlite_statement(const PMA_SQL::SQLITECtx &ctx,
                                 std::string stmt) {
  char *buf = nullptr;
  int ret = sqlite3_exec(ctx.get_sqlite_ctx<sqlite3>(), stmt.c_str(), nullptr,
                         nullptr, &buf);
  if (ret) {
    std::string err_msg("No Error Message");
    if (buf) {
      err_msg = std::string(buf);
      sqlite3_free(buf);
    }
    return std::make_tuple<PMA_SQL::SQLITECtx, PMA_SQL::ErrorT, std::string>(
        {}, PMA_SQL::ErrorT::FAILED_TO_INIT_DB, std::move(err_msg));
  } else {
    if (buf) {
      sqlite3_free(buf);
    }
  }

  return std::nullopt;
}

// Non-Internal Functions.

PMA_SQL::SQLITECtx::SQLITECtx() : ctx(nullptr) {}

PMA_SQL::SQLITECtx::SQLITECtx(std::string sqlite_path) : ctx(nullptr) {
  sqlite3 *db = nullptr;
  int ret = sqlite3_open(sqlite_path.c_str(), &db);

  if (db && !ret) {
    ctx = reinterpret_cast<void *>(db);
  }
}

PMA_SQL::SQLITECtx::~SQLITECtx() {
  if (ctx) {
    sqlite3 *db = reinterpret_cast<sqlite3 *>(ctx);
    sqlite3_close(db);
  }
}

PMA_SQL::SQLITECtx::SQLITECtx(SQLITECtx &&other) {
  this->ctx = other.ctx;
  other.ctx = nullptr;
}

PMA_SQL::SQLITECtx *PMA_SQL::SQLITECtx::operator=(SQLITECtx &&other) {
  this->~SQLITECtx();
  this->ctx = other.ctx;
  other.ctx = nullptr;

  return this;
}

void *PMA_SQL::SQLITECtx::get_ctx() const { return ctx; }

std::tuple<PMA_SQL::SQLITECtx, PMA_SQL::ErrorT, std::string>
PMA_SQL::init_sqlite(std::string filepath) {
  SQLITECtx ctx(filepath);

  if (ctx.get_ctx() == nullptr) {
    return std::make_tuple<SQLITECtx, ErrorT, std::string>(
        {}, ErrorT::FAILED_TO_OPEN_DB, {});
  }

  auto sql_ret =
      internal_create_sqlite_statement(ctx,
                                       "CREATE TABLE IF NOT EXISTS SEQ_ID (ID "
                                       "INTEGER PRIMARY KEY AUTOINCREMENT)");
  if (sql_ret.has_value()) {
    return std::move(sql_ret.value());
  }

  sql_ret = internal_create_sqlite_statement(
      ctx,
      "CREATE TABLE IF NOT EXISTS CHALLENGE_FACTORS (  UUID TEXT PRIMARY KEY,  "
      "FACTORS TEXT NOT NULL,  PORT INT NOT NULL,  GEN_TIME TEXT DEFAULT ( "
      "datetime() ) )");
  if (sql_ret.has_value()) {
    return std::move(sql_ret.value());
  }

  sql_ret = internal_create_sqlite_statement(
      ctx,
      "CREATE INDEX IF NOT EXISTS CHALLENGE_F_P ON CHALLENGE_FACTORS (FACTORS, "
      "PORT)");
  if (sql_ret.has_value()) {
    return std::move(sql_ret.value());
  }

  sql_ret = internal_create_sqlite_statement(
      ctx,
      "CREATE TABLE IF NOT EXISTS ALLOWED_IPS (  IP TEXT PRIMARY KEY,  ON_TIME "
      "TEXT NOT NULL DEFAULT ( datetime() ) )");
  if (sql_ret.has_value()) {
    return std::move(sql_ret.value());
  }

  sql_ret = internal_create_sqlite_statement(
      ctx,
      "CREATE INDEX IF NOT EXISTS ALLOWED_IPS_TIME ON ALLOWED_IPS (ON_TIME)");
  if (sql_ret.has_value()) {
    return std::move(sql_ret.value());
  }

  return std::make_tuple<SQLITECtx, ErrorT, std::string>(std::move(ctx),
                                                         ErrorT::SUCCESS, {});
}

std::tuple<PMA_SQL::ErrorT, std::string> PMA_SQL::cleanup_stale_challenges(
    const PMA_SQL::SQLITECtx &ctx) {
  sqlite3 *db = ctx.get_sqlite_ctx<sqlite3>();
  if (!db) {
    return {ErrorT::DB_ALREADY_FAILED_TO_INIT, {}};
  }

  std::string stmt = std::format(
      "DELETE FROM CHALLENGE_FACTORS WHERE timediff(datetime(), GEN_TIME) > "
      "'+0000-00-00 00:{:02}:00.000'",
      CHALLENGE_TIMEOUT_MINUTES);
  char *err_msg = nullptr;
  std::string ret_err_msg;
  int ret = sqlite3_exec(db, stmt.c_str(), nullptr, nullptr, &err_msg);
  if (ret) {
    if (err_msg) {
      ret_err_msg = err_msg;
      sqlite3_free(err_msg);
    }
  }

  return {ErrorT::SUCCESS, ret_err_msg};
}

std::tuple<PMA_SQL::ErrorT, std::string> PMA_SQL::cleanup_stale_entries(
    const PMA_SQL::SQLITECtx &ctx) {
  sqlite3 *db = ctx.get_sqlite_ctx<sqlite3>();
  if (!db) {
    return {ErrorT::DB_ALREADY_FAILED_TO_INIT, {}};
  }

  std::string stmt = std::format(
      "DELETE FROM ALLOWED_IPS WHERE timediff(datetime(), ON_TIME) > "
      "'+0000-00-00 00:{:02}:00.000'",
      ALLOWED_IP_TIMEOUT_MINUTES);
  char *err_msg = nullptr;
  std::string ret_err_msg;
  int ret = sqlite3_exec(db, stmt.c_str(), nullptr, nullptr, &err_msg);
  if (ret) {
    if (err_msg) {
      ret_err_msg = err_msg;
      sqlite3_free(err_msg);
    }
  }

  return {ErrorT::SUCCESS, ret_err_msg};
}

std::tuple<PMA_SQL::ErrorT, std::string, std::string>
PMA_SQL::generate_challenge(const SQLITECtx &ctx, uint64_t digits,
                            uint16_t port) {
  Work_Factors factors = work_generate_target_factors(digits);

  char *challenge = work_factors_value_to_str2(factors, nullptr);
  std::string challenge_str = challenge;
  std::free(challenge);

  char *answer = work_factors_factors_to_str2(factors, nullptr);
  std::string answer_str = answer;
  std::free(answer);

  // TODO store hash of answer and port number in database.

  work_cleanup_factors(&factors);
  return {ErrorT::SUCCESS, challenge_str, answer_str};
}
