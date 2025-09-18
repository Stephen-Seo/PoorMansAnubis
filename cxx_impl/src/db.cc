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

#include <sqlite3.h>

std::tuple<void *, PMA_SQL::ErrorT, std::string> PMA_SQL::init_sqlite(
    std::string filepath) {
  sqlite3 *db = nullptr;

  int ret = sqlite3_open(filepath.c_str(), &db);

  if (ret || !db) {
    return {nullptr, ErrorT::FAILED_TO_OPEN_DB, {}};
  }

  // Initialize tables.
  // Buf apparently will point to data owned by sqlite3 instance.
  char *buf = nullptr;
  ret = sqlite3_exec(db,
                     "CREATE TABLE IF NOT EXISTS SEQ_ID (ID INTEGER PRIMARY "
                     "KEY AUTOINCREMENT)",
                     nullptr, nullptr, &buf);
  if (ret) {
    std::string err_msg("No Error Message");
    if (buf) {
      err_msg = std::string(buf);
    }
    sqlite3_free(buf);
    sqlite3_close(db);
    return {db, ErrorT::FAILED_TO_INIT_DB, std::move(err_msg)};
  }

  return {db, ErrorT::SUCCESS, {}};
}
