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

#include <cstdint>
#include <mutex>
#include <string>
#include <tuple>

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
  FAILED_TO_STEP_STMT_CHALLENGE_FACTORS
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

// string is err message.
std::tuple<SQLITECtx, ErrorT, std::string> init_sqlite(std::string filepath);

// string is err message.
std::tuple<ErrorT, std::string> cleanup_stale_challenges(const SQLITECtx &ctx);

// string is err message.
std::tuple<ErrorT, std::string> cleanup_stale_entries(const SQLITECtx &ctx);

// On error, first string is err message. On SUCCESS, first string is challenge
// in base64 and second string is hashed answer.
std::tuple<ErrorT, std::string, std::string> generate_challenge(SQLITECtx &ctx,
                                                                uint64_t digits,
                                                                uint16_t port);

}  // namespace PMA_SQL

template <typename SqliteT>
SqliteT *PMA_SQL::SQLITECtx::get_sqlite_ctx() const {
  return reinterpret_cast<SqliteT *>(ctx);
}

#endif
