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
#include <print>

int main(int argc, char **argv) {
  // Test init sqlite3.
  const auto [ctx, error, cxx_string] = PMA_SQL::init_sqlite("./sqlite_db");

  {
    const auto [cleanup_error, error_str] = PMA_SQL::cleanup_stale_entries(ctx);
    if (cleanup_error != PMA_SQL::ErrorT::SUCCESS) {
      std::println(stderr, "Cleanup Stale Entries ERROR: {}", error_str);
      return 1;
    }
  }
  {
    const auto [cleanup_error, error_str] =
        PMA_SQL::cleanup_stale_challenges(ctx);
    if (cleanup_error != PMA_SQL::ErrorT::SUCCESS) {
      std::println(stderr, "Cleanup Stale Challenges ERROR: {}", error_str);
      return 1;
    }
  }

  {
    const auto [error, challenge_str, answer_str] =
        PMA_SQL::generate_challenge(ctx, 1000, 10000);
    if (error == PMA_SQL::ErrorT::SUCCESS) {
      std::println("Challenge str: {}", challenge_str);
      std::println("Answer str: {}", answer_str);
    } else {
      std::println(stderr, "ERROR: {}", challenge_str);
      return 1;
    }
  }

  return 0;
}
