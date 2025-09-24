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

// Local includes.
#include "db.h"

// Standard library includes.
#include <chrono>
#include <print>
#include <thread>

int main(int argc, char **argv) {
  // Test init sqlite3.
  auto [ctx, error, cxx_string] = PMA_SQL::init_sqlite("./sqlite_db");

  {
    const auto [cleanup_error, error_str] = PMA_SQL::cleanup_stale_entries(ctx);
    if (cleanup_error != PMA_SQL::ErrorT::SUCCESS) {
      std::println(stderr, "Cleanup Stale Entries ErrorT: {}, ERROR: {}",
                   PMA_SQL::error_t_to_string(cleanup_error), error_str);
      return 1;
    }
  }
  {
    const auto [cleanup_error, error_str] =
        PMA_SQL::cleanup_stale_challenges(ctx);
    if (cleanup_error != PMA_SQL::ErrorT::SUCCESS) {
      std::println(stderr, "Cleanup Stale Challenges ErrorT: {}, ERROR: {}",
                   PMA_SQL::error_t_to_string(cleanup_error), error_str);
      return 1;
    }
  }

  if (argc == 3) {
    const auto [err_enum, err_msg, opt_vec] =
        PMA_SQL::SqliteStmtRow<uint64_t, std::string, int,
                               std::string>::exec_sqlite_stmt_with_rows<0,
                                                                        int>(
            ctx, "SELECT ID, IP, PORT, ON_TIME FROM ALLOWED_IPS WHERE 1 == ?",
            std::nullopt, 1);
    if (opt_vec.has_value()) {
      for (auto row : opt_vec.value()) {
        row.print_row<0>();
      }
    }
  } else {
    const auto [error, challenge_str, answer_str, id] =
        PMA_SQL::generate_challenge(ctx, 1000, "127.0.0.1", 10000);
    if (error == PMA_SQL::ErrorT::SUCCESS) {
      std::println("Challenge str: {}", challenge_str);
      std::println("Answer str: {}", answer_str);
    } else {
      std::println(stderr, "ERROR: ErrorT: {}, message: {}",
                   PMA_SQL::error_t_to_string(error), challenge_str);
      return 1;
    }

    if (argc == 2) {
      std::this_thread::sleep_for(std::chrono::seconds(5));
      const auto [error_enum, err_str, port] =
          PMA_SQL::verify_answer(ctx, answer_str, "127.0.0.1", id);
      std::println(stderr, "Got error_enum {}, err_str {}, port {}",
                   PMA_SQL::error_t_to_string(error_enum), err_str, port);
    }
  }

  return 0;
}
