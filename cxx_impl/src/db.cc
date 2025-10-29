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
#include <blake3.h>
#include <sqlite3.h>

// Standard Library includes.
#include <cstdlib>
#include <cstring>
#include <format>
#include <optional>
#include <random>

// Local includes.
#include "helpers.h"
#include "work.h"

////////////////////////////////////////////////////////////////////////////////
// Internal Functions.
////////////////////////////////////////////////////////////////////////////////

std::optional<std::tuple<PMA_SQL::SQLITECtx, PMA_SQL::ErrorT, std::string>>
internal_exec_sqlite_statement(const PMA_SQL::SQLITECtx &ctx,
                               std::string stmt) {
  char *buf = nullptr;
  int ret =
      sqlite3_exec(ctx.get_sqlite_ctx(), stmt.c_str(), nullptr, nullptr, &buf);
  if (ret != SQLITE_OK) {
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

uint64_t internal_rand_id() {
  std::random_device rd{};
  static std::default_random_engine re(rd());
  static std::uniform_int_distribution<uint64_t> int_dist;
  return int_dist(re);
}

uint64_t internal_next_id(uint64_t value) {
  constexpr uint64_t a = 9;
  constexpr uint64_t c = 31;

  std::default_random_engine default_re(value * a + c);

  return std::uniform_int_distribution<uint64_t>()(default_re);
}

std::string internal_next_hash(uint64_t value) {
  uint64_t next_id = internal_next_id(value);
  uint64_t random_val = internal_rand_id();

  blake3_hasher hasher{0};
  blake3_hasher_init(&hasher);
  blake3_hasher_update(&hasher, &next_id, 8);
  blake3_hasher_update(&hasher, &random_val, 8);

  std::array<unsigned char, 32> hash;
  blake3_hasher_finalize(&hasher, reinterpret_cast<uint8_t *>(hash.data()),
                         hash.size());
  return PMA_HELPER::raw_to_hexadecimal<32>(hash);
}

std::tuple<std::optional<uint64_t>, PMA_SQL::ErrorT, std::string>
internal_increment_seq_id(const PMA_SQL::SQLITECtx &ctx) {
  std::optional<uint64_t> optv = std::nullopt;

  {
    const auto [err_enum, err_msg, opt_vec] =
        PMA_SQL::SqliteStmtRow<uint64_t>::exec_sqlite_stmt_with_rows<0>(
            ctx, "SELECT ID FROM SEQ_ID", std::nullopt);

    if (err_enum != PMA_SQL::ErrorT::SUCCESS) {
      return {std::nullopt, err_enum, err_msg};
    } else if (!opt_vec.has_value()) {
      // Intentionally left blank.
    } else {
      optv = std::get<0>(opt_vec.value().at(0).row).value();
    }
  }

  if (optv.has_value()) {
    const auto [err_enum, err_msg, opt_vec] =
        PMA_SQL::SqliteStmtRow<>::exec_sqlite_stmt_with_rows<0, uint64_t>(
            ctx, "UPDATE SEQ_ID SET ID = ?", std::nullopt, optv.value() + 1);

    if (err_enum != PMA_SQL::ErrorT::SUCCESS) {
      return {optv, err_enum, err_msg};
    }
  } else {
    const auto [err_enum, err_msg, opt_vec] =
        PMA_SQL::SqliteStmtRow<>::exec_sqlite_stmt_with_rows<0>(
            ctx, "INSERT INTO SEQ_ID (ID) VALUES (1)", std::nullopt);

    if (err_enum != PMA_SQL::ErrorT::SUCCESS) {
      return {optv, err_enum, err_msg};
    }

    optv = 1;
  }

  return {optv, PMA_SQL::ErrorT::SUCCESS, {}};
}

////////////////////////////////////////////////////////////////////////////////
// Non-Internal Functions.
////////////////////////////////////////////////////////////////////////////////

std::string PMA_SQL::error_t_to_string(PMA_SQL::ErrorT err) {
  switch (err) {
    case PMA_SQL::ErrorT::SUCCESS:
      return "Success";
    case PMA_SQL::ErrorT::FAILED_TO_OPEN_DB:
      return "FailedToOpenDB";
    case PMA_SQL::ErrorT::FAILED_TO_INIT_DB:
      return "FailedToInitDB";
    case PMA_SQL::ErrorT::DB_ALREADY_FAILED_TO_INIT:
      return "DBFailedToInitAlready";
    case PMA_SQL::ErrorT::FAILED_TO_FETCH_FROM_SEQ_ID:
      return "FailedToFetchFromSEQ_ID";
    case PMA_SQL::ErrorT::FAILED_TO_PREPARE_EXEC_GENERIC:
      return "FailedToPrepareStmtGenericExec";
    case PMA_SQL::ErrorT::EXEC_GENERIC_INVALID_STATE:
      return "ExecGenericInvalidState";
    case PMA_SQL::ErrorT::CLIENT_IP_DOES_NOT_MATCH_STORED_IP:
      return "ClientIPDoesNotMatchStoredIP";
    case PMA_SQL::ErrorT::FAILED_TO_FETCH_FROM_ALLOWED_IPS:
      return "FailedToFetchFromAllowedIPs";
    case PMA_SQL::ErrorT::FAILED_TO_FETCH_FROM_ID_TO_PORT:
      return "FailedToFetchFromIDToPort";
    default:
      return "Unknown error";
  }
}

PMA_SQL::SQLITECtx::SQLITECtx() : mutex(), ctx(nullptr) {}

PMA_SQL::SQLITECtx::SQLITECtx(std::string sqlite_path) : mutex(), ctx(nullptr) {
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

sqlite3 *PMA_SQL::SQLITECtx::get_sqlite_ctx() const {
  return reinterpret_cast<sqlite3 *>(ctx);
}

std::mutex &PMA_SQL::SQLITECtx::get_mutex() { return mutex; }

std::lock_guard<std::mutex> PMA_SQL::SQLITECtx::get_mutex_lock_guard() {
  return std::lock_guard<std::mutex>(mutex);
}

void PMA_SQL::exec_sqlite_stmt_str_cleanup(void *ud) {
  char *buf = reinterpret_cast<char *>(ud);
  delete[] buf;
}

std::tuple<PMA_SQL::SQLITECtx, PMA_SQL::ErrorT, std::string>
PMA_SQL::init_sqlite(std::string filepath) {
  SQLITECtx ctx(filepath);

  if (ctx.get_ctx() == nullptr) {
    return std::make_tuple<SQLITECtx, ErrorT, std::string>(
        {}, ErrorT::FAILED_TO_OPEN_DB, {});
  }

  auto sql_ret = internal_exec_sqlite_statement(
      ctx,
      "CREATE TABLE IF NOT EXISTS SEQ_ID (ID INTEGER NOT NULL PRIMARY KEY "
      "AUTOINCREMENT)");
  if (sql_ret.has_value()) {
    return std::move(sql_ret.value());
  }

  sql_ret = internal_exec_sqlite_statement(
      ctx,
      "CREATE TABLE IF NOT EXISTS ID_TO_PORT (ID TEXT NOT NULL PRIMARY KEY, "
      "PORT INT UNSIGNED NOT NULL, ON_TIME TEXT NOT NULL DEFAULT ( datetime() "
      ") )");

  if (sql_ret.has_value()) {
    return std::move(sql_ret.value());
  }

  sql_ret = internal_exec_sqlite_statement(
      ctx,
      "CREATE INDEX IF NOT EXISTS ID_TO_PORT_TIME ON ID_TO_PORT (ON_TIME)");
  if (sql_ret.has_value()) {
    return std::move(sql_ret.value());
  }

  sql_ret = internal_exec_sqlite_statement(
      ctx,
      "CREATE TABLE IF NOT EXISTS CHALLENGE_FACTOR ( ID TEXT NOT NULL PRIMARY "
      "KEY, FACTORS TEXT NOT NULL, IP TEXT NOT NULL, PORT INT NOT NULL, "
      "ON_TIME TEXT DEFAULT ( datetime() ) )");
  if (sql_ret.has_value()) {
    return std::move(sql_ret.value());
  }

  sql_ret = internal_exec_sqlite_statement(
      ctx,
      "CREATE INDEX IF NOT EXISTS CHALLENGE_FACTOR_TIME ON CHALLENGE_FACTOR "
      "(ON_TIME)");
  if (sql_ret.has_value()) {
    return std::move(sql_ret.value());
  }

  // TODO Verify if this is needed
  // sql_ret = internal_exec_sqlite_statement(
  //     ctx,
  //     "CREATE INDEX IF NOT EXISTS CHALLENGE_F_P ON CHALLENGE_FACTOR
  //     (FACTORS, " "PORT)");
  // if (sql_ret.has_value()) {
  //   return std::move(sql_ret.value());
  // }

  sql_ret = internal_exec_sqlite_statement(
      ctx,
      "CREATE TABLE IF NOT EXISTS ALLOWED_IP ( ID INTEGER PRIMARY KEY "
      "AUTOINCREMENT, IP TEXT NOT NULL, PORT INTEGER NOT NULL, ON_TIME TEXT "
      "NOT NULL DEFAULT ( datetime() ) )");
  if (sql_ret.has_value()) {
    return std::move(sql_ret.value());
  }

  sql_ret = internal_exec_sqlite_statement(
      ctx, "CREATE INDEX IF NOT EXISTS ALLOWED_IP_IP ON ALLOWED_IP (IP)");
  if (sql_ret.has_value()) {
    return std::move(sql_ret.value());
  }

  sql_ret = internal_exec_sqlite_statement(
      ctx,
      "CREATE INDEX IF NOT EXISTS ALLOWED_IP_TIME ON ALLOWED_IP (ON_TIME)");
  if (sql_ret.has_value()) {
    return std::move(sql_ret.value());
  }

  return std::make_tuple<SQLITECtx, ErrorT, std::string>(std::move(ctx),
                                                         ErrorT::SUCCESS, {});
}

std::tuple<PMA_SQL::ErrorT, std::string> PMA_SQL::cleanup_stale_id_to_ports(
    const PMA_SQL::SQLITECtx &ctx, uint32_t challenge_timeout) {
  sqlite3 *db = ctx.get_sqlite_ctx();
  if (!db) {
    return {ErrorT::DB_ALREADY_FAILED_TO_INIT, {}};
  }

  std::string stmt = std::format(
      "DELETE FROM ID_TO_PORT WHERE datetime(ON_TIME, '{} minutes') < "
      "datetime('now')",
      challenge_timeout);

  {
    auto opt_tuple = internal_exec_sqlite_statement(ctx, stmt);
    if (opt_tuple.has_value()) {
      const auto [unused, err_enum, err_msg] = std::move(opt_tuple.value());
      return {err_enum, err_msg};
    }
  }

  return {ErrorT::SUCCESS, {}};
}

std::tuple<PMA_SQL::ErrorT, std::string> PMA_SQL::cleanup_stale_challenges(
    const PMA_SQL::SQLITECtx &ctx, uint32_t challenge_timeout) {
  sqlite3 *db = ctx.get_sqlite_ctx();
  if (!db) {
    return {ErrorT::DB_ALREADY_FAILED_TO_INIT, {}};
  }

  std::string stmt = std::format(
      "DELETE FROM CHALLENGE_FACTOR WHERE datetime(ON_TIME, '{} minutes') < "
      "datetime('now')",
      challenge_timeout);

  {
    auto opt_tuple = internal_exec_sqlite_statement(ctx, stmt);
    if (opt_tuple.has_value()) {
      const auto [unused, error_enum, msg] = std::move(opt_tuple.value());
      return {error_enum, msg};
    }
  }

  return {ErrorT::SUCCESS, {}};
}

std::tuple<PMA_SQL::ErrorT, std::string> PMA_SQL::cleanup_stale_entries(
    const PMA_SQL::SQLITECtx &ctx, uint32_t allowed_timeout) {
  sqlite3 *db = ctx.get_sqlite_ctx();
  if (!db) {
    return {ErrorT::DB_ALREADY_FAILED_TO_INIT, {}};
  }

  std::string stmt = std::format(
      "DELETE FROM ALLOWED_IP WHERE datetime(ON_TIME, '{} minutes') < "
      "datetime('now')",
      allowed_timeout);

  {
    auto opt_tuple = internal_exec_sqlite_statement(ctx, stmt);
    if (opt_tuple.has_value()) {
      const auto [unused, error_enum, msg] = std::move(opt_tuple.value());
      return {error_enum, msg};
    }
  }

  return {ErrorT::SUCCESS, {}};
}

std::tuple<PMA_SQL::ErrorT, std::string, std::string> PMA_SQL::init_id_to_port(
    SQLITECtx &ctx, uint16_t port) {
  uint64_t unique_id = 0;
  bool exists_with_id = true;
  while (exists_with_id) {
    {
      const auto [optv, err_type, err_msg] = internal_increment_seq_id(ctx);
      if (err_type != ErrorT::SUCCESS) {
        return {err_type, err_msg, {}};
      } else if (!optv.has_value()) {
        return {ErrorT::FAILED_TO_FETCH_FROM_SEQ_ID,
                "SEQ_ID optv does not have value",
                {}};
      }

      unique_id = internal_next_id(optv.value());
    }

    const auto [err_type, err_msg, opt_vec] =
        SqliteStmtRow<uint64_t>::exec_sqlite_stmt_with_rows<0, uint64_t>(
            ctx, "SELECT ID FROM ID_TO_PORT WHERE ID = ?", std::nullopt,
            unique_id);
    if (err_type != ErrorT::SUCCESS) {
      return {err_type, err_msg, {}};
    } else if (opt_vec.has_value() && !opt_vec.value().empty()) {
      exists_with_id = true;
    } else {
      exists_with_id = false;
    }
  }

  std::string id_hashed = internal_next_hash(unique_id);

  const auto [err_enum, err_msg] = exec_sqlite_statement<0>(
      ctx, "INSERT INTO ID_TO_PORT (ID, PORT) VALUES (?, ?)", std::nullopt,
      id_hashed, port);

  if (err_enum != ErrorT::SUCCESS) {
    return {err_enum, err_msg, {}};
  }

  return {ErrorT::SUCCESS, {}, id_hashed};
}

std::tuple<PMA_SQL::ErrorT, std::string, std::string, std::string>
PMA_SQL::generate_challenge(SQLITECtx &ctx, uint64_t digits,
                            std::string client_ip, std::string hashed_id) {
  uint16_t port = 0;
  {
    const auto [err_enum, err_msg, opt_vec] =
        SqliteStmtRow<uint16_t>::exec_sqlite_stmt_with_rows<0>(
            ctx, "SELECT PORT FROM ID_TO_PORT WHERE ID = ?", std::nullopt,
            hashed_id);
    if (err_enum != ErrorT::SUCCESS) {
      return {err_enum, err_msg, {}, {}};
    } else if (!opt_vec.has_value() || opt_vec.value().empty()) {
      return {
          ErrorT::FAILED_TO_FETCH_FROM_ID_TO_PORT, "ID does not exist", {}, {}};
    } else if (!std::get<0>(opt_vec.value().at(0).row).has_value()) {
      return {ErrorT::FAILED_TO_FETCH_FROM_ID_TO_PORT,
              "Invalid row fetching via ID",
              {},
              {}};
    }

    port = std::get<0>(opt_vec.value().at(0).row).value();
    exec_sqlite_statement<0>(ctx, "DELETE FROM ID_TO_PORT WHERE ID = ?",
                             std::nullopt, hashed_id);
  }

  Work_Factors factors = work_generate_target_factors(digits);
  GenericCleanup<Work_Factors> factors_cleanup(
      factors, [](Work_Factors *ptr) { work_cleanup_factors(ptr); });

  char *challenge = work_factors_value_to_str2(factors, nullptr);
  std::string challenge_str = challenge;
  std::free(challenge);

  char *answer = work_factors_factors_to_str2(factors, nullptr);
  std::string answer_str = answer;
  std::free(answer);

  // Acquire a mutex lock_guard.
  auto lock = ctx.get_mutex_lock_guard();

  // Get a unique identifier for CHALLENGE_FACTOR.
  std::string hash_id;
  bool exists_with_id = true;
  while (exists_with_id) {
    const auto [optv, error_type, error_msg] = internal_increment_seq_id(ctx);
    if (error_type != ErrorT::SUCCESS) {
      return {error_type, error_msg, {}, {}};
    } else if (!optv.has_value()) {
      return {ErrorT::FAILED_TO_FETCH_FROM_SEQ_ID,
              "SEQ_ID optv does not have value",
              {},
              {}};
    }

    hash_id = internal_next_hash(optv.value());

    const auto [err_enum, err_msg, opt_vec] =
        SqliteStmtRow<std::string>::exec_sqlite_stmt_with_rows<0, std::string>(
            ctx, "SELECT ID FROM CHALLENGE_FACTOR WHERE ID = ?", std::nullopt,
            hash_id);

    if (err_enum != ErrorT::SUCCESS) {
      return {err_enum, err_msg, {}, {}};
    } else if (opt_vec.has_value() && !opt_vec.value().empty()) {
      exists_with_id = true;
    } else {
      exists_with_id = false;
    }
  }

  // hash the answer prior to storing
  std::string hash;
  {
    std::array<uint8_t, BLAKE3_OUT_LEN> hash_data;
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);

    blake3_hasher_update(&hasher, answer_str.c_str(), answer_str.size());

    blake3_hasher_finalize(&hasher, hash_data.data(), BLAKE3_OUT_LEN);

    hash = PMA_HELPER::raw_to_hexadecimal<BLAKE3_OUT_LEN>(hash_data);
  }

  // Insert challenge into db.
  {
    const auto [err_enum, err_msg, opt_vec] =
        SqliteStmtRow<>::exec_sqlite_stmt_with_rows<0, std::string, std::string,
                                                    std::string, int>(
            ctx,
            "INSERT INTO CHALLENGE_FACTOR (ID, FACTORS, IP, PORT) VALUES (?, "
            "?, ?, ?)",
            std::nullopt, hash_id, hash, client_ip, port);

    if (err_enum != ErrorT::SUCCESS) {
      return {err_enum, err_msg, {}, hash_id};
    }
  }

  return {ErrorT::SUCCESS, challenge_str, answer_str, hash_id};
}

std::tuple<PMA_SQL::ErrorT, std::string, uint16_t> PMA_SQL::verify_answer(
    SQLITECtx &ctx, std::string answer, std::string ipaddr, std::string id) {
  auto lock = ctx.get_mutex_lock_guard();

  std::string hash;
  // get hash
  {
    std::array<uint8_t, BLAKE3_OUT_LEN> hash_data;
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);

    blake3_hasher_update(&hasher, answer.c_str(), answer.size());

    blake3_hasher_finalize(&hasher, hash_data.data(), BLAKE3_OUT_LEN);

    hash = PMA_HELPER::raw_to_hexadecimal<BLAKE3_OUT_LEN>(hash_data);
  }

  std::string stored_ip;
  uint16_t port = 0;
  {
    const auto [err_enum, err_msg, opt_vec] = SqliteStmtRow<
        std::string, int>::exec_sqlite_stmt_with_rows<0, std::string,
                                                      std::string>(
        ctx,
        "SELECT IP, PORT FROM CHALLENGE_FACTOR WHERE ID = ? AND FACTORS = ?",
        std::nullopt, id, hash);

    if (err_enum != ErrorT::SUCCESS) {
      return {err_enum, err_msg, 0};
    } else if (!opt_vec.has_value() || opt_vec.value().empty()) {
      return {ErrorT::EXEC_GENERIC_INVALID_STATE,
              "Failed to get IP, PORT from CHALLENGE_FACTOR", 0};
    }

    stored_ip = std::get<0>(opt_vec.value().at(0).row).value();
    port =
        static_cast<uint16_t>(std::get<1>(opt_vec.value().at(0).row).value());
  }

  if (stored_ip != ipaddr) {
    return {ErrorT::CLIENT_IP_DOES_NOT_MATCH_STORED_IP,
            "client ip address mismatch", 0};
  }

  {
    const auto [err_enum, err_msg] = exec_sqlite_statement<0, std::string>(
        ctx, "DELETE FROM CHALLENGE_FACTOR WHERE ID = ?", std::nullopt, id);
    if (err_enum != ErrorT::SUCCESS) {
      return {err_enum, err_msg, 0};
    }
  }

  {
    const auto [err_enum, err_msg] = exec_sqlite_statement<0>(
        ctx, "INSERT INTO ALLOWED_IP (IP, PORT) VALUES (?, ?)", std::nullopt,
        ipaddr, port);

    if (err_enum != ErrorT::SUCCESS) {
      return {err_enum, err_msg, 0};
    }
  }

  return {ErrorT::SUCCESS, {}, port};
}

std::tuple<PMA_SQL::ErrorT, std::string, std::unordered_set<uint16_t>>
PMA_SQL::get_allowed_ip_ports(SQLITECtx &ctx, std::string ipaddr) {
  auto lock = ctx.get_mutex_lock_guard();

  const auto [err_enum, err_msg, opt_vec] =
      SqliteStmtRow<int>::exec_sqlite_stmt_with_rows<0, std::string>(
          ctx, "SELECT PORT FROM ALLOWED_IP WHERE IP = ?", std::nullopt,
          ipaddr);

  if (err_enum != ErrorT::SUCCESS) {
    return {err_enum, err_msg, {}};
  } else if (!opt_vec.has_value() || opt_vec.value().empty()) {
    return {ErrorT::FAILED_TO_FETCH_FROM_ALLOWED_IPS,
            "opt_vec is nullopt or empty",
            {}};
  }

  std::unordered_set<uint16_t> ret;

  for (uint64_t idx = 0; idx < opt_vec.value().size(); ++idx) {
    if (auto opt_val = std::get<0>(opt_vec.value().at(idx).row);
        opt_val.has_value()) {
      ret.insert(static_cast<uint16_t>(opt_val.value()));
    }
  }

  return {ErrorT::SUCCESS, {}, ret};
}

std::tuple<PMA_SQL::ErrorT, std::string, bool> PMA_SQL::is_allowed_ip_port(
    SQLITECtx &ctx, std::string ipaddr, uint16_t port) {
  auto lock = ctx.get_mutex_lock_guard();

  const auto [err_enum, err_msg, opt_vec] =
      SqliteStmtRow<int>::exec_sqlite_stmt_with_rows<0, std::string, int>(
          ctx, "SELECT PORT FROM ALLOWED_IP WHERE IP = ? AND PORT = ?",
          std::nullopt, ipaddr, port);

  if (err_enum != ErrorT::SUCCESS) {
    return {err_enum, err_msg, {}};
  } else if (!opt_vec.has_value() || opt_vec.value().empty()) {
    return {ErrorT::FAILED_TO_FETCH_FROM_ALLOWED_IPS,
            "opt_vec is nullopt or empty",
            {}};
  }

  for (size_t idx = 0; idx < opt_vec.value().size(); ++idx) {
    if (auto opt_val = std::get<0>(opt_vec.value().at(idx).row);
        opt_val.has_value() && opt_val.value() == port) {
      return {ErrorT::SUCCESS, {}, true};
    }
  }

  return {ErrorT::SUCCESS, {}, false};
}
