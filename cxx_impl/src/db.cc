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
#include <cstring>
#include <format>
#include <optional>
#include <print>
#include <random>

// Local includes.
#include "helpers.h"
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

uint64_t internal_next_id(uint64_t value) {
  constexpr uint64_t a = 9;
  constexpr uint64_t c = 31;

  std::default_random_engine default_re(value * a + c);

  return std::uniform_int_distribution<uint64_t>()(default_re);
}

std::tuple<std::optional<uint64_t>, PMA_SQL::ErrorT, std::string>
internal_increment_seq_id(const PMA_SQL::SQLITECtx &ctx) {
  std::optional<uint64_t> optv = std::nullopt;

  const auto temp_callback_get_seq_id = [](void *ud, int columns, char **strs,
                                           char **names) -> int {
    std::optional<uint64_t> *optv =
        reinterpret_cast<std::optional<uint64_t> *>(ud);
    if (columns == 1 && std::strcmp(names[0], "ID") == 0) {
      *optv = std::strtoull(strs[0], nullptr, 10);
    }
    return 0;
  };

  char *errmsg = nullptr;

  int ret = sqlite3_exec(ctx.get_sqlite_ctx<sqlite3>(), "SELECT ID FROM SEQ_ID",
                         temp_callback_get_seq_id, &optv, &errmsg);

  if (ret) {
    std::string errmsg_str(errmsg);
    sqlite3_free(errmsg);
    return {std::nullopt, PMA_SQL::ErrorT::FAILED_TO_FETCH_FROM_SEQ_ID,
            errmsg_str};
  } else if (optv.has_value()) {
    sqlite3_stmt *stmt = nullptr;
    ret = sqlite3_prepare_v2(ctx.get_sqlite_ctx<sqlite3>(),
                             "UPDATE SEQ_ID SET ID = ?", 24, &stmt, nullptr);
    if (ret) {
      // error
      sqlite3_finalize(stmt);
      return {std::nullopt, PMA_SQL::ErrorT::FAILED_TO_UPDATE_SEQ_ID,
              "Failed to prepare stmt to update SEQ_ID"};
    } else {
      GenericCleanup<sqlite3_stmt *> stmt_cleanup(
          stmt, [](sqlite3_stmt **ptr) { sqlite3_finalize(*ptr); });
      ret = sqlite3_bind_int64(stmt, 1, optv.value() + 1);
      if (ret) {
        // error
        return {std::nullopt, PMA_SQL::ErrorT::FAILED_TO_BIND_TO_SEQ_ID,
                "Failed to bind value to stmt"};
      } else {
        ret = sqlite3_step(stmt);
        if (ret == SQLITE_DONE) {
          // Intentionally left blank.
        } else {
          // error
          return {std::nullopt, PMA_SQL::ErrorT::FAILED_TO_STEP_STMT_SEQ_ID,
                  "Failed to step stmt"};
        }
      }
    }
  } else {
    // optv has no value.
    auto opt_tuple = internal_create_sqlite_statement(
        ctx, "INSERT INTO SEQ_ID (ID) VALUES (1)");
    if (opt_tuple.has_value()) {
      // error, failed to insert value into SEQ_ID.
      return {std::nullopt, PMA_SQL::ErrorT::FAILED_TO_INSERT_TO_SEQ_ID,
              "Failed to insert to SEQ_ID"};
    }
    optv = 1;
  }

  return {optv, PMA_SQL::ErrorT::SUCCESS, {}};
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
      "CREATE TABLE IF NOT EXISTS CHALLENGE_FACTORS (  ID INTEGER PRIMARY KEY, "
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

  {
    auto opt_tuple = internal_create_sqlite_statement(ctx, stmt);
    if (opt_tuple.has_value()) {
      const auto [unused, error_enum, msg] = std::move(opt_tuple.value());
      return {error_enum, msg};
    }
  }

  return {ErrorT::SUCCESS, {}};
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

  {
    auto opt_tuple = internal_create_sqlite_statement(ctx, stmt);
    if (opt_tuple.has_value()) {
      const auto [unused, error_enum, msg] = std::move(opt_tuple.value());
      return {error_enum, msg};
    }
  }

  return {ErrorT::SUCCESS, {}};
}

std::tuple<PMA_SQL::ErrorT, std::string, std::string>
PMA_SQL::generate_challenge(const SQLITECtx &ctx, uint64_t digits,
                            uint16_t port) {
  Work_Factors factors = work_generate_target_factors(digits);
  GenericCleanup<Work_Factors> factors_cleanup(
      factors, [](Work_Factors *ptr) { work_cleanup_factors(ptr); });

  char *challenge = work_factors_value_to_str2(factors, nullptr);
  std::string challenge_str = challenge;
  std::free(challenge);

  char *answer = work_factors_factors_to_str2(factors, nullptr);
  std::string answer_str = answer;
  std::free(answer);

  // TODO store hash of answer and port number in database.

  // TODO hash the answer.

  uint64_t unique_id = 0;
  bool exists_with_id = true;
  while (exists_with_id) {
    const auto [optv, error_type, error_msg] = internal_increment_seq_id(ctx);
    if (error_type != ErrorT::SUCCESS) {
      return {error_type, error_msg, {}};
    } else if (!optv.has_value()) {
      return {
          ErrorT::FAILED_TO_FETCH_FROM_SEQ_ID, "optv does not have value", {}};
    }

    uint64_t changed_id = internal_next_id(optv.value());

    sqlite3_stmt *stmt = nullptr;
    int ret = sqlite3_prepare_v2(
        ctx.get_sqlite_ctx<sqlite3>(),
        "SELECT ID FROM CHALLENGE_FACTORS WHERE ID = ?", 45, &stmt, nullptr);
    if (ret) {
      // error
      return {ErrorT::FAILED_TO_CHECK_CHALLENGE_FACTORS_ID,
              "Failed to prepare stmt",
              {}};
    } else {
      GenericCleanup<sqlite3_stmt *> stmt_cleanup(
          stmt, [](sqlite3_stmt **ptr) { sqlite3_finalize(*ptr); });
      ret = sqlite3_bind_int64(stmt, 1, changed_id);
      if (ret) {
        // error
        return {ErrorT::FAILED_TO_CHECK_CHALLENGE_FACTORS_ID,
                "Failed to bind to stmt INTEGER id",
                {}};
      } else {
        ret = sqlite3_step(stmt);
        if (ret == SQLITE_ROW) {
          // Has a row
          exists_with_id = true;
        } else if (ret == SQLITE_DONE) {
          // No rows
          exists_with_id = false;
          unique_id = optv.value();
        } else {
          // error
          return {ErrorT::FAILED_TO_CHECK_CHALLENGE_FACTORS_ID,
                  "stmt step not done and no row",
                  {}};
        }
      }
    }
  }

  // ret = sqlite3_prepare_v2(
  //     ctx.get_sqlite_ctx<sqlite3>(), "INSERT INTO CHALLENGE_FACTORS (UUID
  //);
  // std::string stmt = std::format

  return {ErrorT::SUCCESS, challenge_str, answer_str};
}
